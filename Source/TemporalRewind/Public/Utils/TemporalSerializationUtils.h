// Copyright (c) Temporal Rewind System. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Data/TemporalSnapshot.h"
#include "TemporalSerializationUtils.generated.h"

/**
 * Blueprint helpers for packing common types into FTemporalSnapshot.StateBlob
 * and reading them back.
 *
 * CRITICAL READ-ORDER RULE:
 * Reads must occur in the same order as writes. The ByteOffset parameter is
 * advanced in place to walk through the blob. Adding a field to CaptureState
 * MUST be matched by adding the corresponding read in ApplyState in the same
 * position. There is no compile-time check — order errors cause silent data
 * corruption.
 *
 * This is the documented footgun of the Free tier. Pro tier eliminates it via
 * reflection-driven serialization (UAutoSnapshotComponent).
 */
UCLASS()
class TEMPORALREWIND_API UTemporalSerializationUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Returns 0. Use at the start of ApplyState before any Read* calls. */
	UFUNCTION(BlueprintPure, Category = "Temporal Rewind|Serialization")
	static int32 BeginRead(const FTemporalSnapshot& Snapshot);

	// --- Writers ---------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "Temporal Rewind|Serialization")
	static void WriteFloat(UPARAM(ref) FTemporalSnapshot& Snapshot, float Value);

	UFUNCTION(BlueprintCallable, Category = "Temporal Rewind|Serialization")
	static void WriteInt32(UPARAM(ref) FTemporalSnapshot& Snapshot, int32 Value);

	UFUNCTION(BlueprintCallable, Category = "Temporal Rewind|Serialization")
	static void WriteBool(UPARAM(ref) FTemporalSnapshot& Snapshot, bool Value);

	UFUNCTION(BlueprintCallable, Category = "Temporal Rewind|Serialization")
	static void WriteVector(UPARAM(ref) FTemporalSnapshot& Snapshot, FVector Value);

	UFUNCTION(BlueprintCallable, Category = "Temporal Rewind|Serialization")
	static void WriteRotator(UPARAM(ref) FTemporalSnapshot& Snapshot, FRotator Value);

	UFUNCTION(BlueprintCallable, Category = "Temporal Rewind|Serialization")
	static void WriteTransform(UPARAM(ref) FTemporalSnapshot& Snapshot, FTransform Value);

	// --- Readers ---------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "Temporal Rewind|Serialization")
	static float ReadFloat(const FTemporalSnapshot& Snapshot, UPARAM(ref) int32& ByteOffset);

	UFUNCTION(BlueprintCallable, Category = "Temporal Rewind|Serialization")
	static int32 ReadInt32(const FTemporalSnapshot& Snapshot, UPARAM(ref) int32& ByteOffset);

	UFUNCTION(BlueprintCallable, Category = "Temporal Rewind|Serialization")
	static bool ReadBool(const FTemporalSnapshot& Snapshot, UPARAM(ref) int32& ByteOffset);

	UFUNCTION(BlueprintCallable, Category = "Temporal Rewind|Serialization")
	static FVector ReadVector(const FTemporalSnapshot& Snapshot, UPARAM(ref) int32& ByteOffset);

	UFUNCTION(BlueprintCallable, Category = "Temporal Rewind|Serialization")
	static FRotator ReadRotator(const FTemporalSnapshot& Snapshot, UPARAM(ref) int32& ByteOffset);

	UFUNCTION(BlueprintCallable, Category = "Temporal Rewind|Serialization")
	static FTransform ReadTransform(const FTemporalSnapshot& Snapshot, UPARAM(ref) int32& ByteOffset);

	// Animation For Skeletal Meshes
	/** Captures the current skeletal pose and appends raw FPoseSnapshot data into the snapshot blob. */
	UFUNCTION(BlueprintCallable, Category = "Temporal Rewind|Serialization", meta = (DisplayName = "Write Pose Snapshot"))
	static bool WritePoseSnapshot(UPARAM(ref) FTemporalSnapshot& Snapshot,USkeletalMeshComponent* MeshComponent);

	/** Reads FPoseSnapshot data from the blob at ByteOffset, restores the pose on the mesh via SavePoseSnapshot. */
	UFUNCTION(BlueprintCallable, Category = "Temporal Rewind|Serialization", meta = (DisplayName = "Read Pose Snapshot"))
	static bool ReadPoseSnapshot(const FTemporalSnapshot& Snapshot,UPARAM(ref) int32& ByteOffset,USkeletalMeshComponent* MeshComponent,FName PoseSnapshotName = TEXT("TemporalRewindPose"));
};