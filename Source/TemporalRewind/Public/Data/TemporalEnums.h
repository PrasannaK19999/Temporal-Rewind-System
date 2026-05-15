// Copyright (c) 2026 Prasanna Keerthivasan. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "TemporalEnums.generated.h"

/** State machine for UTemporalRewindSubsystem. */
UENUM(BlueprintType)
enum class ETemporalSystemState : uint8
{
	Idle        UMETA(DisplayName = "Idle"),
	Recording   UMETA(DisplayName = "Recording"),
	Rewinding   UMETA(DisplayName = "Rewinding"),
	Paused      UMETA(DisplayName = "Paused"),
	Scrubbing   UMETA(DisplayName = "Scrubbing"),
	Cooldown    UMETA(DisplayName = "Cooldown")
};

/** Per actor policy for what happens to a timeline when its actor is destroyed. */
UENUM(BlueprintType)
enum class EOrphanedTimelinePolicy : uint8
{
	PurgeImmediately       UMETA(DisplayName = "Purge Immediately"),
	KeepUntilOverwritten   UMETA(DisplayName = "Keep Until Overwritten"),
	KeepIndefinitely       UMETA(DisplayName = "Keep Indefinitely")
};

/** Direction of scrub movement during an active rewind session. */
UENUM(BlueprintType)
enum class ERewindDirection : uint8
{
	Backward   UMETA(DisplayName = "Backward"),
	Forward    UMETA(DisplayName = "Forward")
};

/** Preset recording rates for common use cases. */
UENUM(BlueprintType)
enum class ERecordingRatePreset : uint8
{
	Low        UMETA(DisplayName = "10 Hz  — Low fidelity, minimal memory"),
	Medium     UMETA(DisplayName = "20 Hz  — Balanced"),
	High       UMETA(DisplayName = "30 Hz  — Recommended"),
	Ultra      UMETA(DisplayName = "60 Hz  — Smooth, double memory")
};