// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "VOConfigProvider.generated.h"

// This class does not need to be modified.
UINTERFACE()
class UVOConfigProvider : public UInterface
{
	GENERATED_BODY()
};

/**
 * An interface that Pawn (or its component via ImplementedInBP) can implement.
 * Returns the desired VO profile and/or overrides for this particular instance.
 */
class VOBASIC_API IVOConfigProvider
{
	GENERATED_BODY()

public: // TODO: Translate

	/**
	 * @param OutProfileName - Specific profile name for class, implementing IVOConfigProvider 
	 * @return True if profile exists, false if it doesn't
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="VO")
	bool GetVOProfileName(FName& OutProfileName) const;

	/**
	 * @param OutOverrides VOParams preset that should override current preset
	 * @return True if VO overrides are provided, false otherwise.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="VO")
	bool GetVOOverrides(struct FVOParams& OutOverrides) const;
};