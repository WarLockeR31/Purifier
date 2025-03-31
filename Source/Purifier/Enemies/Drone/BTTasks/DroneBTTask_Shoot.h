// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "DroneBTTask_Shoot.generated.h"

/**
 * 
 */
UCLASS()
class PURIFIER_API UDroneBTTask_Shoot : public UBTTaskNode
{
    GENERATED_BODY()

public:
    UDroneBTTask_Shoot();

protected:
    virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

    UPROPERTY(EditAnywhere, Category = "Shoot")
    float ChargeTime = 0.3f;

    UPROPERTY(EditAnywhere, Category = "Shoot")
    float Damage = 10.f;

    UPROPERTY(VisibleAnywhere, Category = "Shoot")
    FVector LastKnownPlayerLocation;

    void Shoot(UBehaviorTreeComponent& OwnerComp);
};
