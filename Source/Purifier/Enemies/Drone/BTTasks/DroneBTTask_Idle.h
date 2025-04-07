// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "DroneBTTask_Idle.generated.h"

UCLASS()
class PURIFIER_API UDroneBTTask_Idle : public UBTTaskNode
{
    GENERATED_BODY()

public:
    UDroneBTTask_Idle();

protected:
    virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

    /** Допустимый радиус для окончания движения */
    UPROPERTY(EditAnywhere, Category = "Movement")
    float AcceptanceRadius = 100.f;

    UPROPERTY(EditAnywhere, Category = "Movement")
    float DesiredAltitude = 1500.f;

    UPROPERTY(EditAnywhere, Category = "Movement")
    float DesiredAltitudeWithoutFloor = 1000.f;

    UPROPERTY(EditAnywhere, Category = "Movement")
    float IdleDuration = 2.f;

    virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaTime) override;
};