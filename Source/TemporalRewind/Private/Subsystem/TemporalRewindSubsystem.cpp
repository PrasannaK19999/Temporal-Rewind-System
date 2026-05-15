// Copyright (c) Temporal Rewind System. All rights reserved.

#include "Subsystem/TemporalRewindSubsystem.h"

#include "Engine/World.h"
#include "Interfaces/Rewindable.h"
#include "Recording/RecordingStrategy.h"
#include "Session/RewindSessionContext.h"
#include "Settings/TemporalRewindSettings.h"
#include "TemporalRewindModule.h"
#include "Timeline/ActorSnapshotTimeline.h"

// ---------------------------------------------------------------------------
// State machine
// ---------------------------------------------------------------------------

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
		{ ETemporalSystemState::Rewinding,  ETemporalSystemState::Cooldown  },
		{ ETemporalSystemState::Rewinding,  ETemporalSystemState::Paused    },
		{ ETemporalSystemState::Rewinding,  ETemporalSystemState::Scrubbing },
		{ ETemporalSystemState::Paused,     ETemporalSystemState::Cooldown  },
		{ ETemporalSystemState::Paused,     ETemporalSystemState::Rewinding },
		{ ETemporalSystemState::Paused,     ETemporalSystemState::Scrubbing },
		{ ETemporalSystemState::Scrubbing,  ETemporalSystemState::Cooldown  },
		{ ETemporalSystemState::Scrubbing,  ETemporalSystemState::Paused    },
		{ ETemporalSystemState::Scrubbing,  ETemporalSystemState::Rewinding },
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

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void UTemporalRewindSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	ActiveRecordingStrategy = NewObject<URecordingStrategy>(this);

	if (const UTemporalRewindSettings* Settings = GetDefault<UTemporalRewindSettings>())
	{
		BufferDuration = Settings->BufferDuration;
		MaxScrubRange = FMath::Min(Settings->MaxScrubRange, Settings->BufferDuration);
		CooldownDuration = Settings->CooldownDuration;
		RewindSpeed = Settings->RewindSpeed;
		DefaultOrphanPolicy = Settings->DefaultOrphanPolicy;
		ActiveRecordingStrategy->SetRecordingInterval(Settings->GetEffectiveRecordingInterval());
	}

	CurrentState = ETemporalSystemState::Idle;
	bTickEnabled = true;

	UE_LOG(LogTemporalRewind, Display,
		TEXT("[TemporalRewindSubsystem] Initialized. BufferDuration=%.2fs MaxScrubRange=%.2fs RecordingInterval=%.4fs"),
		BufferDuration, MaxScrubRange, ActiveRecordingStrategy->GetRecordingInterval());
}

void UTemporalRewindSubsystem::Deinitialize()
{
	ActiveRecordingStrategy = nullptr;
	ActiveSessionContext = nullptr;
	ActorTimelines.Empty();
	OrphanedTimelines.Empty();
	RegisteredActors.Empty();
	bTickEnabled = false;

	UE_LOG(LogTemporalRewind, Display, TEXT("[TemporalRewindSubsystem] Deinitialized."));

	Super::Deinitialize();
}

TStatId UTemporalRewindSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UTemporalRewindSubsystem, STATGROUP_Tickables);
}

// ---------------------------------------------------------------------------
// Tick
// ---------------------------------------------------------------------------

void UTemporalRewindSubsystem::Tick(float DeltaTime)
{
	switch (CurrentState)
	{
	case ETemporalSystemState::Idle:                                 break;
	case ETemporalSystemState::Recording:  TickRecording(DeltaTime); break;
	case ETemporalSystemState::Rewinding:  TickRewinding(DeltaTime); break;
	case ETemporalSystemState::Paused:     TickPaused(DeltaTime);    break;
	case ETemporalSystemState::Scrubbing:  TickScrubbing(DeltaTime); break;
	case ETemporalSystemState::Cooldown:   TickCooldown(DeltaTime);  break;
	}
}

