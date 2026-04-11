// Copyright (c) Temporal Rewind System. All rights reserved.

#include "Utils/TemporalSerializationUtils.h"
#include "TemporalRewindModule.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include "Animation/PoseSnapshot.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"

namespace TemporalRewind::Serialization
{
	// Shared validation for all readers � checks there are at least RequiredBytes
	// remaining from ByteOffset. Logs an error and returns false on underflow.
	static bool ValidateRead(const FTemporalSnapshot& Snapshot, int32 ByteOffset, int32 RequiredBytes, const TCHAR* ReadKind)
	{
		if (ByteOffset < 0 || ByteOffset + RequiredBytes > Snapshot.StateBlob.Num())
		{
			UE_LOG(LogTemporalRewind, Error,
				TEXT("[TemporalSerializationUtils] %s underflow: offset=%d need=%d blob=%d owner=%s"),
				ReadKind, ByteOffset, RequiredBytes, Snapshot.StateBlob.Num(), *Snapshot.OwnerGuid.ToString());
			return false;
		}
		return true;
	}
}

int32 UTemporalSerializationUtils::BeginRead(const FTemporalSnapshot& Snapshot)
{
	return 0;
}

// --- Writers ---------------------------------------------------------------

void UTemporalSerializationUtils::WriteFloat(FTemporalSnapshot& Snapshot, float Value)
{
	FMemoryWriter Writer(Snapshot.StateBlob, /*bIsPersistent*/ false);
	Writer.Seek(Snapshot.StateBlob.Num());
	Writer << Value;
}

void UTemporalSerializationUtils::WriteInt32(FTemporalSnapshot& Snapshot, int32 Value)
{
	FMemoryWriter Writer(Snapshot.StateBlob, false);
	Writer.Seek(Snapshot.StateBlob.Num());
	Writer << Value;
}

void UTemporalSerializationUtils::WriteBool(FTemporalSnapshot& Snapshot, bool Value)
{
	// Stored as one byte for Free-tier simplicity. Bit packing is Pro.
	const uint8 Byte = Value ? 1u : 0u;
	Snapshot.StateBlob.Add(Byte);
}

void UTemporalSerializationUtils::WriteVector(FTemporalSnapshot& Snapshot, FVector Value)
{
	FMemoryWriter Writer(Snapshot.StateBlob, false);
	Writer.Seek(Snapshot.StateBlob.Num());
	Writer << Value;
}

void UTemporalSerializationUtils::WriteRotator(FTemporalSnapshot& Snapshot, FRotator Value)
{
	FMemoryWriter Writer(Snapshot.StateBlob, false);
	Writer.Seek(Snapshot.StateBlob.Num());
	Writer << Value;
}

void UTemporalSerializationUtils::WriteTransform(FTemporalSnapshot& Snapshot, FTransform Value)
{
	FMemoryWriter Writer(Snapshot.StateBlob, false);
	Writer.Seek(Snapshot.StateBlob.Num());
	Writer << Value;
}

// --- Readers ---------------------------------------------------------------

float UTemporalSerializationUtils::ReadFloat(const FTemporalSnapshot& Snapshot, int32& ByteOffset)
{
	float Value = 0.0f;
	if (!TemporalRewind::Serialization::ValidateRead(Snapshot, ByteOffset, sizeof(float), TEXT("ReadFloat")))
	{
		return Value;
	}

	FMemoryReader Reader(Snapshot.StateBlob, false);
	Reader.Seek(ByteOffset);
	Reader << Value;
	ByteOffset += sizeof(float);
	return Value;
}

int32 UTemporalSerializationUtils::ReadInt32(const FTemporalSnapshot& Snapshot, int32& ByteOffset)
{
	int32 Value = 0;
	if (!TemporalRewind::Serialization::ValidateRead(Snapshot, ByteOffset, sizeof(int32), TEXT("ReadInt32")))
	{
		return Value;
	}

	FMemoryReader Reader(Snapshot.StateBlob, false);
	Reader.Seek(ByteOffset);
	Reader << Value;
	ByteOffset += sizeof(int32);
	return Value;
}

