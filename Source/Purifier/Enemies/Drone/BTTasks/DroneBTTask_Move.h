// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "DroneBTTask_Move.generated.h"

UCLASS()
class PURIFIER_API UDroneBTTask_Move : public UBTTaskNode
{
    GENERATED_BODY()

public:
    UDroneBTTask_Move();

protected:
    virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

    /** Допустимый радиус для окончания движения */
    UPROPERTY(EditAnywhere, Category = "Movement")
    float AcceptanceRadius = 100.f;
};