void UTemporalRewindSubsystem::TickRecording(float DeltaTime)
{
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

void UTemporalRewindSubsystem::TickPaused(float /*DeltaTime*/)
{
}

void UTemporalRewindSubsystem::TickScrubbing(float /*DeltaTime*/)
{
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

// ---------------------------------------------------------------------------
// State machine
// ---------------------------------------------------------------------------

bool UTemporalRewindSubsystem::TryTransitionTo(ETemporalSystemState NewState)
{
	if (NewState == CurrentState)
	{
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

	UE_LOG(LogTemporalRewind, Display,
		TEXT("[TemporalRewindSubsystem] State: %s -> %s"),
		TemporalRewind::StateMachine::StateToString(CurrentState),
		TemporalRewind::StateMachine::StateToString(NewState));

	CurrentState = NewState;
	return true;
}

// ---------------------------------------------------------------------------
// State queries
// ---------------------------------------------------------------------------

bool UTemporalRewindSubsystem::IsRewindActive() const
{
	return CurrentState == ETemporalSystemState::Rewinding
		|| CurrentState == ETemporalSystemState::Paused
		|| CurrentState == ETemporalSystemState::Scrubbing;
}

// ---------------------------------------------------------------------------
// Recording
// ---------------------------------------------------------------------------

void UTemporalRewindSubsystem::StartRecording()
{
	if (TryTransitionTo(ETemporalSystemState::Recording) && ActiveRecordingStrategy)
	{
		ActiveRecordingStrategy->Reset();
	}
}

void UTemporalRewindSubsystem::StopRecording()
{
	TryTransitionTo(ETemporalSystemState::Idle);
}

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------

void UTemporalRewindSubsystem::RegisterRewindable(TScriptInterface<IRewindable> Rewindable)
{
	UObject* Obj = Rewindable.GetObject();
	if (!Obj)
	{
		UE_LOG(LogTemporalRewind, Warning,
			TEXT("[TemporalRewindSubsystem] RegisterRewindable called with null object."));
		return;
	}

	const FGuid ActorGuid = IRewindable::Execute_GetRewindableId(Obj);
	if (!ActorGuid.IsValid())
	{
		UE_LOG(LogTemporalRewind, Warning,
			TEXT("[TemporalRewindSubsystem] RegisterRewindable: %s returned invalid GUID."),
			*Obj->GetName());
		return;
	}

	if (RegisteredActors.Contains(ActorGuid))
	{
		return;
	}

	FRewindableActorEntry Entry;
	Entry.ActorGuid = ActorGuid;
	Entry.OrphanPolicy = IRewindable::Execute_GetOrphanedTimelinePolicy(Obj);
	Entry.WeakRef = TWeakObjectPtr<UObject>(Obj);
	RegisteredActors.Add(ActorGuid, Entry);

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
		return;
	}

	const FGuid ActorGuid = IRewindable::Execute_GetRewindableId(Obj);
	const FRewindableActorEntry* Entry = RegisteredActors.Find(ActorGuid);
	if (!Entry)
	{
		return;
	}

	const EOrphanedTimelinePolicy Policy = Entry->OrphanPolicy;
	RegisteredActors.Remove(ActorGuid);

	if (TObjectPtr<UActorSnapshotTimeline>* TimelinePtr = ActorTimelines.Find(ActorGuid))
	{
		UActorSnapshotTimeline* Timeline = *TimelinePtr;
		ActorTimelines.Remove(ActorGuid);

		if (Policy != EOrphanedTimelinePolicy::PurgeImmediately)
		{
			OrphanedTimelines.Add(ActorGuid, Timeline);
		}
	}

	UE_LOG(LogTemporalRewind, Display,
		TEXT("[TemporalRewindSubsystem] Unregistered actor %s."), *ActorGuid.ToString());

	OnActorUnregistered.Broadcast(ActorGuid);
}

// ---------------------------------------------------------------------------
// Stale-ref sweep
// ---------------------------------------------------------------------------

void UTemporalRewindSubsystem::CleanupStaleActors()
{
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
			TEXT("[TemporalRewindSubsystem] Stale ref detected for %s. Applying orphan policy."),
			*StaleGuid.ToString());

		RegisteredActors.Remove(StaleGuid);

		if (TObjectPtr<UActorSnapshotTimeline>* TimelinePtr = ActorTimelines.Find(StaleGuid))
		{
			UActorSnapshotTimeline* Timeline = *TimelinePtr;
			ActorTimelines.Remove(StaleGuid);

			if (Policy != EOrphanedTimelinePolicy::PurgeImmediately)
			{
				OrphanedTimelines.Add(StaleGuid, Timeline);
			}
		}

		OnActorUnregistered.Broadcast(StaleGuid);
	}
}

