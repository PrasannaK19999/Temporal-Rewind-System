// Copyright (c) 2026 Prasanna Keerthivasan. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Data/TemporalEnums.h"
#include "Engine/DeveloperSettings.h"

#include "TemporalRewindSettings.generated.h"

/**
 * Project-wide defaults for the Temporal Rewind System.
 * Edit -> Project Settings -> Plugins -> Temporal Rewind
 *
 * Read once by UTemporalRewindSubsystem::Initialize().
 * Not live-linked. Runtime setters override independently.
 */
UCLASS(Config = TemporalRewind, DefaultConfig, meta = (DisplayName = "Temporal Rewind"))
class TEMPORALREWIND_API UTemporalRewindSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:

	UTemporalRewindSettings();

	// --- UDeveloperSettings --------------------------------------------------

	virtual FName GetCategoryName()  const override { return TEXT("Plugins"); }
	virtual FName GetContainerName() const override { return TEXT("Project"); }
	virtual FName GetSectionName()   const override { return TEXT("Temporal Rewind"); }

#if WITH_EDITOR
	virtual FText GetSectionDescription() const override
	{
		return NSLOCTEXT("TemporalRewind", "SettingsDesc",
			"Project-wide defaults for the Temporal Rewind System. "
			"Applied on subsystem startup. Override at runtime via subsystem API.");
	}

	virtual FText GetSectionText() const override
	{
		return NSLOCTEXT("TemporalRewind", "SettingsSection", "Temporal Rewind");
	}
#endif

	float GetEffectiveRecordingInterval() const
	{
		if (!bUsePresetRecordingRate)
		{
			return RecordingInterval;
		}

		switch (RecordingRatePreset)
		{
		case ERecordingRatePreset::Low:    return 0.1f;
		case ERecordingRatePreset::Medium: return 0.05f;
		case ERecordingRatePreset::High:   return 0.033f;
		case ERecordingRatePreset::Ultra:  return 0.016f;
		default:                           return 0.033f;
		}
	}

	// --- Lifecycle -----------------------------------------------------------

	UPROPERTY(Config, EditAnywhere, Category = "Lifecycle",
		meta = (ToolTip = "What happens to a dead actor's timeline data by default."))
	EOrphanedTimelinePolicy DefaultOrphanPolicy = EOrphanedTimelinePolicy::KeepUntilOverwritten;

	// --- Playback ------------------------------------------------------------

	UPROPERTY(Config, EditAnywhere, Category = "Playback",
		meta = (ClampMin = "0.0", ForceUnits = "s",
			ToolTip = "Set to 0 for no cooldown."))
	float CooldownDuration = 3.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Playback",
		meta = (ClampMin = "0.1", ClampMax = "10.0",
			ToolTip = "Captured once at session start. Mid-rewind changes have no effect."))
	float RewindSpeed = 1.0f;

	// --- Recording -----------------------------------------------------------

	UPROPERTY(Config, EditAnywhere, Category = "Recording",
		meta = (ToolTip = "Use a standard preset or enter a custom interval below."))
	bool bUsePresetRecordingRate = true;

	UPROPERTY(Config, EditAnywhere, Category = "Recording",
		meta = (ClampMin = "1.0", ClampMax = "60.0", ForceUnits = "s",
			ToolTip = "Total history window. Caps MaxScrubRange."))
	float BufferDuration = 10.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Recording",
		meta = (ClampMin = "1.0", ForceUnits = "s",
			ToolTip = "Gameplay-visible rewind window. Clamped to BufferDuration."))
	float MaxScrubRange = 5.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Recording",
		meta = (EditCondition = "!bUsePresetRecordingRate",
			ClampMin = "0.001", ClampMax = "1.0", ForceUnits = "s",
			ToolTip = "Lower = smoother at higher memory cost. 0.033 = 30Hz."))
	float RecordingInterval = 0.033f;

	UPROPERTY(Config, EditAnywhere, Category = "Recording",
		meta = (EditCondition = "bUsePresetRecordingRate",
			ToolTip = "30 Hz recommended. 60 Hz for precision at higher memory cost."))
	ERecordingRatePreset RecordingRatePreset = ERecordingRatePreset::High;
};