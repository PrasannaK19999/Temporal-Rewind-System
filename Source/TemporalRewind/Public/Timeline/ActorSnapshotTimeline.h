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

	void Initialize(const FGuid& InOwnerGuid, int32 InCapacity);

	/** O(1). Write at head, advance, overwrite oldest on wrap. */
	void PushSnapshot(const FTemporalSnapshot& Snapshot);

	/** O(log N). Binary search for the snapshot nearest to Timestamp (floor). */
	bool GetSnapshotAtTime(float Timestamp, FTemporalSnapshot& OutSnapshot) const;

	/** O(log N). Returns the floor snapshot and the next snapshot with an alpha in [0,1].
	 *  If only one snapshot exists at or before Timestamp, OutAfter == OutBefore and OutAlpha == 0. */
	bool GetSnapshotPair(float Timestamp,FTemporalSnapshot& OutBefore, FTemporalSnapshot& OutAfter, float& OutAlpha) const;

	void GetSnapshotsBetween(float StartTime, float EndTime, TArray<FTemporalSnapshot>& OutSnapshots) const;

	void TruncateAfter(float Timestamp);

	bool GetOldestTimestamp(float& OutTimestamp) const;

	bool GetNewestTimestamp(float& OutTimestamp) const;

	bool IsEmpty() const { return Count == 0; }

	void Clear();

	int32 GetCount() const { return Count; }

	int32 GetCapacity() const { return Capacity; }

	const FGuid& GetOwnerGuid() const { return OwnerGuid; }

private:
	friend class UTemporalDebugLibrary;

	/** Map a logical index [0, Count) to the physical buffer slot. */
	FORCEINLINE int32 LogicalToPhysicalIndex(int32 LogicalIndex) const
	{
		return (HeadIndex - Count + LogicalIndex + Capacity) % Capacity;
	}

	int32 FindFloorLogicalIndex(float TargetTimestamp) const;

	UPROPERTY()
	FGuid OwnerGuid;

	UPROPERTY()
	TArray<FTemporalSnapshot> Buffer;

	int32 HeadIndex = 0;

	int32 Count = 0;

	int32 Capacity = 0;
};