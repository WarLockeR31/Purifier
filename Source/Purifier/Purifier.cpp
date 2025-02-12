// Copyright Epic Games, Inc. All Rights Reserved.

#include "Purifier.h"
#include "Modules/ModuleManager.h"

IMPLEMENT_PRIMARY_GAME_MODULE( FDefaultGameModuleImpl, Purifier, "Purifier" );

#if WITH_EDITOR || UE_BUILD_DEBUG
#define CHECK_PUREVIRTUALS 1
#else
#define CHECK_PUREVIRTUALS 0
#endif
