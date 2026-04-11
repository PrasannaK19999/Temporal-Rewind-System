// Copyright (c) Temporal Rewind System. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/ScriptInterface.h"
#include "Data/TemporalEnums.h"
#include "Data/TemporalSnapshot.h"
#include "Data/RewindableActorEntry.h"
#include "TemporalRewindSubsystem.generated.h"

class UActorSnapshotTimeline;
class URecordingStrategy;
class URewindSessionContext;
class IRewindable;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FTemporalRewindSimpleDelegate);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTemporalRewindTimestampDelegate, float, Timestamp);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTemporalRewindDurationDelegate, float, Duration);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTemporalRewindGuidDelegate, FGuid, ActorGuid);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FTemporalRewindOrphanSnapshotDelegate, FGuid, ActorGuid, FTemporalSnapshot, Snapshot);

/**
 * World-level temporal state orchestrator. Owns the state machine, actor
 * registry, timelines, recording strategy, and active rewind session.
 * One per UWorld; created and torn down by the engine.
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
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override { return bTickEnabled; }
	//~ End FTickableGameObject

	// --- State queries ---------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "Temporal Rewind|State")
	ETemporalSystemState GetCurrentState() const { return CurrentState; }

	UFUNCTION(BlueprintPure, Category = "Temporal Rewind|State")
	bool IsRewindActive() const;

	UFUNCTION(BlueprintPure, Category = "Temporal Rewind|State")
	bool IsRecording() const { return CurrentState == ETemporalSystemState::Recording; }

	// --- Recording API ---------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "Temporal Rewind|Recording")
	void StartRecording();

	UFUNCTION(BlueprintCallable, Category = "Temporal Rewind|Recording")
	void StopRecording();

	UFUNCTION(BlueprintCallable, Category = "Temporal Rewind|Registration")
	void RegisterRewindable(TScriptInterface<IRewindable> Rewindable);

	UFUNCTION(BlueprintCallable, Category = "Temporal Rewind|Registration")
	void UnregisterRewindable(TScriptInterface<IRewindable> Rewindable);

	// --- Rewind API (stubbed � implemented in 8c) ------------------------

	UFUNCTION(BlueprintCallable, Category = "Temporal Rewind|Rewind")
	void StartRewind();

	UFUNCTION(BlueprintCallable, Category = "Temporal Rewind|Rewind")
	void PauseRewind();

	UFUNCTION(BlueprintCallable, Category = "Temporal Rewind|Rewind")
	void ResumeRewind();

	UFUNCTION(BlueprintCallable, Category = "Temporal Rewind|Rewind")
	void ScrubTo(float Timestamp);

	UFUNCTION(BlueprintCallable, Category = "Temporal Rewind|Rewind")
	void Commit();

	UFUNCTION(BlueprintCallable, Category = "Temporal Rewind|Rewind")
	void Cancel();

	UFUNCTION(BlueprintPure, Category = "Temporal Rewind|Rewind")
	float GetScrubTimestamp() const;

	// --- Configuration ---------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "Temporal Rewind|Config")
	void SetMaxScrubRange(float NewRange);

	UFUNCTION(BlueprintCallable, Category = "Temporal Rewind|Config")
	void SetRecordingStrategy(URecordingStrategy* NewStrategy);

	// --- Orphan management -----------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "Temporal Rewind|Orphans")
	void PurgeOrphanedTimeline(FGuid ActorGuid);

	UFUNCTION(BlueprintCallable, Category = "Temporal Rewind|Orphans")
	void PurgeAllOrphanedTimelines();

	UFUNCTION(BlueprintPure, Category = "Temporal Rewind|Orphans")
	TArray<FGuid> GetOrphanedTimelineGuids() const;

	// --- Delegates -------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "Temporal Rewind|Events")
	FTemporalRewindSimpleDelegate OnRewindStarted;

	UPROPERTY(BlueprintAssignable, Category = "Temporal Rewind|Events")
	FTemporalRewindSimpleDelegate OnRewindPaused;

	UPROPERTY(BlueprintAssignable, Category = "Temporal Rewind|Events")
	FTemporalRewindSimpleDelegate OnRewindResumed;

	UPROPERTY(BlueprintAssignable, Category = "Temporal Rewind|Events")
	FTemporalRewindTimestampDelegate OnScrubUpdated;

	UPROPERTY(BlueprintAssignable, Category = "Temporal Rewind|Events")
	FTemporalRewindSimpleDelegate OnRewindCommitted;

	UPROPERTY(BlueprintAssignable, Category = "Temporal Rewind|Events")
	FTemporalRewindSimpleDelegate OnRewindCancelled;

	UPROPERTY(BlueprintAssignable, Category = "Temporal Rewind|Events")
	FTemporalRewindDurationDelegate OnCooldownStarted;

	UPROPERTY(BlueprintAssignable, Category = "Temporal Rewind|Events")
	FTemporalRewindSimpleDelegate OnCooldownEnded;

	UPROPERTY(BlueprintAssignable, Category = "Temporal Rewind|Events")
	FTemporalRewindGuidDelegate OnActorRegistered;

	UPROPERTY(BlueprintAssignable, Category = "Temporal Rewind|Events")
	FTemporalRewindGuidDelegate OnActorUnregistered;

	UPROPERTY(BlueprintAssignable, Category = "Temporal Rewind|Events")
	FTemporalRewindOrphanSnapshotDelegate OnOrphanedSnapshotReached;

protected:
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Temporal Rewind|Config",
		meta = (ClampMin = "0.1", UIMin = "0.1", ForceUnits = "s"))
	float BufferDuration = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Temporal Rewind|Config",
		meta = (ClampMin = "0.1", UIMin = "0.1", ForceUnits = "s"))
	float MaxScrubRange = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Temporal Rewind|Config",
		meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "s"))
	float CooldownDuration = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Temporal Rewind|Config",
		meta = (ClampMin = "0.1", UIMin = "0.1"))
	float RewindSpeed = 1.0f;

private:
	friend class UTemporalDebugLibrary;

	/** Validate and execute a state transition. Single source of truth for rules. */
	bool TryTransitionTo(ETemporalSystemState NewState);

	/** Per-state tick handlers. */
	void TickRecording(float DeltaTime);
	void TickRewinding(float DeltaTime);
	void TickScrubbing(float DeltaTime);
	void TickPaused(float DeltaTime);
	void TickCooldown(float DeltaTime);

	/** Iterate every registered actor and every orphan; apply the snapshot nearest the scrub timestamp. */
	void ApplyStateAtScrubTimestamp();

	/** Enter cooldown state with the configured duration; fires OnCooldownStarted. */
	void BeginCooldown();

	/** Called from TickRecording. Sweeps dead weak refs, applies orphan policy. */
	void CleanupStaleActors();

	/** Compute ring buffer capacity for a new timeline. */
	int32 ComputeTimelineCapacity() const;

	// --- State -----------------------------------------------------------

	UPROPERTY()
	ETemporalSystemState CurrentState = ETemporalSystemState::Idle;

	bool bTickEnabled = true;

	UPROPERTY()
	TMap<FGuid, FRewindableActorEntry> RegisteredActors;

	UPROPERTY()
	TMap<FGuid, TObjectPtr<UActorSnapshotTimeline>> ActorTimelines;

	UPROPERTY()
	TMap<FGuid, TObjectPtr<UActorSnapshotTimeline>> OrphanedTimelines;

	UPROPERTY()
	TObjectPtr<URewindSessionContext> ActiveSessionContext = nullptr;

	UPROPERTY()
	TObjectPtr<URecordingStrategy> ActiveRecordingStrategy = nullptr;

	float CooldownTimeRemaining = 0.0f;

	/** Project-default orphan policy, read from UTemporalRewindSettings on Initialize(). */
	EOrphanedTimelinePolicy DefaultOrphanPolicy = EOrphanedTimelinePolicy::KeepUntilOverwritten;
};