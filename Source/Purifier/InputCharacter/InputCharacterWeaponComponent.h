// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Purifier/Weapons/WeaponComponent.h"
#include "InputCharacterWeaponComponent.generated.h"

/**
 * 
 */
UCLASS()
class PURIFIER_API UInputCharacterWeaponComponent : public UWeaponComponent
{
	GENERATED_BODY()
	
protected:
	UPROPERTY(VisibleAnywhere, Category = "Weapons")
	class AInputCharacter* OwnerInputCharacter;

	UPROPERTY(VisibleAnywhere, Category = "Weapons")
	class UCameraComponent* OwnerInputCharacterCamera;

	UPROPERTY(VisibleAnywhere, Category = "Weapons")
	APlayerController* OwnerPlayerController;

public:
	virtual void BeginPlay() override;

protected:
	virtual FVector CorrectRaycastPosition_Implementation(FVector OldLocation) const override;

};
