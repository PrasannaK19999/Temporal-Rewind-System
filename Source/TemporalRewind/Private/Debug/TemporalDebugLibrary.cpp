// Copyright (c) 2026 Prasanna Keerthivasan. All rights reserved.

#include "Debug/TemporalDebugLibrary.h"

#include "Data/TemporalSnapshot.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Subsystem/TemporalRewindSubsystem.h"
#include "TemporalRewindModule.h"
#include "Timeline/ActorSnapshotTimeline.h"

// ---------------------------------------------------------------------------
// Console commands
// ---------------------------------------------------------------------------

static FAutoConsoleCommandWithWorld GCmd_PrintStats(
	TEXT("TemporalRewind.PrintStats"),
	TEXT("Full Temporal Rewind stats dump to Output Log."),
	FConsoleCommandWithWorldDelegate::CreateStatic([](UWorld* World)
		{
			UTemporalDebugLibrary::PrintStatsToLog(World);
		})
);

static FAutoConsoleCommandWithWorld GCmd_PrintState(
	TEXT("TemporalRewind.PrintState"),
	TEXT("Current Temporal Rewind state machine state."),
	FConsoleCommandWithWorldDelegate::CreateStatic([](UWorld* World)
		{
			const FString State = UTemporalDebugLibrary::GetSystemStateString(World);
			UE_LOG(LogTemporalRewind, Display, TEXT("[TemporalDebug] State: %s"), *State);
		})
);

// ---------------------------------------------------------------------------
// Internal
// ---------------------------------------------------------------------------

UTemporalRewindSubsystem* UTemporalDebugLibrary::GetSubsystem(const UObject* WorldContextObject)
{
	if (!WorldContextObject)
	{
		return nullptr;
	}

	const UWorld* World = GEngine
		? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull)
		: nullptr;

	return World ? World->GetSubsystem<UTemporalRewindSubsystem>() : nullptr;
}

int64 UTemporalDebugLibrary::CalcTimelineMemory(const UActorSnapshotTimeline* Timeline)
{
	if (!Timeline)
	{
		return 0;
	}

	int64 Total = static_cast<int64>(Timeline->Capacity) * sizeof(FTemporalSnapshot);

	for (int32 i = 0; i < Timeline->Count; ++i)
	{
		Total += Timeline->Buffer[Timeline->LogicalToPhysicalIndex(i)].StateBlob.GetAllocatedSize();
	}

	return Total;
}

float UTemporalDebugLibrary::CalcTimelineDuration(const UActorSnapshotTimeline* Timeline)
{
	if (!Timeline || Timeline->Count < 2)
	{
		return 0.0f;
	}

	const float Oldest = Timeline->Buffer[Timeline->LogicalToPhysicalIndex(0)].Timestamp;
	const float Newest = Timeline->Buffer[Timeline->LogicalToPhysicalIndex(Timeline->Count - 1)].Timestamp;
	return Newest - Oldest;
}

// ---------------------------------------------------------------------------
// Actor registry
// ---------------------------------------------------------------------------

int32 UTemporalDebugLibrary::GetOrphanedTimelineCount(const UObject* WorldContextObject)
{
	const UTemporalRewindSubsystem* Sub = GetSubsystem(WorldContextObject);
	return Sub ? Sub->OrphanedTimelines.Num() : 0;
}

int32 UTemporalDebugLibrary::GetRegisteredActorCount(const UObject* WorldContextObject)
{
	const UTemporalRewindSubsystem* Sub = GetSubsystem(WorldContextObject);
	return Sub ? Sub->RegisteredActors.Num() : 0;
}

// ---------------------------------------------------------------------------
// Duration
// ---------------------------------------------------------------------------

float UTemporalDebugLibrary::GetPlayableRewindDuration(const UObject* WorldContextObject)
{
	const UTemporalRewindSubsystem* Sub = GetSubsystem(WorldContextObject);
	if (!Sub)
	{
		return 0.0f;
	}

	return FMath::Min(Sub->MaxScrubRange, GetRecordedDuration(WorldContextObject));
}

float UTemporalDebugLibrary::GetRecordedDuration(const UObject* WorldContextObject)
{
	const UTemporalRewindSubsystem* Sub = GetSubsystem(WorldContextObject);
	if (!Sub)
	{
		return 0.0f;
	}

	float MaxDuration = 0.0f;
	for (const auto& Pair : Sub->ActorTimelines)
	{
		MaxDuration = FMath::Max(MaxDuration, CalcTimelineDuration(Pair.Value.Get()));
	}

	return MaxDuration;
}

float UTemporalDebugLibrary::GetRecordedDurationForActor(const UObject* WorldContextObject, FGuid ActorGuid)
{
	const UTemporalRewindSubsystem* Sub = GetSubsystem(WorldContextObject);
	if (!Sub)
	{
		return 0.0f;
	}

	if (const TObjectPtr<UActorSnapshotTimeline>* T = Sub->ActorTimelines.Find(ActorGuid))
	{
		return CalcTimelineDuration(T->Get());
	}

	return 0.0f;
}

// ---------------------------------------------------------------------------
// Memory
// ---------------------------------------------------------------------------

int64 UTemporalDebugLibrary::GetMemoryUsageForActor(const UObject* WorldContextObject, FGuid ActorGuid)
{
	const UTemporalRewindSubsystem* Sub = GetSubsystem(WorldContextObject);
	if (!Sub)
	{
		return 0;
	}

	if (const TObjectPtr<UActorSnapshotTimeline>* T = Sub->ActorTimelines.Find(ActorGuid))
	{
		return CalcTimelineMemory(T->Get());
	}

	if (const TObjectPtr<UActorSnapshotTimeline>* T = Sub->OrphanedTimelines.Find(ActorGuid))
	{
		return CalcTimelineMemory(T->Get());
	}

	return 0;
}

