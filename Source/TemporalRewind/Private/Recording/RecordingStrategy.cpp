// Copyright (c) Temporal Rewind System. All rights reserved.

#include "Recording/RecordingStrategy.h"
#include "TemporalRewindModule.h"

bool URecordingStrategy::ShouldRecordThisTick(float DeltaTime)
{
	AccumulatedTime += DeltaTime;

	if (AccumulatedTime >= RecordingInterval)
	{
		// Subtract instead of zeroing so leftover time carries into the next
		// interval — preserves average capture rate under jittery frame times.
		AccumulatedTime -= RecordingInterval;
		return true;
	}

	return false;
}

void URecordingStrategy::Reset()
{
	AccumulatedTime = 0.0f;
}

void URecordingStrategy::SetRecordingInterval(float NewInterval)
{
	if (NewInterval < MinRecordingInterval)
	{
		UE_LOG(LogTemporalRewind, Warning,
			TEXT("[RecordingStrategy] SetRecordingInterval(%f) below minimum %f; clamping."),
			NewInterval, MinRecordingInterval);
		NewInterval = MinRecordingInterval;
	}
	RecordingInterval = NewInterval;
}