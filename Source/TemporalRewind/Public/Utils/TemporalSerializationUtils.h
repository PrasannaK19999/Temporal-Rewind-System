// Copyright (c) 2026 Prasanna Keerthivasan. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Data/TemporalSnapshot.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "TemporalSerializationUtils.generated.h"

class USkeletalMeshComponent;

/**
 * Blueprint helpers for packing common types into FTemporalSnapshot.StateBlob.
 *
 * Read-order rule: reads must match writes in the same order.
 * Order errors cause silent data corruption.
 */
UCLASS()
class TEMPORALREWIND_API UTemporalSerializationUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintPure, Category = "Temporal Rewind|Serialization")
	static int32 BeginRead(const FTemporalSnapshot& Snapshot);

	// --- Writers ---------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "Temporal Rewind|Serialization")
	static void WriteBool(UPARAM(ref) FTemporalSnapshot& Snapshot, bool Value);

	UFUNCTION(BlueprintCallable, Category = "Temporal Rewind|Serialization")
	static void WriteFloat(UPARAM(ref) FTemporalSnapshot& Snapshot, float Value);

	UFUNCTION(BlueprintCallable, Category = "Temporal Rewind|Serialization")
	static void WriteInt32(UPARAM(ref) FTemporalSnapshot& Snapshot, int32 Value);

	UFUNCTION(BlueprintCallable, Category = "Temporal Rewind|Serialization")
	static bool WritePoseSnapshot(UPARAM(ref) FTemporalSnapshot& Snapshot,
		USkeletalMeshComponent* MeshComponent);

	UFUNCTION(BlueprintCallable, Category = "Temporal Rewind|Serialization")
	static void WriteRotator(UPARAM(ref) FTemporalSnapshot& Snapshot, FRotator Value);

	UFUNCTION(BlueprintCallable, Category = "Temporal Rewind|Serialization")
	static void WriteTransform(UPARAM(ref) FTemporalSnapshot& Snapshot, FTransform Value);

	UFUNCTION(BlueprintCallable, Category = "Temporal Rewind|Serialization")
	static void WriteVector(UPARAM(ref) FTemporalSnapshot& Snapshot, FVector Value);

	// --- Readers ---------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "Temporal Rewind|Serialization")
	static bool ReadBool(const FTemporalSnapshot& Snapshot, UPARAM(ref) int32& ByteOffset);

	UFUNCTION(BlueprintCallable, Category = "Temporal Rewind|Serialization")
	static float ReadFloat(const FTemporalSnapshot& Snapshot, UPARAM(ref) int32& ByteOffset);

	UFUNCTION(BlueprintCallable, Category = "Temporal Rewind|Serialization")
	static int32 ReadInt32(const FTemporalSnapshot& Snapshot, UPARAM(ref) int32& ByteOffset);

	UFUNCTION(BlueprintCallable, Category = "Temporal Rewind|Serialization")
	static bool ReadPoseSnapshot(const FTemporalSnapshot& Snapshot,
		UPARAM(ref) int32& ByteOffset,
		USkeletalMeshComponent* MeshComponent,
		FName PoseSnapshotName = TEXT("TemporalRewindPose"));

	UFUNCTION(BlueprintCallable, Category = "Temporal Rewind|Serialization")
	static FRotator ReadRotator(const FTemporalSnapshot& Snapshot, UPARAM(ref) int32& ByteOffset);

	UFUNCTION(BlueprintCallable, Category = "Temporal Rewind|Serialization")
	static FTransform ReadTransform(const FTemporalSnapshot& Snapshot, UPARAM(ref) int32& ByteOffset);

	UFUNCTION(BlueprintCallable, Category = "Temporal Rewind|Serialization")
	static FVector ReadVector(const FTemporalSnapshot& Snapshot, UPARAM(ref) int32& ByteOffset);
};