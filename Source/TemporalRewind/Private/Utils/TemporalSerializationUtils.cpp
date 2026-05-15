// Copyright (c) 2026 Prasanna Keerthivasan. All rights reserved.

#include "Utils/TemporalSerializationUtils.h"

#include "Animation/AnimInstance.h"
#include "Animation/PoseSnapshot.h"
#include "Components/SkeletalMeshComponent.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "TemporalRewindModule.h"

namespace TemporalRewind::Serialization
{
	static bool ValidateRead(const FTemporalSnapshot& Snapshot, int32 ByteOffset,
		int32 RequiredBytes, const TCHAR* ReadKind)
	{
		if (ByteOffset < 0 || ByteOffset + RequiredBytes > Snapshot.StateBlob.Num())
		{
			UE_LOG(LogTemporalRewind, Error,
				TEXT("[SerializationUtils] %s underflow: offset=%d need=%d blob=%d"),
				ReadKind, ByteOffset, RequiredBytes, Snapshot.StateBlob.Num());
			return false;
		}
		return true;
	}
}

int32 UTemporalSerializationUtils::BeginRead(const FTemporalSnapshot& Snapshot)
{
	return 0;
}

// ---------------------------------------------------------------------------
// Writers
// ---------------------------------------------------------------------------

void UTemporalSerializationUtils::WriteBool(FTemporalSnapshot& Snapshot, bool Value)
{
	Snapshot.StateBlob.Add(Value ? 1u : 0u);
}

void UTemporalSerializationUtils::WriteFloat(FTemporalSnapshot& Snapshot, float Value)
{
	FMemoryWriter Writer(Snapshot.StateBlob, false);
	Writer.Seek(Snapshot.StateBlob.Num());
	Writer << Value;
}

void UTemporalSerializationUtils::WriteInt32(FTemporalSnapshot& Snapshot, int32 Value)
{
	FMemoryWriter Writer(Snapshot.StateBlob, false);
	Writer.Seek(Snapshot.StateBlob.Num());
	Writer << Value;
}

