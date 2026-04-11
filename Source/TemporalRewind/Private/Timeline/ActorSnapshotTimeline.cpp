// Copyright (c) Temporal Rewind System. All rights reserved.

#include "Timeline/ActorSnapshotTimeline.h"
#include "TemporalRewindModule.h"

void UActorSnapshotTimeline::Initialize(const FGuid& InOwnerGuid, int32 InCapacity)
{
	if (InCapacity <= 0)
	{
		UE_LOG(LogTemporalRewind, Error,
			TEXT("[ActorSnapshotTimeline] Initialize called with non-positive capacity %d."),
			InCapacity);
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
		return;
	}

	Buffer[HeadIndex] = Snapshot;
	HeadIndex = (HeadIndex + 1) % Capacity;

	if (Count < Capacity)
	{
		++Count;
	}
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

bool UActorSnapshotTimeline::GetSnapshotPair(float Timestamp,
	FTemporalSnapshot& OutBefore, FTemporalSnapshot& OutAfter, float& OutAlpha) const
{
	const int32 FloorLogical = FindFloorLogicalIndex(Timestamp);
	if (FloorLogical == INDEX_NONE)
	{
		return false;
	}

	OutBefore = Buffer[LogicalToPhysicalIndex(FloorLogical)];

	if (FloorLogical + 1 < Count)
	{
		OutAfter = Buffer[LogicalToPhysicalIndex(FloorLogical + 1)];
		const float Dt = OutAfter.Timestamp - OutBefore.Timestamp;
		OutAlpha = (Dt > KINDA_SMALL_NUMBER)
			? FMath::Clamp((Timestamp - OutBefore.Timestamp) / Dt, 0.0f, 1.0f)
			: 0.0f;
	}
	else
	{
		OutAfter = OutBefore;
		OutAlpha = 0.0f;
	}

	return true;
}

void UActorSnapshotTimeline::GetSnapshotsBetween(float StartTime, float EndTime,
	TArray<FTemporalSnapshot>& OutSnapshots) const
{
	OutSnapshots.Reset();

	if (Count == 0 || EndTime < StartTime)
	{
		return;
	}

	const int32 FloorAtStart = FindFloorLogicalIndex(StartTime);
	int32 FirstLogical = (FloorAtStart == INDEX_NONE) ? 0 : FloorAtStart;

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

	const int32 FloorLogical = FindFloorLogicalIndex(Timestamp);
	if (FloorLogical == INDEX_NONE)
	{
		Clear();
		return;
	}

	const int32 NewCount = FloorLogical + 1;
	HeadIndex = (LogicalToPhysicalIndex(NewCount - 1) + 1) % Capacity;
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
}

int32 UActorSnapshotTimeline::FindFloorLogicalIndex(float TargetTimestamp) const
{
	if (Count == 0)
	{
		return INDEX_NONE;
	}

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