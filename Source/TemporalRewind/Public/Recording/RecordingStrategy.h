// Copyright (c) Temporal Rewind System. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "RecordingStrategy.generated.h"

/**
 * Decides when a recording tick should fire. Time-based interval gate.
 * Owned by UTemporalRewindSubsystem.
 */
UCLASS(BlueprintType)
class TEMPORALREWIND_API URecordingStrategy : public UObject
{
	GENERATED_BODY()

public:
	/** O(1). Accumulate DeltaTime and return true when the interval is reached. */
	bool ShouldRecordThisTick(float DeltaTime);

	/** O(1). Zero the accumulator. Called on state transitions that restart recording. */
	void Reset();

	/** Runtime setter; clamped to MinRecordingInterval. */
	UFUNCTION(BlueprintCallable, Category = "Temporal Rewind")
	void SetRecordingInterval(float NewInterval);

	UFUNCTION(BlueprintPure, Category = "Temporal Rewind")
	float GetRecordingInterval() const { return RecordingInterval; }

	UFUNCTION(BlueprintPure, Category = "Temporal Rewind")
	float GetMinRecordingInterval() const { return MinRecordingInterval; }

protected:
	/**
	 * Seconds between recording ticks. Default 0.1s = 10 Hz, matching the
	 * architecture document's reference frequency. Clamped to MinRecordingInterval.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Temporal Rewind",
		meta = (ClampMin = "0.001", UIMin = "0.001", ForceUnits = "s"))
	float RecordingInterval = 0.1f;

	/**
	 * Hard floor for RecordingInterval. Exposed so projects with specialized
	 * needs (e.g. high-rate physics capture) can lower it, or defensive projects
	 * can raise it. The absolute engine-level floor is 0.001s to prevent
	 * degenerate per-frame accumulation; anything below that is rejected.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Temporal Rewind",
		meta = (ClampMin = "0.001", UIMin = "0.001", ForceUnits = "s"))
	float MinRecordingInterval = 0.01f;

private:
	/** Internal timer, resets when it crosses RecordingInterval. */
	float AccumulatedTime = 0.0f;
};