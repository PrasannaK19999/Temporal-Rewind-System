// Copyright (c) Temporal Rewind System. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "RecordingStrategy.generated.h"

/**
 * Time-based interval gate for recording ticks.
 * Owned by UTemporalRewindSubsystem.
 */
UCLASS(BlueprintType)
class TEMPORALREWIND_API URecordingStrategy : public UObject
{
	GENERATED_BODY()

public:

	/** O(1). Returns true when accumulated time crosses the interval threshold. */
	bool ShouldRecordThisTick(float DeltaTime);

	void Reset();

	UFUNCTION(BlueprintCallable, Category = "Temporal Rewind")
	void SetRecordingInterval(float NewInterval);

	UFUNCTION(BlueprintPure, Category = "Temporal Rewind")
	float GetRecordingInterval() const { return RecordingInterval; }

	UFUNCTION(BlueprintPure, Category = "Temporal Rewind")
	float GetMinRecordingInterval() const { return MinRecordingInterval; }

protected:

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Temporal Rewind",
		meta = (ClampMin = "0.001", UIMin = "0.001", ForceUnits = "s"))
	float RecordingInterval = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Temporal Rewind",
		meta = (ClampMin = "0.001", UIMin = "0.001", ForceUnits = "s"))
	float MinRecordingInterval = 0.01f;

private:

	float AccumulatedTime = 0.0f;
};