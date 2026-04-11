// Copyright (c) Temporal Rewind System. All rights reserved.

#include "Subsystem/TemporalRewindSubsystem.h"
#include "TemporalRewindModule.h"
#include "Timeline/ActorSnapshotTimeline.h"
#include "Recording/RecordingStrategy.h"
#include "Session/RewindSessionContext.h"
#include "Interfaces/Rewindable.h"
#include "Settings/TemporalRewindSettings.h"
#include "Engine/World.h"

// --- State machine transition table ----------------------------------------

namespace TemporalRewind::StateMachine
{
	struct FTransition
	{
		ETemporalSystemState From;
		ETemporalSystemState To;
	};

	static const FTransition LegalTransitions[] =
	{
		{ ETemporalSystemState::Idle,       ETemporalSystemState::Recording },
		{ ETemporalSystemState::Recording,  ETemporalSystemState::Idle      },
		{ ETemporalSystemState::Recording,  ETemporalSystemState::Rewinding },
		{ ETemporalSystemState::Rewinding,  ETemporalSystemState::Paused    },
		{ ETemporalSystemState::Rewinding,  ETemporalSystemState::Scrubbing },
		{ ETemporalSystemState::Rewinding,  ETemporalSystemState::Cooldown  },
		{ ETemporalSystemState::Paused,     ETemporalSystemState::Rewinding },
		{ ETemporalSystemState::Paused,     ETemporalSystemState::Scrubbing },
		{ ETemporalSystemState::Paused,     ETemporalSystemState::Cooldown  },
		{ ETemporalSystemState::Scrubbing,  ETemporalSystemState::Paused    },
		{ ETemporalSystemState::Scrubbing,  ETemporalSystemState::Rewinding },
		{ ETemporalSystemState::Scrubbing,  ETemporalSystemState::Cooldown  },
		{ ETemporalSystemState::Cooldown,   ETemporalSystemState::Recording },
	};

	static bool IsLegalTransition(ETemporalSystemState From, ETemporalSystemState To)
	{
		for (const FTransition& T : LegalTransitions)
		{
			if (T.From == From && T.To == To)
			{
				return true;
			}
		}
		return false;
	}

	static const TCHAR* StateToString(ETemporalSystemState State)
	{
		switch (State)
		{
		case ETemporalSystemState::Idle:       return TEXT("Idle");
		case ETemporalSystemState::Recording:  return TEXT("Recording");
		case ETemporalSystemState::Rewinding:  return TEXT("Rewinding");
		case ETemporalSystemState::Paused:     return TEXT("Paused");
		case ETemporalSystemState::Scrubbing:  return TEXT("Scrubbing");
		case ETemporalSystemState::Cooldown:   return TEXT("Cooldown");
		default:                               return TEXT("Unknown");
		}
	}
}

// --- Lifecycle --------------------------------------------------------------

void UTemporalRewindSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	ActiveRecordingStrategy = NewObject<URecordingStrategy>(this);

	// Apply project-wide defaults from Edit → Project Settings → Plugins → Temporal Rewind.
	// These are the starting values; runtime calls (SetMaxScrubRange, SetRecordingStrategy, etc.)
	// override them independently after startup.
	if (const UTemporalRewindSettings* Settings = GetDefault<UTemporalRewindSettings>())
	{
		BufferDuration      = Settings->BufferDuration;
		MaxScrubRange       = FMath::Min(Settings->MaxScrubRange, Settings->BufferDuration);
		CooldownDuration    = Settings->CooldownDuration;
		RewindSpeed         = Settings->RewindSpeed;
		DefaultOrphanPolicy = Settings->DefaultOrphanPolicy;
		ActiveRecordingStrategy->SetRecordingInterval(Settings->GetEffectiveRecordingInterval());
	}

	CurrentState = ETemporalSystemState::Idle;
	bTickEnabled = true;

	UE_LOG(LogTemporalRewind, Display,
		TEXT("[TemporalRewindSubsystem] Initialized. BufferDuration=%.2fs MaxScrubRange=%.2fs RecordingInterval=%.4fs"),
		BufferDuration, MaxScrubRange,
		ActiveRecordingStrategy->GetRecordingInterval());
}

