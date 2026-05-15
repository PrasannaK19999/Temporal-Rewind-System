// Copyright (c) 2026 Prasanna Keerthivasan. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Data/RewindableActorEntry.h"
#include "Data/TemporalEnums.h"
#include "Data/TemporalSnapshot.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/ScriptInterface.h"

#include "TemporalRewindSubsystem.generated.h"

class IRewindable;
class UActorSnapshotTimeline;
class URecordingStrategy;
class URewindSessionContext;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FTemporalRewindSimpleDelegate);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTemporalRewindDurationDelegate, float, Duration);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTemporalRewindGuidDelegate, FGuid, ActorGuid);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTemporalRewindTimestampDelegate, float, Timestamp);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FTemporalRewindOrphanSnapshotDelegate, FGuid, ActorGuid, FTemporalSnapshot, Snapshot);

/**
 * World-level temporal state orchestrator.
 * One per UWorld, created and torn down by the engine.
 */
UCLASS()
class TEMPORALREWIND_API UTemporalRewindSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:

	//~ Begin USubsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~ End USubsystem

	//~ Begin FTickableGameObject
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override { return bTickEnabled; }
	virtual void Tick(float DeltaTime) override;
	//~ End FTickableGameObject

	// --- Configuration ---------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "Temporal Rewind|Config")
	void SetMaxScrubRange(float NewRange);

	UFUNCTION(BlueprintCallable, Category = "Temporal Rewind|Config")
	void SetRecordingStrategy(URecordingStrategy* NewStrategy);

	// --- Orphan Management -----------------------------------------------

	UFUNCTION(BlueprintPure, Category = "Temporal Rewind|Orphans")
	TArray<FGuid> GetOrphanedTimelineGuids() const;

	UFUNCTION(BlueprintCallable, Category = "Temporal Rewind|Orphans")
	void PurgeAllOrphanedTimelines();

	UFUNCTION(BlueprintCallable, Category = "Temporal Rewind|Orphans")
	void PurgeOrphanedTimeline(FGuid ActorGuid);

	// --- Recording -------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "Temporal Rewind|Recording")
	void StartRecording();

	UFUNCTION(BlueprintCallable, Category = "Temporal Rewind|Recording")
	void StopRecording();

	// --- Registration ----------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "Temporal Rewind|Registration")
	void RegisterRewindable(TScriptInterface<IRewindable> Rewindable);

	UFUNCTION(BlueprintCallable, Category = "Temporal Rewind|Registration")
	void UnregisterRewindable(TScriptInterface<IRewindable> Rewindable);

	// --- Rewind ----------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "Temporal Rewind|Rewind")
	void Cancel();

	UFUNCTION(BlueprintCallable, Category = "Temporal Rewind|Rewind")
	void Commit();

	UFUNCTION(BlueprintPure, Category = "Temporal Rewind|Rewind")
	float GetScrubTimestamp() const;

	UFUNCTION(BlueprintCallable, Category = "Temporal Rewind|Rewind")
	void PauseRewind();

	UFUNCTION(BlueprintCallable, Category = "Temporal Rewind|Rewind")
	void ResumeRewind();

	UFUNCTION(BlueprintCallable, Category = "Temporal Rewind|Rewind")
	void ScrubTo(float Timestamp);

	UFUNCTION(BlueprintCallable, Category = "Temporal Rewind|Rewind")
	void StartRewind();

	/** Returns the oldest timestamp the active session can scrub to. 0 if no session. */
	UFUNCTION(BlueprintPure, Category = "Temporal Rewind|Rewind")
	float GetOldestAllowedTimestamp() const;

	/** Returns the present-moment timestamp the session started from. 0 if no session. */
	UFUNCTION(BlueprintPure, Category = "Temporal Rewind|Rewind")
	float GetOriginalPresentTimestamp() const;

	/** Returns seconds remaining in the active cooldown. 0 if not in cooldown. */
	UFUNCTION(BlueprintPure, Category = "Temporal Rewind|State")
	float GetCooldownTimeRemaining() const { return CooldownTimeRemaining; }

	/** Returns the configured cooldown duration in seconds. */
	UFUNCTION(BlueprintPure, Category = "Temporal Rewind|Config")
	float GetCooldownDuration() const { return CooldownDuration; }

	/** Returns the current rewind playback speed multiplier. */
	UFUNCTION(BlueprintPure, Category = "Temporal Rewind|Config")
	float GetRewindSpeed() const { return RewindSpeed; }

	/** Sets the rewind playback speed. Also updates the active session if one is running. */
	UFUNCTION(BlueprintCallable, Category = "Temporal Rewind|Config",
		meta = (ClampMin = "0.01"))
	void SetRewindSpeed(float NewSpeed);

	// --- State -----------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "Temporal Rewind|State")
	ETemporalSystemState GetCurrentState() const { return CurrentState; }

	UFUNCTION(BlueprintPure, Category = "Temporal Rewind|State")
	bool IsRecording() const { return CurrentState == ETemporalSystemState::Recording; }

	UFUNCTION(BlueprintPure, Category = "Temporal Rewind|State")
	bool IsRewindActive() const;

	// --- Delegates -------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "Temporal Rewind|Events")
	FTemporalRewindGuidDelegate OnActorRegistered;

	UPROPERTY(BlueprintAssignable, Category = "Temporal Rewind|Events")
	FTemporalRewindGuidDelegate OnActorUnregistered;

	UPROPERTY(BlueprintAssignable, Category = "Temporal Rewind|Events")
	FTemporalRewindSimpleDelegate OnCooldownEnded;

	UPROPERTY(BlueprintAssignable, Category = "Temporal Rewind|Events")
	FTemporalRewindDurationDelegate OnCooldownStarted;

	UPROPERTY(BlueprintAssignable, Category = "Temporal Rewind|Events")
	FTemporalRewindOrphanSnapshotDelegate OnOrphanedSnapshotReached;

	UPROPERTY(BlueprintAssignable, Category = "Temporal Rewind|Events")
	FTemporalRewindSimpleDelegate OnRewindCancelled;

	UPROPERTY(BlueprintAssignable, Category = "Temporal Rewind|Events")
	FTemporalRewindSimpleDelegate OnRewindCommitted;

	UPROPERTY(BlueprintAssignable, Category = "Temporal Rewind|Events")
	FTemporalRewindSimpleDelegate OnRewindPaused;

	UPROPERTY(BlueprintAssignable, Category = "Temporal Rewind|Events")
	FTemporalRewindSimpleDelegate OnRewindResumed;

	UPROPERTY(BlueprintAssignable, Category = "Temporal Rewind|Events")
	FTemporalRewindSimpleDelegate OnRewindStarted;

	UPROPERTY(BlueprintAssignable, Category = "Temporal Rewind|Events")
	FTemporalRewindTimestampDelegate OnScrubUpdated;

