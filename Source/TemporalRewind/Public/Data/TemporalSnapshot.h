#pragma once

#include "CoreMinimal.h"
#include "TemporalSnapshot.generated.h"

/**
 * Timestamped opaque container. The plugin never reads StateBlob —
 * the owning actor writes and reads it through IRewindable.
 */
USTRUCT(BlueprintType)
struct TEMPORALREWIND_API FTemporalSnapshot
{
	GENERATED_BODY()

	/** World time when captured (UWorld::GetTimeSeconds). */
	UPROPERTY(BlueprintReadOnly, Category = "Temporal Rewind")
	float Timestamp = 0.0f;

	/** Stable identity of the actor that produced this snapshot. */
	UPROPERTY(BlueprintReadOnly, Category = "Temporal Rewind")
	FGuid OwnerGuid;

	/** Opaque payload actor authored, plugin never inspects it. */
	UPROPERTY(BlueprintReadWrite, Category = "Temporal Rewind")
	TArray<uint8> StateBlob;
};