int64 UTemporalDebugLibrary::GetTotalMemoryUsageBytes(const UObject* WorldContextObject)
{
	const UTemporalRewindSubsystem* Sub = GetSubsystem(WorldContextObject);
	if (!Sub)
	{
		return 0;
	}

	int64 Total = 0;

	for (const auto& Pair : Sub->ActorTimelines)
	{
		Total += CalcTimelineMemory(Pair.Value.Get());
	}

	for (const auto& Pair : Sub->OrphanedTimelines)
	{
		Total += CalcTimelineMemory(Pair.Value.Get());
	}

	return Total;
}

// ---------------------------------------------------------------------------
// Snapshots
// ---------------------------------------------------------------------------

int32 UTemporalDebugLibrary::GetSnapshotCountForActor(const UObject* WorldContextObject, FGuid ActorGuid)
{
	const UTemporalRewindSubsystem* Sub = GetSubsystem(WorldContextObject);
	if (!Sub)
	{
		return 0;
	}

	if (const TObjectPtr<UActorSnapshotTimeline>* T = Sub->ActorTimelines.Find(ActorGuid))
	{
		if (*T)
		{
			return (*T)->Count;
		}
	}

	if (const TObjectPtr<UActorSnapshotTimeline>* T = Sub->OrphanedTimelines.Find(ActorGuid))
	{
		if (*T)
		{
			return (*T)->Count;
		}
	}

	return 0;
}

int32 UTemporalDebugLibrary::GetTotalSnapshotCount(const UObject* WorldContextObject)
{
	const UTemporalRewindSubsystem* Sub = GetSubsystem(WorldContextObject);
	if (!Sub)
	{
		return 0;
	}

	int32 Total = 0;

	for (const auto& Pair : Sub->ActorTimelines)
	{
		if (Pair.Value)
		{
			Total += Pair.Value->Count;
		}
	}

	for (const auto& Pair : Sub->OrphanedTimelines)
	{
		if (Pair.Value)
		{
			Total += Pair.Value->Count;
		}
	}

	return Total;
}

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------

FString UTemporalDebugLibrary::GetSystemStateString(const UObject* WorldContextObject)
{
	const UTemporalRewindSubsystem* Sub = GetSubsystem(WorldContextObject);
	if (!Sub)
	{
		return TEXT("(no subsystem)");
	}

	switch (Sub->CurrentState)
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

// ---------------------------------------------------------------------------
// Log dump
// ---------------------------------------------------------------------------

void UTemporalDebugLibrary::PrintStatsToLog(const UObject* WorldContextObject)
{
	const UTemporalRewindSubsystem* Sub = GetSubsystem(WorldContextObject);
	if (!Sub)
	{
		UE_LOG(LogTemporalRewind, Warning, TEXT("[TemporalDebug] No subsystem found."));
		return;
	}

	const int32   Actors = Sub->RegisteredActors.Num();
	const int32   Orphans = Sub->OrphanedTimelines.Num();
	const int32   Snapshots = GetTotalSnapshotCount(WorldContextObject);
	const int64   MemBytes = GetTotalMemoryUsageBytes(WorldContextObject);
	const float   Recorded = GetRecordedDuration(WorldContextObject);
	const float   Playable = GetPlayableRewindDuration(WorldContextObject);
	const FString State = GetSystemStateString(WorldContextObject);
	const double  MemMB = static_cast<double>(MemBytes) / (1024.0 * 1024.0);

	UE_LOG(LogTemporalRewind, Display, TEXT("[TemporalDebug] ========== Temporal Rewind Stats =========="));
	UE_LOG(LogTemporalRewind, Display, TEXT("[TemporalDebug] State      : %s"), *State);
	UE_LOG(LogTemporalRewind, Display, TEXT("[TemporalDebug] Actors     : %d registered, %d orphaned"), Actors, Orphans);
	UE_LOG(LogTemporalRewind, Display, TEXT("[TemporalDebug] Snapshots  : %d total"), Snapshots);
	UE_LOG(LogTemporalRewind, Display, TEXT("[TemporalDebug] Memory     : %.2f MB (%lld bytes)"), MemMB, MemBytes);
	UE_LOG(LogTemporalRewind, Display, TEXT("[TemporalDebug] Recorded   : %.2fs  |  Playable: %.2fs"), Recorded, Playable);
	UE_LOG(LogTemporalRewind, Display, TEXT("[TemporalDebug] -------------------------------------------"));

	for (const auto& Pair : Sub->ActorTimelines)
	{
		const UActorSnapshotTimeline* T = Pair.Value.Get();
		if (!T)
		{
			continue;
		}

		UE_LOG(LogTemporalRewind, Display,
			TEXT("[TemporalDebug]   [live]    %s  snaps=%d  mem=%.1f KB  dur=%.2fs"),
			*Pair.Key.ToString(), T->Count,
			static_cast<double>(CalcTimelineMemory(T)) / 1024.0,
			CalcTimelineDuration(T));
	}

	for (const auto& Pair : Sub->OrphanedTimelines)
	{
		const UActorSnapshotTimeline* T = Pair.Value.Get();
		if (!T)
		{
			continue;
		}

		UE_LOG(LogTemporalRewind, Display,
			TEXT("[TemporalDebug]   [orphan]  %s  snaps=%d  mem=%.1f KB  dur=%.2fs"),
			*Pair.Key.ToString(), T->Count,
			static_cast<double>(CalcTimelineMemory(T)) / 1024.0,
			CalcTimelineDuration(T));
	}

	UE_LOG(LogTemporalRewind, Display, TEXT("[TemporalDebug] ==========================================="));
}