bool UTemporalSerializationUtils::ReadBool(const FTemporalSnapshot& Snapshot, int32& ByteOffset)
{
	if (!TemporalRewind::Serialization::ValidateRead(Snapshot, ByteOffset, 1, TEXT("ReadBool")))
	{
		return false;
	}

	const uint8 Byte = Snapshot.StateBlob[ByteOffset];
	ByteOffset += 1;
	return Byte != 0u;
}

FVector UTemporalSerializationUtils::ReadVector(const FTemporalSnapshot& Snapshot, int32& ByteOffset)
{
	FVector Value = FVector::ZeroVector;

	if (ByteOffset < 0 || ByteOffset >= Snapshot.StateBlob.Num())
	{
		UE_LOG(LogTemporalRewind, Error,
			TEXT("[TemporalSerializationUtils] ReadVector offset out of range: offset=%d blob=%d owner=%s"),
			ByteOffset, Snapshot.StateBlob.Num(), *Snapshot.OwnerGuid.ToString());
		return Value;
	}

	FMemoryReader Reader(Snapshot.StateBlob, false);
	Reader.Seek(ByteOffset);
	Reader << Value;

	if (Reader.IsError())
	{
		UE_LOG(LogTemporalRewind, Error,
			TEXT("[TemporalSerializationUtils] ReadVector underflow at offset=%d blob=%d owner=%s"),
			ByteOffset, Snapshot.StateBlob.Num(), *Snapshot.OwnerGuid.ToString());
		return FVector::ZeroVector;
	}

	ByteOffset = Reader.Tell();
	return Value;
}

FRotator UTemporalSerializationUtils::ReadRotator(const FTemporalSnapshot& Snapshot, int32& ByteOffset)
{
	FRotator Value = FRotator::ZeroRotator;

	if (ByteOffset < 0 || ByteOffset >= Snapshot.StateBlob.Num())
	{
		UE_LOG(LogTemporalRewind, Error,
			TEXT("[TemporalSerializationUtils] ReadRotator offset out of range: offset=%d blob=%d owner=%s"),
			ByteOffset, Snapshot.StateBlob.Num(), *Snapshot.OwnerGuid.ToString());
		return Value;
	}

	FMemoryReader Reader(Snapshot.StateBlob, false);
	Reader.Seek(ByteOffset);
	Reader << Value;

	if (Reader.IsError())
	{
		UE_LOG(LogTemporalRewind, Error,
			TEXT("[TemporalSerializationUtils] ReadRotator underflow at offset=%d blob=%d owner=%s"),
			ByteOffset, Snapshot.StateBlob.Num(), *Snapshot.OwnerGuid.ToString());
		return FRotator::ZeroRotator;
	}

	ByteOffset = Reader.Tell();
	return Value;
}

FTransform UTemporalSerializationUtils::ReadTransform(const FTemporalSnapshot& Snapshot, int32& ByteOffset)
{
	FTransform Value = FTransform::Identity;

	if (ByteOffset < 0 || ByteOffset >= Snapshot.StateBlob.Num())
	{
		UE_LOG(LogTemporalRewind, Error,
			TEXT("[TemporalSerializationUtils] ReadTransform offset out of range: offset=%d blob=%d owner=%s"),
			ByteOffset, Snapshot.StateBlob.Num(), *Snapshot.OwnerGuid.ToString());
		return Value;
	}

	FMemoryReader Reader(Snapshot.StateBlob, false);
	Reader.Seek(ByteOffset);
	Reader << Value;

	if (Reader.IsError())
	{
		UE_LOG(LogTemporalRewind, Error,
			TEXT("[TemporalSerializationUtils] ReadTransform underflow at offset=%d blob=%d owner=%s"),
			ByteOffset, Snapshot.StateBlob.Num(), *Snapshot.OwnerGuid.ToString());
		return FTransform::Identity;
	}

	

	ByteOffset = Reader.Tell();
	return Value;
}

