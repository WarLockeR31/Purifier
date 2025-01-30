// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "InputCharacterMovementComponent.generated.h"

UENUM(BlueprintType)
enum class ECustomMovementMode : uint8
{
    CMOVE_Dash UMETA(DisplayName = "Dash")
};

/**
 * 
 */
UCLASS()
class PURIFIER_API UInputCharacterMovementComponent : public UCharacterMovementComponent
{
    GENERATED_BODY()

public:
    UInputCharacterMovementComponent();

    void StartDash();
    void StopDash();

protected:
    virtual void PhysCustom(float DeltaTime, int32 Iterations) override;

};