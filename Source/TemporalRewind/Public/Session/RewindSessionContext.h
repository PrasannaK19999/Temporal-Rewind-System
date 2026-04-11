// Copyright (c) Temporal Rewind System. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Data/TemporalEnums.h"
#include "UObject/Object.h"

#include "RewindSessionContext.generated.h"

/**
 * Transient state for one active rewind session.
 * Created on StartRewind, destroyed on Commit/Cancel.
 */
UCLASS()
class TEMPORALREWIND_API URewindSessionContext : public UObject
{
	GENERATED_BODY()

public:

	void Initialize(float PresentTimestamp, float InMaxScrubRange, float InSpeed);

	/** O(1). Advance scrub by DeltaTime * Speed in the current direction. */
	void AdvanceScrub(float DeltaTime);

	void SetScrubTimestamp(float NewTimestamp);

	void SetDirection(ERewindDirection NewDirection) { Direction = NewDirection; }

	ERewindDirection GetDirection() const { return Direction; }

	float GetCurrentScrubTimestamp() const { return CurrentScrubTimestamp; }

	float GetOldestAllowedTimestamp() const { return OldestAllowedTimestamp; }

	float GetOriginalPresentTimestamp() const { return OriginalPresentTimestamp; }

	float GetRewindSpeed() const { return RewindSpeed; }

	bool IsAtOldestPoint() const;

	bool IsAtPresentPoint() const;

private:

	void ClampScrubTimestamp();

	float CurrentScrubTimestamp = 0.0f;

	ERewindDirection Direction = ERewindDirection::Backward;

	float OldestAllowedTimestamp = 0.0f;

	float OriginalPresentTimestamp = 0.0f;

	float RewindSpeed = 1.0f;

	float RewindStartTimestamp = 0.0f;
};