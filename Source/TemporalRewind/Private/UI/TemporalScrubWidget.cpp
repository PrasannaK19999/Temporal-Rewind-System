// Copyright (c) 2026 Prasanna Keerthivasan. All rights reserved.

#include "UI/TemporalScrubWidget.h"

#include "Components/Button.h"
#include "Components/Overlay.h"
#include "Components/ProgressBar.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Data/TemporalEnums.h"
#include "Subsystem/TemporalRewindSubsystem.h"

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void UTemporalScrubWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Wire slider events
	if (Slider_Scrub)
	{
		Slider_Scrub->OnValueChanged.AddDynamic(this, &UTemporalScrubWidget::OnSliderValueChanged);
		Slider_Scrub->OnMouseCaptureBegin.AddDynamic(this, &UTemporalScrubWidget::OnSliderMouseCaptureBegin);
		Slider_Scrub->OnMouseCaptureEnd.AddDynamic(this, &UTemporalScrubWidget::OnSliderMouseCaptureEnd);
	}

	// Wire VCR buttons
	if (Button_Rewind) Button_Rewind->OnClicked.AddDynamic(this, &UTemporalScrubWidget::OnRewindClicked);
	if (Button_Pause)  Button_Pause->OnClicked.AddDynamic(this,  &UTemporalScrubWidget::OnPauseClicked);
	if (Button_Commit) Button_Commit->OnClicked.AddDynamic(this, &UTemporalScrubWidget::OnCommitClicked);
	if (Button_Cancel) Button_Cancel->OnClicked.AddDynamic(this, &UTemporalScrubWidget::OnCancelClicked);

	// Wire speed buttons
	if (Button_Speed025) Button_Speed025->OnClicked.AddDynamic(this, &UTemporalScrubWidget::OnSpeed025Clicked);
	if (Button_Speed05)  Button_Speed05->OnClicked.AddDynamic(this,  &UTemporalScrubWidget::OnSpeed05Clicked);
	if (Button_Speed1)   Button_Speed1->OnClicked.AddDynamic(this,   &UTemporalScrubWidget::OnSpeed1Clicked);
	if (Button_Speed2)   Button_Speed2->OnClicked.AddDynamic(this,   &UTemporalScrubWidget::OnSpeed2Clicked);

	// Start collapsed; auto-shows when HandleRewindStarted fires
	SetVisibility(ESlateVisibility::Collapsed);

	// Bind to subsystem so HandleRewindStarted can auto-show this widget
	BindToSubsystem();
}

void UTemporalScrubWidget::NativeDestruct()
{
	UnbindFromSubsystem();
	Super::NativeDestruct();
}

void UTemporalScrubWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Drive the cooldown progress bar by polling the subsystem.
	// OnCooldownStarted/Ended handle the endpoints; tick fills the middle.
	if (ProgressBar_Cooldown && IsVisible())
	{
		if (UTemporalRewindSubsystem* Sub = GetSubsystem())
		{
			const float Total = Sub->GetCooldownDuration();
			const float Remaining = Sub->GetCooldownTimeRemaining();

			ProgressBar_Cooldown->SetPercent(
				(Total > KINDA_SMALL_NUMBER) ? (Remaining / Total) : 0.f);
		}
	}
}

// ---------------------------------------------------------------------------
// Subsystem cache
// ---------------------------------------------------------------------------

