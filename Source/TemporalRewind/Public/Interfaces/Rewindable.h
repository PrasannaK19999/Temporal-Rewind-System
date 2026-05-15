// Copyright (c) 2026 Prasanna Keerthivasan. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Data/TemporalEnums.h"
#include "Data/TemporalSnapshot.h"
#include "UObject/Interface.h"

#include "Rewindable.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class URewindable : public UInterface
{
	GENERATED_BODY()
};

/**
 * Opt-in contract for actors participating in the Temporal Rewind System.
 *
 * The subsystem never inspects snapshot contents. The actor owns what is
 * captured, how it is serialized, and how it is restored.
 */
class TEMPORALREWIND_API IRewindable
{
	GENERATED_BODY()

public:

	/** Stable identity across respawn and GC. Called once at registration. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Temporal Rewind")
	FGuid GetRewindableId() const;

	/** Per-actor destruction policy. Called once at registration and cached. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Temporal Rewind")
	EOrphanedTimelinePolicy GetOrphanedTimelinePolicy() const;

	/** O(1) dirty check. Called every recording tick. No allocations. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Temporal Rewind")
	bool HasStateChanged() const;

	/** Serialize current state. Only called when HasStateChanged() returns true. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Temporal Rewind")
	FTemporalSnapshot CaptureState();

	/** Restore from a snapshot this actor previously produced. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Temporal Rewind")
	void ApplyState(const FTemporalSnapshot& Snapshot);

	/**
	 * Blend between two bracketing snapshots.
	 *
	 * @param Before  Floor snapshot at or before the scrub timestamp.
	 * @param After   Ceiling snapshot after the scrub timestamp.
	 * @param Alpha   Interpolation factor [0,1]. 0 = Before, 1 = After.
	 *
	 * Default implementation snaps to Before via Execute_ApplyState.
	 * Override to provide per-field lerp/slerp interpolation.
	 */
	virtual void ApplyStateInterpolated(
		const FTemporalSnapshot& Before,
		const FTemporalSnapshot& After,
		float Alpha);
};