// Copyright (c) Temporal Rewind System. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "TemporalDebugLibrary.generated.h"

class UActorSnapshotTimeline;
class UTemporalRewindSubsystem;

/**
 * Static debug utilities for the Temporal Rewind System.
 *
 * Console commands:
 *   TemporalRewind.PrintStats   — full stats dump
 *   TemporalRewind.PrintState   — current state only
 */
UCLASS()
class TEMPORALREWIND_API UTemporalDebugLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	// --- Actor Registry --------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "Temporal Rewind|Debug",
		meta = (WorldContext = "WorldContextObject"))
	static int32 GetOrphanedTimelineCount(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "Temporal Rewind|Debug",
		meta = (WorldContext = "WorldContextObject"))
	static int32 GetRegisteredActorCount(const UObject* WorldContextObject);

	// --- Duration --------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "Temporal Rewind|Debug",
		meta = (WorldContext = "WorldContextObject"))
	static float GetPlayableRewindDuration(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "Temporal Rewind|Debug",
		meta = (WorldContext = "WorldContextObject"))
	static float GetRecordedDuration(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "Temporal Rewind|Debug",
		meta = (WorldContext = "WorldContextObject"))
	static float GetRecordedDurationForActor(const UObject* WorldContextObject, FGuid ActorGuid);

	// --- Memory ----------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "Temporal Rewind|Debug",
		meta = (WorldContext = "WorldContextObject"))
	static int64 GetMemoryUsageForActor(const UObject* WorldContextObject, FGuid ActorGuid);

	UFUNCTION(BlueprintPure, Category = "Temporal Rewind|Debug",
		meta = (WorldContext = "WorldContextObject"))
	static int64 GetTotalMemoryUsageBytes(const UObject* WorldContextObject);

	// --- Snapshots -------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "Temporal Rewind|Debug",
		meta = (WorldContext = "WorldContextObject"))
	static int32 GetSnapshotCountForActor(const UObject* WorldContextObject, FGuid ActorGuid);

	UFUNCTION(BlueprintPure, Category = "Temporal Rewind|Debug",
		meta = (WorldContext = "WorldContextObject"))
	static int32 GetTotalSnapshotCount(const UObject* WorldContextObject);

	// --- State -----------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "Temporal Rewind|Debug",
		meta = (WorldContext = "WorldContextObject"))
	static FString GetSystemStateString(const UObject* WorldContextObject);

	// --- Log Dump --------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "Temporal Rewind|Debug",
		meta = (WorldContext = "WorldContextObject"))
	static void PrintStatsToLog(const UObject* WorldContextObject);

private:

	static float CalcTimelineDuration(const UActorSnapshotTimeline* Timeline);

	static int64 CalcTimelineMemory(const UActorSnapshotTimeline* Timeline);

	static UTemporalRewindSubsystem* GetSubsystem(const UObject* WorldContextObject);
};