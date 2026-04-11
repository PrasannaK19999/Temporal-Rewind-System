// Copyright (c) Temporal Rewind System. All rights reserved.

#include "Timeline/ActorSnapshotTimeline.h"
#include "TemporalRewindModule.h"

void UActorSnapshotTimeline::Initialize(const FGuid& InOwnerGuid, int32 InCapacity)
{
	if (InCapacity <= 0)
	{
		UE_LOG(LogTemporalRewind, Error,
			TEXT("[ActorSnapshotTimeline] Initialize called with non-positive capacity %d for owner %s."),
			InCapacity, *InOwnerGuid.ToString());
		return;
	}

	OwnerGuid = InOwnerGuid;
	Capacity = InCapacity;
	HeadIndex = 0;
	Count = 0;

	Buffer.Reset();
	Buffer.SetNum(Capacity);
}

void UActorSnapshotTimeline::PushSnapshot(const FTemporalSnapshot& Snapshot)
{
	if (Capacity <= 0)
	{
		UE_LOG(LogTemporalRewind, Error,
			TEXT("[ActorSnapshotTimeline] PushSnapshot called before Initialize for owner %s."),
			*OwnerGuid.ToString());
		return;
	}

	Buffer[HeadIndex] = Snapshot;
	HeadIndex = (HeadIndex + 1) % Capacity;

	if (Count < Capacity)
	{
		++Count;
	}
	// When full, Count stays at Capacity and the write above has overwritten
	// the oldest entry — that IS the ring buffer's expiration policy.
}

bool UActorSnapshotTimeline::GetSnapshotAtTime(float Timestamp, FTemporalSnapshot& OutSnapshot) const
{
	const int32 FloorLogical = FindFloorLogicalIndex(Timestamp);
	if (FloorLogical == INDEX_NONE)
	{
		return false;
	}

	OutSnapshot = Buffer[LogicalToPhysicalIndex(FloorLogical)];
	return true;
}

void UActorSnapshotTimeline::GetSnapshotsBetween(float StartTime, float EndTime, TArray<FTemporalSnapshot>& OutSnapshots) const
{
	OutSnapshots.Reset();

	if (Count == 0 || EndTime < StartTime)
	{
		return;
	}

	// Find the first logical index whose timestamp is >= StartTime.
	// Then walk forward while timestamp <= EndTime. Since timestamps are
	// monotonically increasing (UWorld::GetTimeSeconds never goes backward
	// during recording), this walk is O(K) and the initial search is O(log N).
	const int32 FloorAtStart = FindFloorLogicalIndex(StartTime);
	int32 FirstLogical = (FloorAtStart == INDEX_NONE) ? 0 : FloorAtStart;

	// If the floor entry is strictly before StartTime, advance past it.
	if (FirstLogical < Count &&
		Buffer[LogicalToPhysicalIndex(FirstLogical)].Timestamp < StartTime)
	{
		++FirstLogical;
	}

	for (int32 LogicalIdx = FirstLogical; LogicalIdx < Count; ++LogicalIdx)
	{
		const FTemporalSnapshot& Entry = Buffer[LogicalToPhysicalIndex(LogicalIdx)];
		if (Entry.Timestamp > EndTime)
		{
			break;
		}
		OutSnapshots.Add(Entry);
	}
}

void UActorSnapshotTimeline::TruncateAfter(float Timestamp)
{
	if (Count == 0)
	{
		return;
	}

	// Find the last logical index with timestamp <= Timestamp. Everything
	// after it becomes invalid. We don't zero the underlying slots — they
	// will be overwritten by future PushSnapshot calls. Only Count moves.
	const int32 FloorLogical = FindFloorLogicalIndex(Timestamp);
	if (FloorLogical == INDEX_NONE)
	{
		// Nothing at or before Timestamp — everything is "after", clear all.
		Clear();
		return;
	}

	// Keep [0 .. FloorLogical] inclusive, drop the rest.
	const int32 NewCount = FloorLogical + 1;

	// HeadIndex must point to the slot *after* the last kept entry —
	// that's where the next PushSnapshot will write.
	HeadIndex = LogicalToPhysicalIndex(NewCount - 1);
	HeadIndex = (HeadIndex + 1) % Capacity;
	Count = NewCount;
}

bool UActorSnapshotTimeline::GetOldestTimestamp(float& OutTimestamp) const
{
	if (Count == 0)
	{
		return false;
	}
	OutTimestamp = Buffer[LogicalToPhysicalIndex(0)].Timestamp;
	return true;
}

bool UActorSnapshotTimeline::GetNewestTimestamp(float& OutTimestamp) const
{
	if (Count == 0)
	{
		return false;
	}
	OutTimestamp = Buffer[LogicalToPhysicalIndex(Count - 1)].Timestamp;
	return true;
}

void UActorSnapshotTimeline::Clear()
{
	HeadIndex = 0;
	Count = 0;
	// Buffer allocation preserved.
}

int32 UActorSnapshotTimeline::FindFloorLogicalIndex(float TargetTimestamp) const
{
	if (Count == 0)
	{
		return INDEX_NONE;
	}

	// Standard floor-binary-search over the LOGICAL index space [0, Count).
	// Each comparison dereferences through LogicalToPhysicalIndex, which
	// is the only trick here — the underlying buffer is wrapped but the
	// logical view is strictly increasing in timestamp.
	int32 Low = 0;
	int32 High = Count - 1;
	int32 Result = INDEX_NONE;

	while (Low <= High)
	{
		const int32 Mid = Low + ((High - Low) >> 1);
		const float MidTimestamp = Buffer[LogicalToPhysicalIndex(Mid)].Timestamp;

		if (MidTimestamp <= TargetTimestamp)
		{
			Result = Mid;
			Low = Mid + 1;
		}
		else
		{
			High = Mid - 1;
		}
	}

	return Result;
}