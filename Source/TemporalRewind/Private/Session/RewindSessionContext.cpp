// Copyright (c) Temporal Rewind System. All rights reserved.

#include "Session/RewindSessionContext.h"
#include "TemporalRewindModule.h"

void URewindSessionContext::Initialize(float PresentTimestamp, float MaxScrubRange, float Speed)
{
	RewindStartTimestamp = PresentTimestamp;
	OriginalPresentTimestamp = PresentTimestamp;
	CurrentScrubTimestamp = PresentTimestamp;
	OldestAllowedTimestamp = PresentTimestamp - FMath::Max(MaxScrubRange, 0.0f);
	RewindSpeed = FMath::Max(Speed, 0.0f);
	Direction = ERewindDirection::Backward;

	UE_LOG(LogTemporalRewind, Verbose,
		TEXT("[RewindSessionContext] Initialized: Present=%.3f Oldest=%.3f Speed=%.2f"),
		OriginalPresentTimestamp, OldestAllowedTimestamp, RewindSpeed);
}

void URewindSessionContext::AdvanceScrub(float DeltaTime)
{
	// Backward moves scrub timestamp toward the past (subtract).
	// Forward moves it toward the present (add).
	const float Sign = (Direction == ERewindDirection::Backward) ? -1.0f : 1.0f;
	CurrentScrubTimestamp += Sign * RewindSpeed * DeltaTime;
	ClampScrubTimestamp();
}

void URewindSessionContext::SetScrubTimestamp(float NewTimestamp)
{
	CurrentScrubTimestamp = NewTimestamp;
	ClampScrubTimestamp();
}

bool URewindSessionContext::IsAtOldestPoint() const
{
	return FMath::IsNearlyEqual(CurrentScrubTimestamp, OldestAllowedTimestamp);
}

bool URewindSessionContext::IsAtPresentPoint() const
{
	return FMath::IsNearlyEqual(CurrentScrubTimestamp, OriginalPresentTimestamp);
}

void URewindSessionContext::ClampScrubTimestamp()
{
	CurrentScrubTimestamp = FMath::Clamp(
		CurrentScrubTimestamp,
		OldestAllowedTimestamp,
		OriginalPresentTimestamp);
}