bool UTemporalSerializationUtils::WritePoseSnapshot(FTemporalSnapshot& Snapshot,
	USkeletalMeshComponent* MeshComponent)
{
	if (!MeshComponent)
	{
		UE_LOG(LogTemporalRewind, Warning,
			TEXT("[SerializationUtils] WritePoseSnapshot: null MeshComponent."));
		return false;
	}

	FPoseSnapshot CapturedPose;
	MeshComponent->SnapshotPose(CapturedPose);

	const int32 BoneCount = CapturedPose.BoneNames.Num();
	if (BoneCount == 0)
	{
		UE_LOG(LogTemporalRewind, Warning,
			TEXT("[SerializationUtils] WritePoseSnapshot: 0 bones on '%s'."),
			*MeshComponent->GetName());
		return false;
	}

	FMemoryWriter Writer(Snapshot.StateBlob, false);
	Writer.Seek(Snapshot.StateBlob.Num());

	int32 BoneCountMutable = BoneCount;
	Writer << BoneCountMutable;

	for (int32 i = 0; i < BoneCount; ++i)
	{
		Writer << CapturedPose.LocalTransforms[i];
	}

	return true;
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

void UTemporalSerializationUtils::WriteVector(FTemporalSnapshot& Snapshot, FVector Value)
{
	FMemoryWriter Writer(Snapshot.StateBlob, false);
	Writer.Seek(Snapshot.StateBlob.Num());
	Writer << Value;
}

// ---------------------------------------------------------------------------
// Readers
// ---------------------------------------------------------------------------

bool UTemporalSerializationUtils::ReadBool(const FTemporalSnapshot& Snapshot, int32& ByteOffset)
{
	if (!TemporalRewind::Serialization::ValidateRead(Snapshot, ByteOffset, 1, TEXT("ReadBool")))
	{
		return false;
	}

	const bool Value = Snapshot.StateBlob[ByteOffset] != 0u;
	ByteOffset += 1;
	return Value;
}

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

bool UTemporalSerializationUtils::ReadPoseSnapshot(const FTemporalSnapshot& Snapshot,
	int32& ByteOffset, USkeletalMeshComponent* MeshComponent, FName PoseSnapshotName)
{
	if (!MeshComponent)
	{
		UE_LOG(LogTemporalRewind, Warning,
			TEXT("[SerializationUtils] ReadPoseSnapshot: null MeshComponent."));
		return false;
	}

	if (ByteOffset >= Snapshot.StateBlob.Num())
	{
		UE_LOG(LogTemporalRewind, Warning,
			TEXT("[SerializationUtils] ReadPoseSnapshot: offset %d past end of blob %d."),
			ByteOffset, Snapshot.StateBlob.Num());
		return false;
	}

	FMemoryReader Reader(Snapshot.StateBlob, false);
	Reader.Seek(ByteOffset);

	int32 BoneCount = 0;
	Reader << BoneCount;

	if (Reader.IsError() || BoneCount <= 0 || BoneCount > 1000)
	{
		UE_LOG(LogTemporalRewind, Warning,
			TEXT("[SerializationUtils] ReadPoseSnapshot: invalid bone count %d."), BoneCount);
		return false;
	}

	FPoseSnapshot LivePose;
	MeshComponent->SnapshotPose(LivePose);

	if (LivePose.BoneNames.Num() != BoneCount)
	{
		UE_LOG(LogTemporalRewind, Warning,
			TEXT("[SerializationUtils] ReadPoseSnapshot: bone count mismatch blob=%d mesh=%d."),
			BoneCount, LivePose.BoneNames.Num());
		return false;
	}

	FPoseSnapshot RestoredPose;
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
			TEXT("[SerializationUtils] ReadPoseSnapshot: no AnimInstance on '%s'."),
			*MeshComponent->GetName());
		return false;
	}

	RestoredPose.SnapshotName = PoseSnapshotName;
	RestoredPose.bIsValid = true;
	AnimInstance->AddPoseSnapshot(PoseSnapshotName) = MoveTemp(RestoredPose);

	return true;
}

FRotator UTemporalSerializationUtils::ReadRotator(const FTemporalSnapshot& Snapshot, int32& ByteOffset)
{
	FRotator Value = FRotator::ZeroRotator;

	if (ByteOffset < 0 || ByteOffset >= Snapshot.StateBlob.Num())
	{
		UE_LOG(LogTemporalRewind, Error,
			TEXT("[SerializationUtils] ReadRotator: offset %d out of range, blob %d."),
			ByteOffset, Snapshot.StateBlob.Num());
		return Value;
	}

	FMemoryReader Reader(Snapshot.StateBlob, false);
	Reader.Seek(ByteOffset);
	Reader << Value;

	if (Reader.IsError())
	{
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
			TEXT("[SerializationUtils] ReadTransform: offset %d out of range, blob %d."),
			ByteOffset, Snapshot.StateBlob.Num());
		return Value;
	}

	FMemoryReader Reader(Snapshot.StateBlob, false);
	Reader.Seek(ByteOffset);
	Reader << Value;

	if (Reader.IsError())
	{
		return FTransform::Identity;
	}

	ByteOffset = Reader.Tell();
	return Value;
}

FVector UTemporalSerializationUtils::ReadVector(const FTemporalSnapshot& Snapshot, int32& ByteOffset)
{
	FVector Value = FVector::ZeroVector;

	if (ByteOffset < 0 || ByteOffset >= Snapshot.StateBlob.Num())
	{
		UE_LOG(LogTemporalRewind, Error,
			TEXT("[SerializationUtils] ReadVector: offset %d out of range, blob %d."),
			ByteOffset, Snapshot.StateBlob.Num());
		return Value;
	}

	FMemoryReader Reader(Snapshot.StateBlob, false);
	Reader.Seek(ByteOffset);
	Reader << Value;

	if (Reader.IsError())
	{
		return FVector::ZeroVector;
	}

	ByteOffset = Reader.Tell();
	return Value;
}