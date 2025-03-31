// Fill out your copyright notice in the Description page of Project Settings.

#include "DroneBTTask_Idle.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AIController.h"
#include "Purifier/Enemies/Drone/Drone.h"

UDroneBTTask_Idle::UDroneBTTask_Idle()
{
    NodeName = TEXT("Idle With Flocking");
}

EBTNodeResult::Type UDroneBTTask_Idle::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    // Получаем AI Controller и Pawn
    AAIController* AICon = OwnerComp.GetAIOwner();
    if (!AICon)
    {
        return EBTNodeResult::Failed;
    }

    APawn* AIPawn = AICon->GetPawn();
    if (!AIPawn)
    {
        return EBTNodeResult::Failed;
    }

    ADrone* Drone = Cast<ADrone>(AIPawn);
    if (!Drone)
    {
        return EBTNodeResult::Failed;
    }

    // Получаем целевую позицию из Blackboard (ключ TargetLocation)
    UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
    FVector TargetLocation = BlackboardComp->GetValueAsVector("TargetLocation");

    // Если дрон уже близко к цели, считаем задачу выполненной
    if (FVector::Dist(Drone->GetActorLocation(), TargetLocation) <= AcceptanceRadius)
    {
        return EBTNodeResult::Succeeded;
    }

    FVector FlockingVector = Drone->GetFlockingVector(OwnerComp.GetWorld()->GetDeltaSeconds());
    Drone->MoveToLocationWithFlocking(TargetLocation, FlockingVector);

    return EBTNodeResult::Succeeded;
}