bool UTemporalSerializationUtils::WritePoseSnapshot(UPARAM(ref)FTemporalSnapshot& Snapshot, USkeletalMeshComponent* MeshComponent)
{
	if (!MeshComponent)
	{
		UE_LOG(LogTemporalRewind, Warning,
			TEXT("[SerializationUtils] WritePoseSnapshot � MeshComponent is null."));
		return false;
	}

	FPoseSnapshot CapturedPose;
	MeshComponent->SnapshotPose(CapturedPose);

	const int32 BoneCount = CapturedPose.BoneNames.Num();
	if (BoneCount == 0)
	{
		UE_LOG(LogTemporalRewind, Warning,
			TEXT("[SerializationUtils] WritePoseSnapshot � SnapshotPose returned 0 bones on '%s'."),
			*MeshComponent->GetName());
		return false;
	}

	// Bone names are fixed by the skeleton — same every frame.
	// Write only the count (read-side sanity check) + transforms.
	// Names are reconstructed at restore time from a live SnapshotPose() call.
	FMemoryWriter Writer(Snapshot.StateBlob, false);
	Writer.Seek(Snapshot.StateBlob.Num());

	int32 BoneCountToWrite = BoneCount;
	Writer << BoneCountToWrite;

	for (int32 i = 0; i < BoneCount; ++i)
	{
		Writer << CapturedPose.LocalTransforms[i];
	}

	UE_LOG(LogTemporalRewind, VeryVerbose,
		TEXT("[SerializationUtils] WritePoseSnapshot - %d bones, blob %d bytes"),
		BoneCount, Snapshot.StateBlob.Num());

	return true;
}

bool UTemporalSerializationUtils::ReadPoseSnapshot(const FTemporalSnapshot& Snapshot, UPARAM(ref)int32& ByteOffset, USkeletalMeshComponent* MeshComponent, FName PoseSnapshotName)
{
	if (!MeshComponent)
	{
		UE_LOG(LogTemporalRewind, Warning,
			TEXT("[SerializationUtils] ReadPoseSnapshot � MeshComponent is null."));
		return false;
	}

	if (ByteOffset >= Snapshot.StateBlob.Num())
	{
		UE_LOG(LogTemporalRewind, Warning,
			TEXT("[SerializationUtils] ReadPoseSnapshot � ByteOffset (%d) past end of blob (%d)."),
			ByteOffset, Snapshot.StateBlob.Num());
		return false;
	}

	FMemoryReader Reader(Snapshot.StateBlob);
	Reader.Seek(ByteOffset);

	FPoseSnapshot RestoredPose;

	int32 BoneCount = 0;
	Reader << BoneCount;
	if (Reader.IsError() || BoneCount <= 0 || BoneCount > 1000)
	{
		UE_LOG(LogTemporalRewind, Warning,
			TEXT("[SerializationUtils] ReadPoseSnapshot � Invalid bone count: %d."), BoneCount);
		return false;
	}

	// Reconstruct bone names from the live mesh — same skeleton, same order.
	FPoseSnapshot LivePose;
	MeshComponent->SnapshotPose(LivePose);
	if (LivePose.BoneNames.Num() != BoneCount)
	{
		UE_LOG(LogTemporalRewind, Warning,
			TEXT("[SerializationUtils] ReadPoseSnapshot - Bone count mismatch: blob=%d mesh=%d on '%s'."),
			BoneCount, LivePose.BoneNames.Num(), *MeshComponent->GetName());
		return false;
	}

	RestoredPose.BoneNames = MoveTemp(LivePose.BoneNames);
	RestoredPose.SkeletalMeshName = LivePose.SkeletalMeshName;

	RestoredPose.LocalTransforms.SetNum(BoneCount);
	for (int32 i = 0; i < BoneCount; ++i)
	{
		Reader << RestoredPose.LocalTransforms[i];
		if (Reader.IsError())
		{
			return false;
		}
	}

	ByteOffset = Reader.Tell();

	UAnimInstance* AnimInstance = MeshComponent->GetAnimInstance();
	if (!AnimInstance)
	{
		UE_LOG(LogTemporalRewind, Warning,
			TEXT("[SerializationUtils] ReadPoseSnapshot - No AnimInstance on '%s'."),
			*MeshComponent->GetName());
		return false;
	}

	RestoredPose.SnapshotName = PoseSnapshotName;
	RestoredPose.bIsValid = true;
	AnimInstance->AddPoseSnapshot(PoseSnapshotName) = MoveTemp(RestoredPose);

	UE_LOG(LogTemporalRewind, VeryVerbose,
		TEXT("[SerializationUtils] ReadPoseSnapshot - Applied %d bones via '%s', offset now %d"),
		BoneCount, *PoseSnapshotName.ToString(), ByteOffset);

	return true;

}