UTemporalRewindSubsystem* UTemporalScrubWidget::GetSubsystem()
{
	if (!CachedSubsystem)
	{
		if (const UWorld* World = GetWorld())
		{
			CachedSubsystem = World->GetSubsystem<UTemporalRewindSubsystem>();
		}
	}

	return CachedSubsystem;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void UTemporalScrubWidget::ShowScrubWidget()
{
	if (!bIsBound)
	{
		BindToSubsystem();
	}

	SetVisibility(ESlateVisibility::Visible);

	RefreshSliderRange();
	UpdateStateDisplay();
	UpdateButtonStates();
	UpdateSpeedDisplay();

	if (UTemporalRewindSubsystem* Sub = GetSubsystem())
	{
		UpdateTimeDisplay(Sub->GetScrubTimestamp());
	}

	if (Anim_ShowPanel)
	{
		PlayAnimation(Anim_ShowPanel);
	}
}

void UTemporalScrubWidget::HideScrubWidget()
{
	if (Anim_HidePanel)
	{
		PlayAnimation(Anim_HidePanel);
		// Visibility is left to the animation; bind an OnAnimationFinished event
		// in your Widget Blueprint if you want the panel to collapse after fade-out.
	}
	else
	{
		SetVisibility(ESlateVisibility::Collapsed);
	}
}

bool UTemporalScrubWidget::IsScrubWidgetVisible() const
{
	return IsVisible() && bIsBound;
}

// ---------------------------------------------------------------------------
// Delegate binding
// ---------------------------------------------------------------------------

void UTemporalScrubWidget::BindToSubsystem()
{
	if (bIsBound)
	{
		return;
	}

	UTemporalRewindSubsystem* Sub = GetSubsystem();
	if (!Sub)
	{
		return;
	}

	Sub->OnRewindStarted.AddDynamic(this,   &UTemporalScrubWidget::HandleRewindStarted);
	Sub->OnScrubUpdated.AddDynamic(this,    &UTemporalScrubWidget::HandleScrubUpdated);
	Sub->OnRewindPaused.AddDynamic(this,    &UTemporalScrubWidget::HandleRewindPaused);
	Sub->OnRewindResumed.AddDynamic(this,   &UTemporalScrubWidget::HandleRewindResumed);
	Sub->OnRewindCommitted.AddDynamic(this, &UTemporalScrubWidget::HandleRewindCommitted);
	Sub->OnRewindCancelled.AddDynamic(this, &UTemporalScrubWidget::HandleRewindCancelled);
	Sub->OnCooldownStarted.AddDynamic(this, &UTemporalScrubWidget::HandleCooldownStarted);
	Sub->OnCooldownEnded.AddDynamic(this,   &UTemporalScrubWidget::HandleCooldownEnded);

	bIsBound = true;
}

void UTemporalScrubWidget::UnbindFromSubsystem()
{
	if (!bIsBound)
	{
		return;
	}

	if (UTemporalRewindSubsystem* Sub = GetSubsystem())
	{
		Sub->OnRewindStarted.RemoveDynamic(this,   &UTemporalScrubWidget::HandleRewindStarted);
		Sub->OnScrubUpdated.RemoveDynamic(this,    &UTemporalScrubWidget::HandleScrubUpdated);
		Sub->OnRewindPaused.RemoveDynamic(this,    &UTemporalScrubWidget::HandleRewindPaused);
		Sub->OnRewindResumed.RemoveDynamic(this,   &UTemporalScrubWidget::HandleRewindResumed);
		Sub->OnRewindCommitted.RemoveDynamic(this, &UTemporalScrubWidget::HandleRewindCommitted);
		Sub->OnRewindCancelled.RemoveDynamic(this, &UTemporalScrubWidget::HandleRewindCancelled);
		Sub->OnCooldownStarted.RemoveDynamic(this, &UTemporalScrubWidget::HandleCooldownStarted);
		Sub->OnCooldownEnded.RemoveDynamic(this,   &UTemporalScrubWidget::HandleCooldownEnded);
	}

	bIsBound = false;
}

// ---------------------------------------------------------------------------
// Slider callbacks
// ---------------------------------------------------------------------------

void UTemporalScrubWidget::OnSliderValueChanged(float Value)
{
	// Ignore programmatic changes (e.g. from HandleScrubUpdated); only act on user drag.
	if (!bIsDragging)
	{
		return;
	}

	const float Timestamp = SliderValueToTimestamp(Value);
	UpdateTimeDisplay(Timestamp);

	if (UTemporalRewindSubsystem* Sub = GetSubsystem())
	{
		Sub->ScrubTo(Timestamp);
	}
}

void UTemporalScrubWidget::OnSliderMouseCaptureBegin()
{
	// Flag as dragging so HandleScrubUpdated stops fighting the slider position.
	bIsDragging = true;
}

void UTemporalScrubWidget::OnSliderMouseCaptureEnd()
{
	bIsDragging = false;
	// State is now Scrubbing; refresh buttons so Rewind becomes available.
	UpdateButtonStates();
}

// ---------------------------------------------------------------------------
// VCR button callbacks
// ---------------------------------------------------------------------------

void UTemporalScrubWidget::OnRewindClicked()
{
	if (UTemporalRewindSubsystem* Sub = GetSubsystem())
	{
		Sub->ResumeRewind();
	}
}

void UTemporalScrubWidget::OnPauseClicked()
{
	if (UTemporalRewindSubsystem* Sub = GetSubsystem())
	{
		Sub->PauseRewind();
	}
}

void UTemporalScrubWidget::OnCommitClicked()
{
	if (UTemporalRewindSubsystem* Sub = GetSubsystem())
	{
		Sub->Commit();
	}
}

void UTemporalScrubWidget::OnCancelClicked()
{
	if (UTemporalRewindSubsystem* Sub = GetSubsystem())
	{
		Sub->Cancel();
	}
}

// ---------------------------------------------------------------------------
// Speed button callbacks
// ---------------------------------------------------------------------------

void UTemporalScrubWidget::OnSpeed025Clicked()
{
	if (UTemporalRewindSubsystem* Sub = GetSubsystem()) { Sub->SetRewindSpeed(0.25f); }
	UpdateSpeedDisplay();
}

void UTemporalScrubWidget::OnSpeed05Clicked()
{
	if (UTemporalRewindSubsystem* Sub = GetSubsystem()) { Sub->SetRewindSpeed(0.5f); }
	UpdateSpeedDisplay();
}

void UTemporalScrubWidget::OnSpeed1Clicked()
{
	if (UTemporalRewindSubsystem* Sub = GetSubsystem()) { Sub->SetRewindSpeed(1.0f); }
	UpdateSpeedDisplay();
}

void UTemporalScrubWidget::OnSpeed2Clicked()
{
	if (UTemporalRewindSubsystem* Sub = GetSubsystem()) { Sub->SetRewindSpeed(2.0f); }
	UpdateSpeedDisplay();
}

// ---------------------------------------------------------------------------
// Subsystem delegate handlers
// ---------------------------------------------------------------------------

void UTemporalScrubWidget::HandleRewindStarted()
{
	ShowScrubWidget();
}

void UTemporalScrubWidget::HandleScrubUpdated(float Timestamp)
{
	UpdateTimeDisplay(Timestamp);

	// Only move the slider if the user is not dragging it.
	if (!bIsDragging && Slider_Scrub)
	{
		Slider_Scrub->SetValue(TimestampToSliderValue(Timestamp));
	}
}

void UTemporalScrubWidget::HandleRewindPaused()
{
	UpdateStateDisplay();
	UpdateButtonStates();
}

void UTemporalScrubWidget::HandleRewindResumed()
{
	UpdateStateDisplay();
	UpdateButtonStates();
}

void UTemporalScrubWidget::HandleRewindCommitted()
{
	HideScrubWidget();
}

void UTemporalScrubWidget::HandleRewindCancelled()
{
	HideScrubWidget();
}

void UTemporalScrubWidget::HandleCooldownStarted(float Duration)
{
	CooldownTotal     = Duration;
	CooldownRemaining = Duration;
	UpdateStateDisplay();
}

void UTemporalScrubWidget::HandleCooldownEnded()
{
	CooldownTotal     = 0.f;
	CooldownRemaining = 0.f;

	if (ProgressBar_Cooldown)
	{
		ProgressBar_Cooldown->SetPercent(0.f);
	}
}

// ---------------------------------------------------------------------------
// Display helpers
// ---------------------------------------------------------------------------

void UTemporalScrubWidget::RefreshSliderRange()
{
	UTemporalRewindSubsystem* Sub = GetSubsystem();
	if (!Sub)
	{
		return;
	}

	CachedOldestTimestamp = Sub->GetOldestAllowedTimestamp();
	CachedNewestTimestamp = Sub->GetOriginalPresentTimestamp();

	if (Text_Range)
	{
		const float Window = CachedNewestTimestamp - CachedOldestTimestamp;
		Text_Range->SetText(FText::FromString(FString::Printf(TEXT("%.2fs window"), Window)));
	}
}

void UTemporalScrubWidget::UpdateTimeDisplay(float Timestamp)
{
	if (Text_Time)
	{
		Text_Time->SetText(FormatTimestamp(Timestamp));
	}
}

void UTemporalScrubWidget::UpdateStateDisplay()
{
	if (!Text_State)
	{
		return;
	}

	const UTemporalRewindSubsystem* Sub = GetSubsystem();
	if (!Sub)
	{
		return;
	}

	FText StateText;

	switch (Sub->GetCurrentState())
	{
	case ETemporalSystemState::Rewinding: StateText = FText::FromString(TEXT("REWINDING")); break;
	case ETemporalSystemState::Paused:    StateText = FText::FromString(TEXT("PAUSED"));    break;
	case ETemporalSystemState::Scrubbing: StateText = FText::FromString(TEXT("SCRUBBING")); break;
	case ETemporalSystemState::Cooldown:  StateText = FText::FromString(TEXT("COOLDOWN"));  break;
	default:                              StateText = FText::GetEmpty();                     break;
	}

	Text_State->SetText(StateText);
}

void UTemporalScrubWidget::UpdateButtonStates()
{
	const UTemporalRewindSubsystem* Sub = GetSubsystem();
	if (!Sub)
	{
		return;
	}

	const ETemporalSystemState State = Sub->GetCurrentState();
	const bool bRewindActive = Sub->IsRewindActive();
	const bool bIsRewinding  = (State == ETemporalSystemState::Rewinding);
	const bool bCanResume    = (State == ETemporalSystemState::Paused || State == ETemporalSystemState::Scrubbing);

	if (Button_Rewind) Button_Rewind->SetIsEnabled(bCanResume);
	if (Button_Pause)  Button_Pause->SetIsEnabled(bIsRewinding);
	if (Button_Commit) Button_Commit->SetIsEnabled(bRewindActive);
	if (Button_Cancel) Button_Cancel->SetIsEnabled(bRewindActive);
}

void UTemporalScrubWidget::UpdateSpeedDisplay()
{
	if (!Text_Speed)
	{
		return;
	}

	const UTemporalRewindSubsystem* Sub = GetSubsystem();
	if (!Sub)
	{
		return;
	}

	Text_Speed->SetText(FText::FromString(
		FString::Printf(TEXT("%.2fx"), Sub->GetRewindSpeed())));
}

float UTemporalScrubWidget::TimestampToSliderValue(float Timestamp) const
{
	const float Range = CachedNewestTimestamp - CachedOldestTimestamp;
	if (Range < KINDA_SMALL_NUMBER)
	{
		return 0.f;
	}

	return FMath::Clamp((Timestamp - CachedOldestTimestamp) / Range, 0.f, 1.f);
}

float UTemporalScrubWidget::SliderValueToTimestamp(float SliderValue) const
{
	return FMath::Lerp(CachedOldestTimestamp, CachedNewestTimestamp,
		FMath::Clamp(SliderValue, 0.f, 1.f));
}

FText UTemporalScrubWidget::FormatTimestamp(float Seconds)
{
	return FText::FromString(FString::Printf(TEXT("%.2fs"), Seconds));
}