protected:

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Temporal Rewind|Config",
		meta = (ClampMin = "0.1", UIMin = "0.1", ForceUnits = "s"))
	float BufferDuration = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Temporal Rewind|Config",
		meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "s"))
	float CooldownDuration = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Temporal Rewind|Config",
		meta = (ClampMin = "0.1", UIMin = "0.1", ForceUnits = "s"))
	float MaxScrubRange = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Temporal Rewind|Config",
		meta = (ClampMin = "0.1", UIMin = "0.1"))
	float RewindSpeed = 1.0f;

private:

	friend class UTemporalDebugLibrary;

	void ApplyStateAtScrubTimestamp();

	void BeginCooldown();

	void CleanupStaleActors();

	int32 ComputeTimelineCapacity() const;

	void TickCooldown(float DeltaTime);

	void TickPaused(float DeltaTime);

	void TickRecording(float DeltaTime);

	void TickRewinding(float DeltaTime);

	void TickScrubbing(float DeltaTime);

	bool TryTransitionTo(ETemporalSystemState NewState);

	UPROPERTY()
	TMap<FGuid, TObjectPtr<UActorSnapshotTimeline>> ActorTimelines;

	UPROPERTY()
	TObjectPtr<URecordingStrategy> ActiveRecordingStrategy = nullptr;

	UPROPERTY()
	TObjectPtr<URewindSessionContext> ActiveSessionContext = nullptr;

	bool bTickEnabled = true;

	float CooldownTimeRemaining = 0.0f;

	UPROPERTY()
	ETemporalSystemState CurrentState = ETemporalSystemState::Idle;

	EOrphanedTimelinePolicy DefaultOrphanPolicy = EOrphanedTimelinePolicy::KeepUntilOverwritten;

	UPROPERTY()
	TMap<FGuid, TObjectPtr<UActorSnapshotTimeline>> OrphanedTimelines;

	UPROPERTY()
	TMap<FGuid, FRewindableActorEntry> RegisteredActors;
};