void UTemporalRewindSubsystem::Deinitialize()
{
	RegisteredActors.Empty();
	ActorTimelines.Empty();
	OrphanedTimelines.Empty();
	ActiveSessionContext = nullptr;
	ActiveRecordingStrategy = nullptr;
	bTickEnabled = false;

	UE_LOG(LogTemporalRewind, Display,
		TEXT("[TemporalRewindSubsystem] Deinitialized."));

	Super::Deinitialize();
}

TStatId UTemporalRewindSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UTemporalRewindSubsystem, STATGROUP_Tickables);
}

// --- Tick dispatch ----------------------------------------------------------

void UTemporalRewindSubsystem::Tick(float DeltaTime)
{
	switch (CurrentState)
	{
	case ETemporalSystemState::Idle:                                break;
	case ETemporalSystemState::Recording:  TickRecording(DeltaTime); break;
	case ETemporalSystemState::Rewinding:  TickRewinding(DeltaTime); break;
	case ETemporalSystemState::Scrubbing:  TickScrubbing(DeltaTime); break;
	case ETemporalSystemState::Paused:     TickPaused(DeltaTime);    break;
	case ETemporalSystemState::Cooldown:   TickCooldown(DeltaTime);  break;
	}
}

void UTemporalRewindSubsystem::TickRecording(float DeltaTime)
{
	// Sweep dead actors first so we don't try to poll a stale ref this frame.
	CleanupStaleActors();

	if (!ActiveRecordingStrategy || !ActiveRecordingStrategy->ShouldRecordThisTick(DeltaTime))
	{
		return;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const float NowTimestamp = World->GetTimeSeconds();

	// Iterate the registry. Per-actor hot path: dirty check is O(1),
	// conditional capture is O(blob-size), push is O(1).
	for (TPair<FGuid, FRewindableActorEntry>& Pair : RegisteredActors)
	{
		FRewindableActorEntry& Entry = Pair.Value;

		UObject* Obj = Entry.WeakRef.Get();
		if (!Obj)
		{
			continue;
		}

		if (!IRewindable::Execute_HasStateChanged(Obj))
		{
			continue;
		}

		FTemporalSnapshot Snapshot = IRewindable::Execute_CaptureState(Obj);
		Snapshot.Timestamp = NowTimestamp;
		Snapshot.OwnerGuid = Entry.ActorGuid;

		if (UActorSnapshotTimeline* Timeline = ActorTimelines.FindRef(Entry.ActorGuid))
		{
			Timeline->PushSnapshot(Snapshot);
		}
	}
}

void UTemporalRewindSubsystem::TickRewinding(float DeltaTime)
{
	if (!ActiveSessionContext)
	{
		return;
	}

	ActiveSessionContext->AdvanceScrub(DeltaTime);
	ApplyStateAtScrubTimestamp();

	OnScrubUpdated.Broadcast(ActiveSessionContext->GetCurrentScrubTimestamp());
}

void UTemporalRewindSubsystem::TickScrubbing(float DeltaTime)
{
	// Doc line 458: Scrubbing does NOT advance the scrub timestamp per tick.
	// Scrub position changes only when ScrubTo() is called externally. Tick
	// is intentionally a no-op to keep the state reactive to transitions.
}

void UTemporalRewindSubsystem::TickPaused(float DeltaTime)
{
	// Doc line 459: Paused performs no actor iteration and no apply.
}

void UTemporalRewindSubsystem::TickCooldown(float DeltaTime)
{
	CooldownTimeRemaining -= DeltaTime;

	if (CooldownTimeRemaining <= 0.0f)
	{
		CooldownTimeRemaining = 0.0f;
		OnCooldownEnded.Broadcast();
		TryTransitionTo(ETemporalSystemState::Recording);

		if (ActiveRecordingStrategy)
		{
			ActiveRecordingStrategy->Reset();
		}
	}
}
// --- State machine ----------------------------------------------------------

bool UTemporalRewindSubsystem::TryTransitionTo(ETemporalSystemState NewState)
{
	if (NewState == CurrentState)
	{
		UE_LOG(LogTemporalRewind, Verbose,
			TEXT("[TemporalRewindSubsystem] TryTransitionTo(%s): already in this state."),
			TemporalRewind::StateMachine::StateToString(NewState));
		return false;
	}

	if (!TemporalRewind::StateMachine::IsLegalTransition(CurrentState, NewState))
	{
		UE_LOG(LogTemporalRewind, Warning,
			TEXT("[TemporalRewindSubsystem] Illegal transition: %s -> %s. Rejected."),
			TemporalRewind::StateMachine::StateToString(CurrentState),
			TemporalRewind::StateMachine::StateToString(NewState));
		return false;
	}

	const ETemporalSystemState OldState = CurrentState;
	CurrentState = NewState;

	UE_LOG(LogTemporalRewind, Display,
		TEXT("[TemporalRewindSubsystem] State: %s -> %s"),
		TemporalRewind::StateMachine::StateToString(OldState),
		TemporalRewind::StateMachine::StateToString(NewState));

	return true;
}

// --- State queries ----------------------------------------------------------

bool UTemporalRewindSubsystem::IsRewindActive() const
{
	return CurrentState == ETemporalSystemState::Rewinding
		|| CurrentState == ETemporalSystemState::Paused
		|| CurrentState == ETemporalSystemState::Scrubbing;
}

// --- Recording control ------------------------------------------------------

void UTemporalRewindSubsystem::StartRecording()
{
	if (TryTransitionTo(ETemporalSystemState::Recording))
	{
		if (ActiveRecordingStrategy)
		{
			ActiveRecordingStrategy->Reset();
		}
	}
}

void UTemporalRewindSubsystem::StopRecording()
{
	// Doc line 393: buffer data retained on stop.
	TryTransitionTo(ETemporalSystemState::Idle);
}

// --- Registration -----------------------------------------------------------

void UTemporalRewindSubsystem::RegisterRewindable(TScriptInterface<IRewindable> Rewindable)
{
	UObject* Obj = Rewindable.GetObject();
	if (!Obj)
	{
		UE_LOG(LogTemporalRewind, Warning,
			TEXT("[TemporalRewindSubsystem] RegisterRewindable called with null object. Ignored."));
		return;
	}

	// Read the actor's identity and policy once, cache forever (doc line 498).
	const FGuid ActorGuid = IRewindable::Execute_GetRewindableId(Obj);
	if (!ActorGuid.IsValid())
	{
		UE_LOG(LogTemporalRewind, Warning,
			TEXT("[TemporalRewindSubsystem] RegisterRewindable: actor %s returned an invalid GUID. Ignored."),
			*Obj->GetName());
		return;
	}

	if (RegisteredActors.Contains(ActorGuid))
	{
		UE_LOG(LogTemporalRewind, Verbose,
			TEXT("[TemporalRewindSubsystem] RegisterRewindable: actor %s already registered. No-op."),
			*ActorGuid.ToString());
		return;
	}

	FRewindableActorEntry Entry;
	Entry.ActorGuid = ActorGuid;
	Entry.OrphanPolicy = IRewindable::Execute_GetOrphanedTimelinePolicy(Obj);
	Entry.WeakRef = TWeakObjectPtr<UObject>(Obj);
	RegisteredActors.Add(ActorGuid, Entry);

	// One timeline per actor, pre-sized from current buffer/interval.
	UActorSnapshotTimeline* Timeline = NewObject<UActorSnapshotTimeline>(this);
	Timeline->Initialize(ActorGuid, ComputeTimelineCapacity());
	ActorTimelines.Add(ActorGuid, Timeline);

	UE_LOG(LogTemporalRewind, Display,
		TEXT("[TemporalRewindSubsystem] Registered actor %s."), *ActorGuid.ToString());

	OnActorRegistered.Broadcast(ActorGuid);
}

void UTemporalRewindSubsystem::UnregisterRewindable(TScriptInterface<IRewindable> Rewindable)
{
	UObject* Obj = Rewindable.GetObject();
	if (!Obj)
	{
		UE_LOG(LogTemporalRewind, Warning,
			TEXT("[TemporalRewindSubsystem] UnregisterRewindable called with null object. Ignored."));
		return;
	}

	const FGuid ActorGuid = IRewindable::Execute_GetRewindableId(Obj);
	const FRewindableActorEntry* Entry = RegisteredActors.Find(ActorGuid);
	if (!Entry)
	{
		UE_LOG(LogTemporalRewind, Verbose,
			TEXT("[TemporalRewindSubsystem] UnregisterRewindable: %s not found. No-op."),
			*ActorGuid.ToString());
		return;
	}

	// Explicit unregister is a clean exit � apply the same orphan policy as
	// the stale-ref cleanup path, for consistency with actors that die without
	// calling unregister.
	const EOrphanedTimelinePolicy Policy = Entry->OrphanPolicy;
	RegisteredActors.Remove(ActorGuid);

	if (TObjectPtr<UActorSnapshotTimeline>* TimelinePtr = ActorTimelines.Find(ActorGuid))
	{
		UActorSnapshotTimeline* Timeline = *TimelinePtr;
		ActorTimelines.Remove(ActorGuid);

		switch (Policy)
		{
		case EOrphanedTimelinePolicy::PurgeImmediately:
			// Drop the reference; GC will collect.
			break;

		case EOrphanedTimelinePolicy::KeepUntilOverwritten:
		case EOrphanedTimelinePolicy::KeepIndefinitely:
			OrphanedTimelines.Add(ActorGuid, Timeline);
			break;
		}
	}

	UE_LOG(LogTemporalRewind, Display,
		TEXT("[TemporalRewindSubsystem] Unregistered actor %s."), *ActorGuid.ToString());

	OnActorUnregistered.Broadcast(ActorGuid);
}

// --- Stale-ref sweep --------------------------------------------------------

void UTemporalRewindSubsystem::CleanupStaleActors()
{
	// Collect first, mutate after. Cannot remove from a TMap mid-iteration.
	TArray<FGuid, TInlineAllocator<8>> StaleGuids;

	for (const TPair<FGuid, FRewindableActorEntry>& Pair : RegisteredActors)
	{
		if (!Pair.Value.WeakRef.IsValid())
		{
			StaleGuids.Add(Pair.Key);
		}
	}

	for (const FGuid& StaleGuid : StaleGuids)
	{
		const FRewindableActorEntry* Entry = RegisteredActors.Find(StaleGuid);
		if (!Entry)
		{
			continue;
		}

		const EOrphanedTimelinePolicy Policy = Entry->OrphanPolicy;

		UE_LOG(LogTemporalRewind, Warning,
			TEXT("[TemporalRewindSubsystem] Stale weak ref detected for actor %s during recording tick. Applying orphan policy."),
			*StaleGuid.ToString());

		RegisteredActors.Remove(StaleGuid);

		if (TObjectPtr<UActorSnapshotTimeline>* TimelinePtr = ActorTimelines.Find(StaleGuid))
		{
			UActorSnapshotTimeline* Timeline = *TimelinePtr;
			ActorTimelines.Remove(StaleGuid);

			switch (Policy)
			{
			case EOrphanedTimelinePolicy::PurgeImmediately:
				break;

			case EOrphanedTimelinePolicy::KeepUntilOverwritten:
			case EOrphanedTimelinePolicy::KeepIndefinitely:
				OrphanedTimelines.Add(StaleGuid, Timeline);
				break;
			}
		}

		OnActorUnregistered.Broadcast(StaleGuid);
	}
}

// --- Orphan management ------------------------------------------------------

void UTemporalRewindSubsystem::PurgeOrphanedTimeline(FGuid ActorGuid)
{
	if (OrphanedTimelines.Remove(ActorGuid) > 0)
	{
		UE_LOG(LogTemporalRewind, Display,
			TEXT("[TemporalRewindSubsystem] Purged orphaned timeline for %s."),
			*ActorGuid.ToString());
	}
}

void UTemporalRewindSubsystem::PurgeAllOrphanedTimelines()
{
	const int32 Count = OrphanedTimelines.Num();
	OrphanedTimelines.Empty();

	if (Count > 0)
	{
		UE_LOG(LogTemporalRewind, Display,
			TEXT("[TemporalRewindSubsystem] Purged %d orphaned timelines."), Count);
	}
}

TArray<FGuid> UTemporalRewindSubsystem::GetOrphanedTimelineGuids() const
{
	TArray<FGuid> Result;
	OrphanedTimelines.GetKeys(Result);
	return Result;
}

// --- Capacity ---------------------------------------------------------------

int32 UTemporalRewindSubsystem::ComputeTimelineCapacity() const
{
	// Capacity = BufferDuration / RecordingInterval, with a +1 margin so that
	// a snapshot pushed at exactly the boundary can never cause a one-frame
	// overflow before the ring wraps.
	const float Interval = (ActiveRecordingStrategy)
		? ActiveRecordingStrategy->GetRecordingInterval()
		: 0.1f;

	const int32 RawCapacity = FMath::CeilToInt(BufferDuration / FMath::Max(Interval, KINDA_SMALL_NUMBER));
	return FMath::Max(RawCapacity + 1, 2);
}

// --- Rewind stubs (8c) ------------------------------------------------------

void UTemporalRewindSubsystem::StartRewind()
{
	// Doc Q3(a): reject if there is nothing to rewind to.
	bool bAnyBufferHasData = false;
	for (const TPair<FGuid, TObjectPtr<UActorSnapshotTimeline>>& Pair : ActorTimelines)
	{
		if (Pair.Value && !Pair.Value->IsEmpty())
		{
			bAnyBufferHasData = true;
			break;
		}
	}

	if (!bAnyBufferHasData)
	{
		UE_LOG(LogTemporalRewind, Warning,
			TEXT("[TemporalRewindSubsystem] StartRewind rejected: no recorded snapshots in any timeline."));
		return;
	}

	if (!TryTransitionTo(ETemporalSystemState::Rewinding))
	{
		return;
	}

	const UWorld* World = GetWorld();
	const float NowTimestamp = World ? World->GetTimeSeconds() : 0.0f;

	ActiveSessionContext = NewObject<URewindSessionContext>(this);
	// Doc Q4(a): capture RewindSpeed at session start; ignore later property changes.
	ActiveSessionContext->Initialize(NowTimestamp, MaxScrubRange, RewindSpeed);

	UE_LOG(LogTemporalRewind, Display,
		TEXT("[TemporalRewindSubsystem] Rewind session started at T=%.3f"), NowTimestamp);

	OnRewindStarted.Broadcast();
}

void UTemporalRewindSubsystem::PauseRewind()
{
	if (!TryTransitionTo(ETemporalSystemState::Paused))
	{
		return;
	}
	OnRewindPaused.Broadcast();
}

void UTemporalRewindSubsystem::ResumeRewind()
{
	if (!TryTransitionTo(ETemporalSystemState::Rewinding))
	{
		return;
	}
	OnRewindResumed.Broadcast();
}

void UTemporalRewindSubsystem::ScrubTo(float Timestamp)
{
	if (!IsRewindActive() || !ActiveSessionContext)
	{
		UE_LOG(LogTemporalRewind, Warning,
			TEXT("[TemporalRewindSubsystem] ScrubTo called while no rewind session active. Rejected."));
		return;
	}

	// ScrubTo transitions Rewinding/Paused into Scrubbing. If already Scrubbing, stay.
	if (CurrentState != ETemporalSystemState::Scrubbing)
	{
		TryTransitionTo(ETemporalSystemState::Scrubbing);
	}

	ActiveSessionContext->SetScrubTimestamp(Timestamp);
	ApplyStateAtScrubTimestamp();

	OnScrubUpdated.Broadcast(ActiveSessionContext->GetCurrentScrubTimestamp());
}

void UTemporalRewindSubsystem::Commit()
{
	if (!IsRewindActive() || !ActiveSessionContext)
	{
		UE_LOG(LogTemporalRewind, Warning,
			TEXT("[TemporalRewindSubsystem] Commit called while no rewind session active. Rejected."));
		return;
	}

	const float CommitTimestamp = ActiveSessionContext->GetCurrentScrubTimestamp();

	// Doc Q1(b): truncate both live AND orphaned timelines for consistency.
	for (TPair<FGuid, TObjectPtr<UActorSnapshotTimeline>>& Pair : ActorTimelines)
	{
		if (Pair.Value)
		{
			Pair.Value->TruncateAfter(CommitTimestamp);
		}
	}
	for (TPair<FGuid, TObjectPtr<UActorSnapshotTimeline>>& Pair : OrphanedTimelines)
	{
		if (Pair.Value)
		{
			Pair.Value->TruncateAfter(CommitTimestamp);
		}
	}

	UE_LOG(LogTemporalRewind, Display,
		TEXT("[TemporalRewindSubsystem] Rewind committed at T=%.3f"), CommitTimestamp);

	OnRewindCommitted.Broadcast();

	ActiveSessionContext = nullptr;
	BeginCooldown();
}

void UTemporalRewindSubsystem::Cancel()
{
	if (!IsRewindActive() || !ActiveSessionContext)
	{
		UE_LOG(LogTemporalRewind, Warning,
			TEXT("[TemporalRewindSubsystem] Cancel called while no rewind session active. Rejected."));
		return;
	}

	// Snap scrub back to the original present and apply one last time so actors
	// land exactly where they were when rewind began.
	ActiveSessionContext->SetScrubTimestamp(ActiveSessionContext->GetOriginalPresentTimestamp());
	ApplyStateAtScrubTimestamp();

	UE_LOG(LogTemporalRewind, Display,
		TEXT("[TemporalRewindSubsystem] Rewind cancelled; snapped back to present."));

	OnRewindCancelled.Broadcast();

	ActiveSessionContext = nullptr;
	BeginCooldown();
}

float UTemporalRewindSubsystem::GetScrubTimestamp() const
{
	return ActiveSessionContext ? ActiveSessionContext->GetCurrentScrubTimestamp() : 0.0f;
}

// --- Config -----------------------------------------------------------------

void UTemporalRewindSubsystem::SetMaxScrubRange(float NewRange)
{
	MaxScrubRange = FMath::Clamp(NewRange, 0.0f, BufferDuration);
}

void UTemporalRewindSubsystem::SetRecordingStrategy(URecordingStrategy* NewStrategy)
{
	if (NewStrategy == nullptr)
	{
		UE_LOG(LogTemporalRewind, Warning,
			TEXT("[TemporalRewindSubsystem] SetRecordingStrategy(nullptr) rejected."));
		return;
	}
	ActiveRecordingStrategy = NewStrategy;
}

void UTemporalRewindSubsystem::ApplyStateAtScrubTimestamp()
{
	if (!ActiveSessionContext)
	{
		return;
	}

	const float ScrubTime = ActiveSessionContext->GetCurrentScrubTimestamp();

	// Live actors: fetch floor snapshot, dispatch ApplyState via reflection.
	for (const TPair<FGuid, FRewindableActorEntry>& Pair : RegisteredActors)
	{
		const FRewindableActorEntry& Entry = Pair.Value;
		UObject* Obj = Entry.WeakRef.Get();
		if (!Obj)
		{
			continue;
		}

		const UActorSnapshotTimeline* Timeline = ActorTimelines.FindRef(Entry.ActorGuid);
		if (!Timeline)
		{
			continue;
		}

		FTemporalSnapshot Snapshot;
		if (Timeline->GetSnapshotAtTime(ScrubTime, Snapshot))
		{
			IRewindable::Execute_ApplyState(Obj, Snapshot);
		}
	}

	// Orphan actors: doc Q2(b), edge-triggered � only fire when scrub ENTERS a
	// timestamp range where this orphan has data. We approximate "enter" by
	// comparing against the previous scrub position tracked on the session.
	// For v1, we fire on every tick the scrub position lies over the orphan's
	// valid range; a proper edge-trigger requires a per-orphan "last fired at"
	// set which is deferred to Phase 3 stress testing (doc line 1173).
	//
	// NOTE: This is a knowing compromise. A consumer binding OnOrphanedSnapshotReached
	// should debounce on their side until Phase 3 lands the proper edge-trigger.
	for (const TPair<FGuid, TObjectPtr<UActorSnapshotTimeline>>& Pair : OrphanedTimelines)
	{
		const UActorSnapshotTimeline* Timeline = Pair.Value;
		if (!Timeline || Timeline->IsEmpty())
		{
			continue;
		}

		float OldestTs = 0.0f, NewestTs = 0.0f;
		Timeline->GetOldestTimestamp(OldestTs);
		Timeline->GetNewestTimestamp(NewestTs);

		if (ScrubTime < OldestTs || ScrubTime > NewestTs)
		{
			continue;
		}

		FTemporalSnapshot Snapshot;
		if (Timeline->GetSnapshotAtTime(ScrubTime, Snapshot))
		{
			OnOrphanedSnapshotReached.Broadcast(Pair.Key, Snapshot);
		}
	}
}

void UTemporalRewindSubsystem::BeginCooldown()
{
	if (!TryTransitionTo(ETemporalSystemState::Cooldown))
	{
		return;
	}

	CooldownTimeRemaining = CooldownDuration;
	OnCooldownStarted.Broadcast(CooldownDuration);

	UE_LOG(LogTemporalRewind, Display,
		TEXT("[TemporalRewindSubsystem] Cooldown started for %.2fs."), CooldownDuration);
}

#if WITH_EDITOR

void UTemporalRewindSubsystem::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (MaxScrubRange > BufferDuration)
	{
		MaxScrubRange = BufferDuration;
	}
}

#endif