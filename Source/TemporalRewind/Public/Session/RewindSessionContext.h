// Copyright (c) Temporal Rewind System. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Data/TemporalEnums.h"
#include "RewindSessionContext.generated.h"

/**
 * Transient per-session state for one active rewind.
 * Owned by UTemporalRewindSubsystem; created on StartRewind, destroyed on Commit/Cancel.
 */
UCLASS()
class TEMPORALREWIND_API URewindSessionContext : public UObject
{
	GENERATED_BODY()

public:
	/** Set up the session bounds and initial scrub position. */
	void Initialize(float PresentTimestamp, float MaxScrubRange, float Speed);

	/** O(1). Move the scrub position by DeltaTime * Speed in the current direction. */
	void AdvanceScrub(float DeltaTime);

	/** O(1). Direct jump, clamped to [Oldest, Present]. */
	void SetScrubTimestamp(float NewTimestamp);

	float GetCurrentScrubTimestamp() const { return CurrentScrubTimestamp; }
	float GetOriginalPresentTimestamp() const { return OriginalPresentTimestamp; }
	float GetOldestAllowedTimestamp() const { return OldestAllowedTimestamp; }
	float GetRewindSpeed() const { return RewindSpeed; }

	void SetDirection(ERewindDirection NewDirection) { Direction = NewDirection; }
	ERewindDirection GetDirection() const { return Direction; }

	/** Scrub has reached the earliest allowed point — can't go further back. */
	bool IsAtOldestPoint() const;

	/** Scrub has returned to the original rewind-start timestamp. */
	bool IsAtPresentPoint() const;

private:
	/** Clamp helper — every scrub write goes through here. */
	void ClampScrubTimestamp();

	float RewindStartTimestamp = 0.0f;
	float OriginalPresentTimestamp = 0.0f;
	float CurrentScrubTimestamp = 0.0f;
	float OldestAllowedTimestamp = 0.0f;
	float RewindSpeed = 1.0f;

	ERewindDirection Direction = ERewindDirection::Backward;
};