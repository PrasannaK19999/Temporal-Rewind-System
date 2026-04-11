// Copyright (c) Temporal Rewind System. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Data/TemporalEnums.h"
#include "TemporalRewindSettings.generated.h"

/**
 * Project-wide defaults for the Temporal Rewind System.
 * Edit → Project Settings → Plugins → Temporal Rewind
 *
 * These values are read once by UTemporalRewindSubsystem::Initialize() and
 * applied as the starting state. They are NOT live-linked — runtime calls
 * like SetMaxScrubRange() override them independently after startup.
 *
 * Saved to: Config/DefaultTemporalRewind.ini
 */
UCLASS(Config = TemporalRewind, DefaultConfig, meta = (DisplayName = "Temporal Rewind"))
class TEMPORALREWIND_API UTemporalRewindSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UTemporalRewindSettings();

	virtual FName GetContainerName() const override { return TEXT("Project"); }
	virtual FName GetCategoryName()  const override { return TEXT("Plugins"); }
	virtual FName GetSectionName()   const override { return TEXT("Temporal Rewind"); }

#if WITH_EDITOR
	virtual FText GetSectionText() const override
	{
		return NSLOCTEXT("TemporalRewind", "SettingsSection", "Temporal Rewind");
	}
	virtual FText GetSectionDescription() const override
	{
		return NSLOCTEXT("TemporalRewind", "SettingsDesc",
			"Project-wide defaults for the Temporal Rewind System. "
			"Applied on subsystem startup. Override at runtime via subsystem API.");
	}
#endif

	// --- Recording -----------------------------------------------------------

	/** How many seconds of snapshot history to keep. Ring buffer is sized from this. */
	UPROPERTY(Config, EditAnywhere, Category = "Recording",
		meta = (ClampMin = "1.0", ClampMax = "60.0", ForceUnits = "s",
			ToolTip = "Total history window. More = more memory. Caps MaxScrubRange."))
	float BufferDuration = 10.0f;

	/** How far back the player can scrub during a rewind session. Clamped to BufferDuration. */
	UPROPERTY(Config, EditAnywhere, Category = "Recording",
		meta = (ClampMin = "1.0", ForceUnits = "s",
			ToolTip = "Gameplay-visible rewind window. Set via ability/progression at runtime."))
	float MaxScrubRange = 5.0f;

	/** Enable to pick from standard presets. Disable for a custom interval. */
	UPROPERTY(Config, EditAnywhere, Category = "Recording",
		meta = (ToolTip = "Enable to pick from standard presets. Disable for a custom interval."))
	bool bUsePresetRecordingRate = true;

	/** Preset recording rate. Only used when bUsePresetRecordingRate is true. */
	UPROPERTY(Config, EditAnywhere, Category = "Recording",
		meta = (EditCondition = "bUsePresetRecordingRate",
			ToolTip = "30 Hz is recommended for most games. 60 Hz for precision at higher memory cost."))
	ERecordingRatePreset RecordingRatePreset = ERecordingRatePreset::High;

	/**
	 * Custom seconds between recording ticks. Only used when bUsePresetRecordingRate is false.
	 * 0.033 = 30 Hz. 0.016 = 60 Hz.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Recording",
		meta = (EditCondition = "!bUsePresetRecordingRate",
			ClampMin = "0.001", ClampMax = "1.0", ForceUnits = "s",
			ToolTip = "Lower = smoother rewind at higher memory cost."))
	float RecordingInterval = 0.033f;

	/** Resolves the effective recording interval from preset or custom value. */
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

	// --- Playback ------------------------------------------------------------

	/** Cooldown before the player can trigger another rewind session. */
	UPROPERTY(Config, EditAnywhere, Category = "Playback",
		meta = (ClampMin = "0.0", ForceUnits = "s",
			ToolTip = "Set to 0 for no cooldown. Drive at runtime via ability system."))
	float CooldownDuration = 3.0f;

	/** How fast the rewind plays backward. 1.0 = real time, 2.0 = double speed. */
	UPROPERTY(Config, EditAnywhere, Category = "Playback",
		meta = (ClampMin = "0.1", ClampMax = "10.0",
			ToolTip = "Captured once at session start. Changing this mid-rewind has no effect."))
	float RewindSpeed = 1.0f;

	// --- Lifecycle -----------------------------------------------------------

	/**
	 * Default orphan policy for actors that die without an explicit policy.
	 * Per-actor overrides via IRewindable::GetOrphanedTimelinePolicy() take precedence.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Lifecycle",
		meta = (ToolTip = "What happens to a dead actor's timeline data by default."))
	EOrphanedTimelinePolicy DefaultOrphanPolicy = EOrphanedTimelinePolicy::KeepUntilOverwritten;
};
