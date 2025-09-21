// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GameFramework/Pawn.h"
#include "VOFollowingComponent.h"

#include "VOSettings.generated.h"

UENUM(BlueprintType)
enum class AvoidanceStyle : uint8
{
	VelocityObstacle,
	AccelerationObstacle,
};

/**
 * Project-wide VO settings
 */
UCLASS(config=Game, defaultconfig, meta=(DisplayName="VO Settings"))
class VOBASIC_API UVOSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Config, Category="Presets")
	TMap<FName, FVOParams> Presets;

	UPROPERTY(EditAnywhere, Config, Category="Presets",
		  meta=(GetValueOptions="GetPresetNames", ForceInlineRow="true"))
	TMap<TSoftClassPtr<APawn>, FName> ClassToPreset;

	UPROPERTY(EditAnywhere, Config, Category="Presets", meta=(GetOptions="GetPresetNames"))
	FName DefaultPreset;

	UPROPERTY(EditAnywhere, Config, Category="General")
	AvoidanceStyle AvoidanceStyle;
	

	static const UVOSettings* Get() { return GetDefault<UVOSettings>(); }

	const FVOParams* FindPreset(FName Name) const
	{
		return Presets.Find(Name);
	}

	const FVOParams& GetPresetOrDefault(FName Name) const;

	FName ResolvePresetForPawnClass(UClass* PawnClass) const;

	/** Collect names of presets (for BP/Editor). */
	UFUNCTION(BlueprintCallable, Category="VO|Settings")
	void GetPresetNames(TArray<FName>& OutNames) const
	{
		OutNames.Reset(Presets.Num());
		for (const auto& It : Presets) { OutNames.Add(It.Key); }
	}
};