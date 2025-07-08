// Fill out your copyright notice in the Description page of Project Settings.


#include "DroneBTTask_Shoot.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AIController.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/Actor.h"

UDroneBTTask_Shoot::UDroneBTTask_Shoot()
{
    NodeName = TEXT("Shoot");
}

EBTNodeResult::Type UDroneBTTask_Shoot::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
    if (!BlackboardComp)
        return EBTNodeResult::Failed;

    LastKnownPlayerLocation = BlackboardComp->GetValueAsVector("TargetLocation");

    FTimerHandle TimerHandle;
    FTimerDelegate TimerDelegate;
    TimerDelegate.BindLambda([this, &OwnerComp]() { Shoot(OwnerComp); });
    OwnerComp.GetWorld()->GetTimerManager().SetTimer(TimerHandle, TimerDelegate, ChargeTime, false);

    BlackboardComp->SetValueAsFloat("ShootCooldown", ShootCooldown + ChargeTime);
    
    return EBTNodeResult::InProgress;
}

void UDroneBTTask_Shoot::Shoot(UBehaviorTreeComponent& OwnerComp)
{
    AAIController* AICon = OwnerComp.GetAIOwner();
    if (!AICon)
        return;

    APawn* AIPawn = AICon->GetPawn();
    if (!AIPawn)
        return;

    UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
    if (!BlackboardComp)
        return;


    FVector Start = AIPawn->GetActorLocation();
    FVector End = LastKnownPlayerLocation;

    FHitResult Hit;
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(AIPawn);

    bool bHit = AIPawn->GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, QueryParams);

    DrawDebugLine(AIPawn->GetWorld(), Start, End, FColor::Red, false, 1.f, 0, 2.f);

    if (bHit && Hit.GetActor())
    {
        UGameplayStatics::ApplyDamage(Hit.GetActor(), Damage, AICon, AIPawn, nullptr);
    }

    FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
}
