// Copyright (c) 2026 Prasanna Keerthivasan. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TemporalScrubWidget.generated.h"

class USlider;
class UTextBlock;
class UButton;
class UProgressBar;
class UOverlay;
class UWidgetAnimation;
class UTemporalRewindSubsystem;

/**
 * UMG widget providing a visual scrub timeline for the Temporal Rewind System.
 *
 * Features:
 *   - Draggable slider that calls ScrubTo() on the subsystem
 *   - VCR controls: Rewind, Pause, Commit, Cancel
 *   - Live timestamp and state display
 *   - Speed selector (0.25x, 0.5x, 1x, 2x)
 *   - Auto-show/hide bound to subsystem delegates
 *   - Cooldown progress bar
 *
 * Setup:
 *   1. Create a Widget Blueprint parented to this class.
 *   2. In the designer, add the named widgets listed below (BindWidget auto-wires them).
 *   3. Add the widget to your HUD — it starts hidden and auto-shows when rewind begins.
 *   4. Optionally call ShowScrubWidget() / HideScrubWidget() for manual control.
 */
UCLASS()
class TEMPORALREWIND_API UTemporalScrubWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	// -------------------------------------------------------------------------
	// Bound widgets  (add these names to your Widget Blueprint designer)
	// -------------------------------------------------------------------------

	/** Main scrub slider. Dragging calls ScrubTo() on every tick. */
	UPROPERTY(BlueprintReadOnly, Category = "Temporal Rewind|UI", meta = (BindWidget))
	TObjectPtr<USlider> Slider_Scrub;

	/** Displays the current scrub timestamp. */
	UPROPERTY(BlueprintReadOnly, Category = "Temporal Rewind|UI", meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_Time;

	/** Displays the current system state (REWINDING / PAUSED / SCRUBBING / COOLDOWN). */
	UPROPERTY(BlueprintReadOnly, Category = "Temporal Rewind|UI", meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_State;

	/** Displays the scrub window duration (e.g. "5.00s window"). Optional. */
	UPROPERTY(BlueprintReadOnly, Category = "Temporal Rewind|UI", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Range;

	/** Displays the active rewind speed multiplier (e.g. "1.00x"). Optional. */
	UPROPERTY(BlueprintReadOnly, Category = "Temporal Rewind|UI", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Speed;

	// --- VCR buttons ---------------------------------------------------------

	/** Resumes auto-rewind playback. Enabled when Paused or Scrubbing. */
	UPROPERTY(BlueprintReadOnly, Category = "Temporal Rewind|UI", meta = (BindWidget))
	TObjectPtr<UButton> Button_Rewind;

	/** Pauses auto-rewind. Enabled only while Rewinding. */
	UPROPERTY(BlueprintReadOnly, Category = "Temporal Rewind|UI", meta = (BindWidget))
	TObjectPtr<UButton> Button_Pause;

	/** Commits the rewound state and exits the session. */
	UPROPERTY(BlueprintReadOnly, Category = "Temporal Rewind|UI", meta = (BindWidget))
	TObjectPtr<UButton> Button_Commit;

	/** Cancels and snaps back to the original present. */
	UPROPERTY(BlueprintReadOnly, Category = "Temporal Rewind|UI", meta = (BindWidget))
	TObjectPtr<UButton> Button_Cancel;

	// --- Speed buttons (all optional — omit if using a single fixed speed) ---

	UPROPERTY(BlueprintReadOnly, Category = "Temporal Rewind|UI", meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Speed025;

	UPROPERTY(BlueprintReadOnly, Category = "Temporal Rewind|UI", meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Speed05;

	UPROPERTY(BlueprintReadOnly, Category = "Temporal Rewind|UI", meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Speed1;

	UPROPERTY(BlueprintReadOnly, Category = "Temporal Rewind|UI", meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Speed2;

	// --- Cooldown / layout ---------------------------------------------------

	/** Progress bar showing how much cooldown time remains. Optional. */
	UPROPERTY(BlueprintReadOnly, Category = "Temporal Rewind|UI", meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> ProgressBar_Cooldown;

	/** Root overlay — wraps the entire scrub panel. Optional. */
	UPROPERTY(BlueprintReadOnly, Category = "Temporal Rewind|UI", meta = (BindWidgetOptional))
	TObjectPtr<UOverlay> Overlay_ScrubPanel;

	// --- Animations (create in WBP for show/hide polish) ---------------------

	UPROPERTY(BlueprintReadOnly, Category = "Temporal Rewind|UI", Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> Anim_ShowPanel;

	UPROPERTY(BlueprintReadOnly, Category = "Temporal Rewind|UI", Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> Anim_HidePanel;

	// -------------------------------------------------------------------------
	// Public API
	// -------------------------------------------------------------------------

	/** Make the widget visible and refresh all displays. Safe to call if already visible. */
	UFUNCTION(BlueprintCallable, Category = "Temporal Rewind|UI")
	void ShowScrubWidget();

	/** Collapse the widget. Plays Anim_HidePanel if present. */
	UFUNCTION(BlueprintCallable, Category = "Temporal Rewind|UI")
	void HideScrubWidget();

	/** Returns true if the widget is visible and delegate-bound. */
	UFUNCTION(BlueprintPure, Category = "Temporal Rewind|UI")
	bool IsScrubWidgetVisible() const;

protected:

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:

	// -------------------------------------------------------------------------
	// Subsystem cache
	// -------------------------------------------------------------------------

	UPROPERTY()
	TObjectPtr<UTemporalRewindSubsystem> CachedSubsystem;

	UTemporalRewindSubsystem* GetSubsystem();

	// -------------------------------------------------------------------------
	// Internal state
	// -------------------------------------------------------------------------

	/** True while the user holds the slider, suppressing programmatic slider updates. */
	bool bIsDragging = false;

	/** True once delegate bindings to the subsystem are active. */
	bool bIsBound = false;

	/** Slider normalization range, cached on RefreshSliderRange(). */
	float CachedOldestTimestamp = 0.f;
	float CachedNewestTimestamp = 0.f;

	/** Cooldown tracking for the progress bar. */
	float CooldownTotal = 0.f;
	float CooldownRemaining = 0.f;

	// -------------------------------------------------------------------------
	// Slider callbacks
	// -------------------------------------------------------------------------

	UFUNCTION()
	void OnSliderValueChanged(float Value);

	UFUNCTION()
	void OnSliderMouseCaptureBegin();

	UFUNCTION()
	void OnSliderMouseCaptureEnd();

	// -------------------------------------------------------------------------
	// VCR button callbacks
	// -------------------------------------------------------------------------

	UFUNCTION()
	void OnRewindClicked();

	UFUNCTION()
	void OnPauseClicked();

	UFUNCTION()
	void OnCommitClicked();

	UFUNCTION()
	void OnCancelClicked();

	// -------------------------------------------------------------------------
	// Speed button callbacks
	// -------------------------------------------------------------------------

	UFUNCTION()
	void OnSpeed025Clicked();

	UFUNCTION()
	void OnSpeed05Clicked();

	UFUNCTION()
	void OnSpeed1Clicked();

	UFUNCTION()
	void OnSpeed2Clicked();

	// -------------------------------------------------------------------------
	// Subsystem delegate handlers
	// -------------------------------------------------------------------------

	UFUNCTION()
	void HandleRewindStarted();

	UFUNCTION()
	void HandleScrubUpdated(float Timestamp);

	UFUNCTION()
	void HandleRewindPaused();

	UFUNCTION()
	void HandleRewindResumed();

	UFUNCTION()
	void HandleRewindCommitted();

	UFUNCTION()
	void HandleRewindCancelled();

	UFUNCTION()
	void HandleCooldownStarted(float Duration);

	UFUNCTION()
	void HandleCooldownEnded();

	// -------------------------------------------------------------------------
	// Display helpers
	// -------------------------------------------------------------------------

	void BindToSubsystem();
	void UnbindFromSubsystem();

	/** Re-reads oldest/newest timestamps from the subsystem and updates Text_Range. */
	void RefreshSliderRange();

	void UpdateTimeDisplay(float Timestamp);
	void UpdateStateDisplay();
	void UpdateButtonStates();
	void UpdateSpeedDisplay();

	/** Maps a world timestamp → slider [0, 1]. */
	float TimestampToSliderValue(float Timestamp) const;

	/** Maps slider [0, 1] → world timestamp. */
	float SliderValueToTimestamp(float SliderValue) const;

	static FText FormatTimestamp(float Seconds);
};