// ---------------------------------------------------------------------------
// Orphan management
// ---------------------------------------------------------------------------

TArray<FGuid> UTemporalRewindSubsystem::GetOrphanedTimelineGuids() const
{
	TArray<FGuid> Result;
	OrphanedTimelines.GetKeys(Result);
	return Result;
}

void UTemporalRewindSubsystem::PurgeAllOrphanedTimelines()
{
	const int32 Num = OrphanedTimelines.Num();
	OrphanedTimelines.Empty();

	if (Num > 0)
	{
		UE_LOG(LogTemporalRewind, Display,
			TEXT("[TemporalRewindSubsystem] Purged %d orphaned timelines."), Num);
	}
}

void UTemporalRewindSubsystem::PurgeOrphanedTimeline(FGuid ActorGuid)
{
	if (OrphanedTimelines.Remove(ActorGuid) > 0)
	{
		UE_LOG(LogTemporalRewind, Display,
			TEXT("[TemporalRewindSubsystem] Purged orphaned timeline for %s."),
			*ActorGuid.ToString());
	}
}

// ---------------------------------------------------------------------------
// Capacity
// ---------------------------------------------------------------------------

int32 UTemporalRewindSubsystem::ComputeTimelineCapacity() const
{
	const float Interval = ActiveRecordingStrategy
		? ActiveRecordingStrategy->GetRecordingInterval()
		: 0.1f;

	return FMath::Max(FMath::CeilToInt(BufferDuration / FMath::Max(Interval, KINDA_SMALL_NUMBER)) + 1, 2);
}

// ---------------------------------------------------------------------------
// Rewind
// ---------------------------------------------------------------------------

