// Fill out your copyright notice in the Description page of Project Settings.

#include "DroneBTTask_Dodge.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AIController.h"
#include "GameFramework/Pawn.h"
#include "Purifier/Enemies/Drone/Drone.h"
#include "Purifier/Dash/BaseDashComponent.h"
#include "TimerManager.h"

UDroneBTTask_Dodge::UDroneBTTask_Dodge()
{
    NodeName = TEXT("Dodge");
}

EBTNodeResult::Type UDroneBTTask_Dodge::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    AAIController* AICon = OwnerComp.GetAIOwner();
    if (!AICon)
        return EBTNodeResult::Failed;

    APawn* AIPawn = AICon->GetPawn();
    if (!AIPawn)
        return EBTNodeResult::Failed;

    ADrone* Drone = Cast<ADrone>(AIPawn);
    if (!Drone)
    {
        return EBTNodeResult::Failed;
    }

    UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
    BlackboardComp->SetValueAsFloat("DodgeCooldown", DodgeCooldown);
    
    
    bool shouldDodgeRight = ShouldDodgeRight(
        BlackboardComp->GetValueAsVector("TargetLocation"),
        BlackboardComp->GetValueAsVector("TargetViewDirection"),
        AIPawn->GetActorLocation()
    );

    FVector2D DodgeDirection = (shouldDodgeRight ? FVector2D(1.f, 0.f) : -FVector2D(1.f, 0.f));
    
    Drone->SetDashDirection(DodgeDirection);
    Drone->GetDash()->StartDash();
    
    FTimerHandle TimerHandle;
    FTimerDelegate TimerDelegate;
    TimerDelegate.BindLambda([this, &OwnerComp]() {
        OwnerComp.GetBlackboardComponent()->SetValueAsBool("FlockDodgeInitiated", false);
        FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded); 
    });
    OwnerComp.GetWorld()->GetTimerManager().SetTimer(TimerHandle, TimerDelegate, AfterDodgeWaitTime, false);
    

    return EBTNodeResult::InProgress;
}

bool UDroneBTTask_Dodge::ShouldDodgeRight(FVector TargetLocation, FVector TargetViewDirection, FVector DroneLocation) const
{
    FVector TargetToDrone = (DroneLocation - TargetLocation).GetSafeNormal();

    FVector Perpendicular = FVector::CrossProduct(TargetViewDirection, FVector::UpVector);

    float Dot = FVector::DotProduct(TargetToDrone, Perpendicular);

    return Dot > 0;
}
