// Fill out your copyright notice in the Description page of Project Settings.

#include "DroneBTTask_Move.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AIController.h"
#include "Purifier/Enemies/Drone/Drone.h"
#include "Purifier/Enemies/Drone/DroneAIController.h"

UDroneBTTask_Move::UDroneBTTask_Move()
{
    NodeName = TEXT("Move With Flocking");
    bNotifyTick = true;
}

EBTNodeResult::Type UDroneBTTask_Move::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
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

    UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
    FVector TargetLocation = BlackboardComp->GetValueAsVector("TargetLocation");

    if (FVector::Dist(Drone->GetActorLocation(), TargetLocation) <= AcceptanceRadius)
    {
        return EBTNodeResult::Succeeded;

    }

    //UE_LOG(LogTemp, Warning, TEXT("Success"));

    //DroneAICon->MoveToLocationWithFlocking(TargetLocation, OwnerComp.GetWorld()->GetDeltaSeconds());
    //DroneAICon->RotateTowards(TargetLocation, OwnerComp.GetWorld()->GetDeltaSeconds());
    
    return EBTNodeResult::InProgress;
}

void UDroneBTTask_Move::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaTime) 
{
    AAIController* AICon = OwnerComp.GetAIOwner();
    ADroneAIController* DroneAICon = Cast<ADroneAIController>(AICon);

    APawn* AIPawn = AICon->GetPawn();

    ADrone* Drone = Cast<ADrone>(AIPawn);

    UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();

    /*if (BlackboardComp->GetValueAsBool("TargetIsVisible"))
    {
        FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
        return;
    }*/

    FVector TargetLocation = BlackboardComp->GetValueAsVector("TargetLocation");
    DroneAICon->RotateTowards(TargetLocation, DeltaTime);

    FVector TraceStart = TargetLocation;
    FVector TraceEnd = TraceStart + FVector(0, 0, -10000.0f);
    FCollisionQueryParams TraceParams;
    TraceParams.AddIgnoredActor(Drone);

    FHitResult Hit;
    if (Drone->GetWorld()->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_WorldStatic, TraceParams))
    {
        TargetLocation.Z = Hit.ImpactPoint.Z + DesiredAltitude;
    }
    else
    {
        const float ZDiff = FMath::Abs(Drone->GetActorLocation().Z - TargetLocation.Z);
        if (ZDiff < DesiredAltitudeWithoutFloor)
        {
            TargetLocation.Z += DesiredAltitude;
        }
    }

    const float Distance = FVector::Distance(Drone->GetActorLocation(), TargetLocation);

    /*if (Distance <= AcceptanceRadius)
    {
        FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
        return;
    }*/

    // Обновляем движение каждый кадр
    DroneAICon->MoveToLocationWithFlocking(TargetLocation, DeltaTime);
}