void UTemporalRewindSubsystem::Cancel()
{
	if (!IsRewindActive() || !ActiveSessionContext)
	{
		UE_LOG(LogTemporalRewind, Warning,
			TEXT("[TemporalRewindSubsystem] Cancel called while no rewind session active. Rejected."));
		return;
	}

	ActiveSessionContext->SetScrubTimestamp(ActiveSessionContext->GetOriginalPresentTimestamp());
	ApplyStateAtScrubTimestamp();

	UE_LOG(LogTemporalRewind, Display,
		TEXT("[TemporalRewindSubsystem] Rewind cancelled; snapped back to present."));

	OnRewindCancelled.Broadcast();
	ActiveSessionContext = nullptr;
	BeginCooldown();
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

float UTemporalRewindSubsystem::GetScrubTimestamp() const
{
	return ActiveSessionContext ? ActiveSessionContext->GetCurrentScrubTimestamp() : 0.0f;
}

float UTemporalRewindSubsystem::GetOldestAllowedTimestamp() const
{
	return ActiveSessionContext ? ActiveSessionContext->GetOldestAllowedTimestamp() : 0.f;
}

float UTemporalRewindSubsystem::GetOriginalPresentTimestamp() const
{
	return ActiveSessionContext ? ActiveSessionContext->GetOriginalPresentTimestamp() : 0.f;
}

void UTemporalRewindSubsystem::SetRewindSpeed(float NewSpeed)
{
	RewindSpeed = FMath::Max(NewSpeed, 0.01f);

	if (ActiveSessionContext)
	{
		ActiveSessionContext->SetRewindSpeed(RewindSpeed);
	}
}

void UTemporalRewindSubsystem::PauseRewind()
{
	if (TryTransitionTo(ETemporalSystemState::Paused))
	{
		OnRewindPaused.Broadcast();
	}
}

void UTemporalRewindSubsystem::ResumeRewind()
{
	if (TryTransitionTo(ETemporalSystemState::Rewinding))
	{
		OnRewindResumed.Broadcast();
	}
}

void UTemporalRewindSubsystem::ScrubTo(float Timestamp)
{
	if (!IsRewindActive() || !ActiveSessionContext)
	{
		UE_LOG(LogTemporalRewind, Warning,
			TEXT("[TemporalRewindSubsystem] ScrubTo called while no rewind session active. Rejected."));
		return;
	}

	if (CurrentState != ETemporalSystemState::Scrubbing)
	{
		TryTransitionTo(ETemporalSystemState::Scrubbing);
	}

	ActiveSessionContext->SetScrubTimestamp(Timestamp);
	ApplyStateAtScrubTimestamp();
	OnScrubUpdated.Broadcast(ActiveSessionContext->GetCurrentScrubTimestamp());
}

void UTemporalRewindSubsystem::StartRewind()
{
	bool bAnyData = false;
	for (const TPair<FGuid, TObjectPtr<UActorSnapshotTimeline>>& Pair : ActorTimelines)
	{
		if (Pair.Value && !Pair.Value->IsEmpty())
		{
			bAnyData = true;
			break;
		}
	}

	if (!bAnyData)
	{
		UE_LOG(LogTemporalRewind, Warning,
			TEXT("[TemporalRewindSubsystem] StartRewind rejected: no recorded snapshots."));
		return;
	}

	if (!TryTransitionTo(ETemporalSystemState::Rewinding))
	{
		return;
	}

	const float NowTimestamp = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

	ActiveSessionContext = NewObject<URewindSessionContext>(this);
	ActiveSessionContext->Initialize(NowTimestamp, MaxScrubRange, RewindSpeed);

	UE_LOG(LogTemporalRewind, Display,
		TEXT("[TemporalRewindSubsystem] Rewind session started at T=%.3f"), NowTimestamp);

	OnRewindStarted.Broadcast();
}

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------

void UTemporalRewindSubsystem::SetMaxScrubRange(float NewRange)
{
	MaxScrubRange = FMath::Clamp(NewRange, 0.0f, BufferDuration);
}

void UTemporalRewindSubsystem::SetRecordingStrategy(URecordingStrategy* NewStrategy)
{
	if (!NewStrategy)
	{
		UE_LOG(LogTemporalRewind, Warning,
			TEXT("[TemporalRewindSubsystem] SetRecordingStrategy(nullptr) rejected."));
		return;
	}

	ActiveRecordingStrategy = NewStrategy;
}

// ---------------------------------------------------------------------------
// Apply state
// ---------------------------------------------------------------------------

void UTemporalRewindSubsystem::ApplyStateAtScrubTimestamp()
{
	if (!ActiveSessionContext)
	{
		return;
	}

	const float ScrubTime = ActiveSessionContext->GetCurrentScrubTimestamp();

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

		FTemporalSnapshot Before, After;
		float Alpha = 0.0f;

		if (Timeline->GetSnapshotPair(ScrubTime, Before, After, Alpha))
		{
			if (IRewindable* Rewindable = Cast<IRewindable>(Obj))
			{
				// C++ IRewindable (e.g. AutoSnapshotComponent) — supports interpolation.
				Rewindable->ApplyStateInterpolated(Before, After, Alpha);
			}
			else
			{
				// Blueprint IRewindable — Cast fails for BP-only interface implementations.
				// Fall back to the reflection path which works for both C++ and Blueprint.
				IRewindable::Execute_ApplyState(Obj, Before);
			}
		}
	}

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

// ---------------------------------------------------------------------------
// Editor
// ---------------------------------------------------------------------------

#if WITH_EDITOR

void UTemporalRewindSubsystem::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	MaxScrubRange = FMath::Min(MaxScrubRange, BufferDuration);
}

#endif