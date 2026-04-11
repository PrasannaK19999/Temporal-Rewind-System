// Copyright (c) Temporal Rewind System. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Data/TemporalEnums.h"
#include "Data/TemporalSnapshot.h"
#include "Rewindable.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class URewindable : public UInterface
{
	GENERATED_BODY()
};

/**
 * Opt-in contract for actors that participate in the Temporal Rewind System.
 * The subsystem never inspects snapshot contents — the actor owns what is
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

	/** MUST be O(1). Called every recording tick. No allocations. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Temporal Rewind")
	bool HasStateChanged() const;

	/** Only called immediately after HasStateChanged() returns true. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Temporal Rewind")
	FTemporalSnapshot CaptureState();

	/** Restore from a snapshot this actor previously produced. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Temporal Rewind")
	void ApplyState(const FTemporalSnapshot& Snapshot);
};