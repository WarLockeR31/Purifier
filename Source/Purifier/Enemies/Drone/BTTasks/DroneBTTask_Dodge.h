// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "DroneBTTask_Dodge.generated.h"

/**
 * 
 */
UCLASS()
class PURIFIER_API UDroneBTTask_Dodge : public UBTTaskNode
{
    GENERATED_BODY()

public:
    UDroneBTTask_Dodge();

protected:
    virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

    UPROPERTY(EditAnywhere, Category = "Dodge")
    float DodgeCooldown = 1.0f;

    UFUNCTION()
    bool ShouldDodgeRight(FVector PlayerLocation, FVector PlayerViewDirection, FVector DroneLocation) const;
};
