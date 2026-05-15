// Copyright (c) 2026 Prasanna Keerthivasan. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtr.h"
#include "Data/TemporalEnums.h"
#include "RewindableActorEntry.generated.h"

USTRUCT()
struct TEMPORALREWIND_API FRewindableActorEntry
{
	GENERATED_BODY()

	TWeakObjectPtr<UObject> WeakRef;

	EOrphanedTimelinePolicy OrphanPolicy = EOrphanedTimelinePolicy::KeepUntilOverwritten;

	FGuid ActorGuid;
};