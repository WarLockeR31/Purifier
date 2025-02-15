// Fill out your copyright notice in the Description page of Project Settings.


#include "LightningPistol.h"
#include "Projectiles/ProjectileBase.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "DrawDebugHelpers.h"
#include "Components/SphereComponent.h"

ALightningPistol::ALightningPistol()
{
    PrimaryFireMode = EFireMode::Raycast;  
    SecondaryFireMode = EFireMode::Projectile;  
}

//void ALightningPistol::FireRaycast_Implementation(const FDamageInfo& Damage)
//{
//    // Стандартный пистолетный рейкаст
//    FVector Start = GetActorLocation();
//    FVector End = Start + (GetActorForwardVector() * 1000.0f); // Моделируем прямую линию выстрела
//
//    FHitResult HitResult;
//    FCollisionQueryParams Params;
//    Params.AddIgnoredActor(this);
//
//    if (GetWorld()->LineTraceSingleByChannel(HitResult, Start, End, ECC_Visibility, Params))
//    {
//        if (HitResult.GetActor()->Implements<UDamagable>())
//        {
//            FDamageInfo DamageInfo;
//            DamageInfo.DamageAmount = PrimaryDamage;
//            DamageInfo.DamageCauser = this;
//
//            IDamagable* DamagableActor = Cast<IDamagable>(HitResult.GetActor());
//            if (DamagableActor)
//            {
//                DamagableActor->TakeDamage(DamageInfo);
//            }
//        }
//    }
//
//    // Для отладки (линия выстрела)
//    DrawDebugLine(GetWorld(), Start, End, FColor::Green, false, 1.0f, 0, 1.0f);
//}

//void ALightningPistol::FireProjectile_Implementation(const FDamageInfo& Damage, TSubclassOf<AProjectileBase> ProjectileClass)
//{
//    // Молния - это проджектайл
//    if (ProjectileClass)
//    {
//        FVector SpawnLocation = GetActorLocation() + (GetActorForwardVector() * 100.0f); // Стартовая позиция для снаряда
//        FRotator SpawnRotation = GetActorRotation();
//
//        AProjectileBase* Projectile = GetWorld()->SpawnActor<AProjectileBase>(ProjectileClass, SpawnLocation, SpawnRotation);
//
//        if (Projectile)
//        {
//            
//
//            // Инициализация снаряда с урона
//            Projectile->InitializeProjectile(SecondaryDamage);
//        }
//    }
//}

////void ALightningPistol::FireLightning()
////{
////    // Начинаем наносить урон с интервалом
////    GetWorld()->GetTimerManager().SetTimer(LightningDamageTimer, this, &ALightningPistol::ApplyLightningDamage, LightningDamageInterval, true);
////}
////
////void ALightningPistol::ApplyLightningDamage()
////{
////    // Логика для молнии - проверка объектов в радиусе
////    FVector Start = GetActorLocation();
////    TArray<FOverlapResult> OverlapResults;
////    FCollisionShape Sphere = FCollisionShape::MakeSphere(LightningRadius);
////    FCollisionQueryParams Params;
////    Params.AddIgnoredActor(this);
////
////    // Найти все акторы в радиусе действия молнии
////    if (GetWorld()->OverlapMultiByChannel(OverlapResults, Start, FQuat::Identity, ECC_Visibility, Sphere, Params))
////    {
////        for (auto& Hit : OverlapResults)
////        {
////            AActor* HitActor = Hit.GetActor();
////            if (HitActor && HitActor->Implements<UDamagable>())
////            {
////                FDamageInfo DamageInfo;
////                DamageInfo.DamageAmount = LightningDamage;
////                DamageInfo.DamageCauser = this;
////
////                IDamagable* DamagableActor = Cast<IDamagable>(HitActor);
////                if (DamagableActor)
////                {
////                    DamagableActor->TakeDamage(DamageInfo);
////                }
////            }
////        }
////    }
////
////    // Для отладки (сфера молнии)
////    DrawDebugSphere(GetWorld(), Start, LightningRadius, 12, FColor::Blue, false, 1.0f, 0, 1.0f);
////}
////
////
