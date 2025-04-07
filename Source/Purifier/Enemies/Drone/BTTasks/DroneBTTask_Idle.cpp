// Fill out your copyright notice in the Description page of Project Settings.

#include "DroneBTTask_Idle.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AIController.h"
#include "Purifier/Enemies/Drone/Drone.h"
#include "Purifier/Enemies/Drone/DroneAIController.h"

UDroneBTTask_Idle::UDroneBTTask_Idle()
{
    NodeName = TEXT("Idle With Flocking");
    bNotifyTick = true;
}

EBTNodeResult::Type UDroneBTTask_Idle::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    AAIController* AICon = OwnerComp.GetAIOwner();
    if (!AICon)
    {
        return EBTNodeResult::Failed;
    }

    ADroneAIController* DroneAICon = Cast<ADroneAIController>(AICon);
    if (!DroneAICon)
    {
        return EBTNodeResult::Failed;
    }

    APawn* Pawn = AICon->GetPawn();
    if (!Pawn)
    {
        return EBTNodeResult::Failed;
    }

    ADrone* Drone = Cast<ADrone>(Pawn);
    if (!Drone)
    {
        return EBTNodeResult::Failed;
    }

    UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
    FVector TargetLocation = BlackboardComp->GetValueAsVector("TargetLocation");

    FTimerHandle TimerHandle;
    FTimerDelegate TimerDelegate;
    TimerDelegate.BindLambda([this, &OwnerComp]() { FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded); });
    OwnerComp.GetWorld()->GetTimerManager().SetTimer(TimerHandle, TimerDelegate, IdleDuration, false);

    return EBTNodeResult::InProgress;
}

void UDroneBTTask_Idle::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaTime)
{
    AAIController* AICon = OwnerComp.GetAIOwner();
    ADroneAIController* DroneAICon = Cast<ADroneAIController>(AICon);

    APawn* AIPawn = AICon->GetPawn();

    ADrone* Drone = Cast<ADrone>(AIPawn);

    UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();

    FVector TargetLocation = BlackboardComp->GetValueAsVector("TargetLocation");
    DroneAICon->RotateTowards(TargetLocation, DeltaTime);


    const float Distance = FVector::Distance(Drone->GetActorLocation(), TargetLocation);

    DroneAICon->IdleWithFlocking(DeltaTime);
}
