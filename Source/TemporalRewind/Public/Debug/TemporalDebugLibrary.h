// Copyright (c) Temporal Rewind System. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TemporalDebugLibrary.generated.h"

class UTemporalRewindSubsystem;
class UActorSnapshotTimeline;

/**
 * Static debug utilities for the Temporal Rewind System.
 *
 * All functions take a WorldContextObject so they work from any Blueprint
 * or C++ context — game mode, actor, widget, etc.
 *
 * Console commands (usable in PIE Output Log or ~ console):
 *   TemporalRewind.PrintStats        — full stats dump to Output Log
 *   TemporalRewind.PrintState        — current state machine state only
 */
UCLASS()
class TEMPORALREWIND_API UTemporalDebugLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	// --- Memory --------------------------------------------------------------

	/** Total bytes across all active and orphaned timelines (fixed buffers + blob heap). */
	UFUNCTION(BlueprintPure, Category = "Temporal Rewind|Debug", meta = (WorldContext = "WorldContextObject"))
	static int64 GetTotalMemoryUsageBytes(const UObject* WorldContextObject);

	/** Bytes used by one actor's timeline. Checks orphaned map too. Returns 0 if GUID not found. */
	UFUNCTION(BlueprintPure, Category = "Temporal Rewind|Debug", meta = (WorldContext = "WorldContextObject"))
	static int64 GetMemoryUsageForActor(const UObject* WorldContextObject, FGuid ActorGuid);

	// --- Duration ------------------------------------------------------------

	/** Longest recorded duration across all active timelines in seconds. */
	UFUNCTION(BlueprintPure, Category = "Temporal Rewind|Debug", meta = (WorldContext = "WorldContextObject"))
	static float GetRecordedDuration(const UObject* WorldContextObject);

	/** Recorded duration for one actor in seconds. Returns 0 if GUID not found. */
	UFUNCTION(BlueprintPure, Category = "Temporal Rewind|Debug", meta = (WorldContext = "WorldContextObject"))
	static float GetRecordedDurationForActor(const UObject* WorldContextObject, FGuid ActorGuid);

	/** How far back the player can actually rewind right now: min(MaxScrubRange, RecordedDuration). */
	UFUNCTION(BlueprintPure, Category = "Temporal Rewind|Debug", meta = (WorldContext = "WorldContextObject"))
	static float GetPlayableRewindDuration(const UObject* WorldContextObject);

	// --- Snapshot counts -----------------------------------------------------

	/** Total snapshot count across all active and orphaned timelines. */
	UFUNCTION(BlueprintPure, Category = "Temporal Rewind|Debug", meta = (WorldContext = "WorldContextObject"))
	static int32 GetTotalSnapshotCount(const UObject* WorldContextObject);

	/** Snapshot count for one actor. Returns 0 if GUID not found. */
	UFUNCTION(BlueprintPure, Category = "Temporal Rewind|Debug", meta = (WorldContext = "WorldContextObject"))
	static int32 GetSnapshotCountForActor(const UObject* WorldContextObject, FGuid ActorGuid);

	// --- Actor registry ------------------------------------------------------

	/** Number of currently registered (live) actors. */
	UFUNCTION(BlueprintPure, Category = "Temporal Rewind|Debug", meta = (WorldContext = "WorldContextObject"))
	static int32 GetRegisteredActorCount(const UObject* WorldContextObject);

	/** Number of orphaned timelines (dead actors with retained data). */
	UFUNCTION(BlueprintPure, Category = "Temporal Rewind|Debug", meta = (WorldContext = "WorldContextObject"))
	static int32 GetOrphanedTimelineCount(const UObject* WorldContextObject);

	// --- State ---------------------------------------------------------------

	/** Current state machine state as a readable string (e.g. "Recording", "Rewinding"). */
	UFUNCTION(BlueprintPure, Category = "Temporal Rewind|Debug", meta = (WorldContext = "WorldContextObject"))
	static FString GetSystemStateString(const UObject* WorldContextObject);

	// --- Log dump ------------------------------------------------------------

	/** Prints a full stats table to Output Log. Also callable via console: TemporalRewind.PrintStats */
	UFUNCTION(BlueprintCallable, Category = "Temporal Rewind|Debug", meta = (WorldContext = "WorldContextObject"))
	static void PrintStatsToLog(const UObject* WorldContextObject);

private:
	static UTemporalRewindSubsystem* GetSubsystem(const UObject* WorldContextObject);

	// Internal helpers — use friend access directly on the two private classes.
	static int64 CalcTimelineMemory(const UActorSnapshotTimeline* Timeline);
	static float CalcTimelineDuration(const UActorSnapshotTimeline* Timeline);
};
