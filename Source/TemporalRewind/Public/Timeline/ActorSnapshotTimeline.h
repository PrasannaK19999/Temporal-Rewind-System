// Copyright (c) Temporal Rewind System. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Data/TemporalSnapshot.h"
#include "ActorSnapshotTimeline.generated.h"

/**
 * Fixed-capacity ring buffer of FTemporalSnapshot for a single actor.
 * Owned by UTemporalRewindSubsystem, one per registered actor.
 */
UCLASS()
class TEMPORALREWIND_API UActorSnapshotTimeline : public UObject
{
	GENERATED_BODY()

public:
	/** Pre-allocate the ring buffer and bind this timeline to its owner. */
	void Initialize(const FGuid& InOwnerGuid, int32 InCapacity);

	/** O(1). Write at head, advance, overwrite oldest on wrap. */
	void PushSnapshot(const FTemporalSnapshot& Snapshot);

	/** O(log N). Binary search for the snapshot nearest to Timestamp (floor). */
	bool GetSnapshotAtTime(float Timestamp, FTemporalSnapshot& OutSnapshot) const;

	/** O(log N + K). Range query, K = results in [Start, End]. */
	void GetSnapshotsBetween(float StartTime, float EndTime, TArray<FTemporalSnapshot>& OutSnapshots) const;

	/** O(log N). Invalidate all entries strictly after Timestamp. */
	void TruncateAfter(float Timestamp);

	/** O(1). Earliest valid entry; returns false if empty. */
	bool GetOldestTimestamp(float& OutTimestamp) const;

	/** O(1). Latest valid entry; returns false if empty. */
	bool GetNewestTimestamp(float& OutTimestamp) const;

	/** O(1). */
	bool IsEmpty() const { return Count == 0; }

	/** O(1). Reset head and count; buffer stays allocated. */
	void Clear();

	/** O(1). */
	int32 GetCount() const { return Count; }

	/** O(1). */
	int32 GetCapacity() const { return Capacity; }

	/** O(1). */
	const FGuid& GetOwnerGuid() const { return OwnerGuid; }

private:
	friend class UTemporalDebugLibrary;

	/** Map a logical index [0, Count) to the physical buffer slot. */
	FORCEINLINE int32 LogicalToPhysicalIndex(int32 LogicalIndex) const
	{
		// Oldest entry lives at (HeadIndex - Count) mod Capacity.
		// % on negative numbers is implementation-defined in C, so we add
		// Capacity before modding to guarantee a non-negative result.
		return (HeadIndex - Count + LogicalIndex + Capacity) % Capacity;
	}

	/** Binary search for the largest logical index with Timestamp <= Target.
	 *  Returns INDEX_NONE if no entry satisfies the condition. */
	int32 FindFloorLogicalIndex(float TargetTimestamp) const;

	UPROPERTY()
	FGuid OwnerGuid;

	UPROPERTY()
	TArray<FTemporalSnapshot> Buffer;

	/** Next write position. Always in [0, Capacity). */
	int32 HeadIndex = 0;

	/** Valid entries in the buffer. In [0, Capacity]. */
	int32 Count = 0;

	/** Fixed at Initialize(). Never resized. */
	int32 Capacity = 0;
};