// Fill out your copyright notice in the Description page of Project Settings.


#include "BallLightning.h"
#include "Components/SphereComponent.h"
#include "TimerManager.h"
#include "GameFramework/Actor.h"
#include "Engine/OverlapResult.h"
#include "Purifier/DamageSystem/Damagable.h"

ABallLightning::ABallLightning()
{
    PrimaryActorTick.bCanEverTick = true;
}

void ABallLightning::BeginPlay()
{
    Super::BeginPlay();

    GetWorld()->GetTimerManager().SetTimer(LightningDamageTimer, this, &ABallLightning::ApplyLightningDamage, LightningDamageInterval, true);
}

void ABallLightning::OnHit_Implementation(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
    UE_LOG(LogTemp, Warning, TEXT("AAAAAAAAAAAAAAA"));
    Destroy();
}

//void ABallLightning::OnOverlapWithTarget(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
//{
//    if (OtherActor && OtherActor->Implements<UDamagable>())
//    {
//        FDamageInfo DamageInfo;
//        DamageInfo.DamageAmount = LightningDamage;
//        DamageInfo.DamageCauser = this;
//
//        // Наносим урон, если объект может получать урон
//        IDamagable* DamagableActor = Cast<IDamagable>(OtherActor);
//        if (DamagableActor)
//        {
//            DamagableActor->TakeDamage(DamageInfo);
//        }
//    }
//}

void ABallLightning::ApplyLightningDamage()
{
    // Логика для обнаружения всех объектов в радиусе молнии
    TArray<FOverlapResult> OverlapResults;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this); // Игнорируем саму молнию

    // Пересекаем с объектами в радиусе молнии
    if (GetWorld()->OverlapMultiByChannel(OverlapResults, GetActorLocation(), FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(LightningRadius), Params))
    {
        for (auto& Hit : OverlapResults)
        {
            AActor* HitActor = Hit.GetActor();
            if (HitActor && HitActor->Implements<UDamagable>())
            {
                IDamagable::Execute_TakeDamage(HitActor, DamageInfo);
            }
        }
    }
}
