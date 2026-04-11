// Copyright (c) Temporal Rewind System. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtr.h"
#include "Data/TemporalEnums.h"
#include "RewindableActorEntry.generated.h"

/**
 * Per-actor registration record held by UTemporalRewindSubsystem.
 * WeakRef is a plain UObject weak pointer because TWeakInterfacePtr does not
 * bind to Blueprint implementers of a UINTERFACE. Interface calls go through
 * IRewindable::Execute_XXX(Obj), which works for both C++ and Blueprint.
 */
USTRUCT()
struct TEMPORALREWIND_API FRewindableActorEntry
{
	GENERATED_BODY()

	TWeakObjectPtr<UObject> WeakRef;

	EOrphanedTimelinePolicy OrphanPolicy = EOrphanedTimelinePolicy::KeepUntilOverwritten;

	FGuid ActorGuid;
};