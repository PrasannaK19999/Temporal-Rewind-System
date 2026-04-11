// Copyright Epic Games, Inc. All Rights Reserved.

#include "TemporalRewindModule.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogTemporalRewind);

#define LOCTEXT_NAMESPACE "FTemporalRewindModule"

void FTemporalRewindModule::StartupModule()
{
	UE_LOG(LogTemporalRewind, Display, TEXT("[TemporalRewindModule] Free module started."));
}

void FTemporalRewindModule::ShutdownModule()
{
	UE_LOG(LogTemporalRewind, Display, TEXT("[TemporalRewindModule] Free module shut down."));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FTemporalRewindModule, TemporalRewind)