// Fill out your copyright notice in the Description page of Project Settings.


#include "WeaponBase.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "DrawDebugHelpers.h"
#include "Purifier/DamageSystem/Damagable.h"

AWeaponBase::AWeaponBase()
{
    PrimaryActorTick.bCanEverTick = false;
}

void AWeaponBase::FirePrimary_Implementation()
{
    if (PrimaryFireMode == EFireMode::Raycast)
    {
        FireRaycast(PrimaryDamage);
    }
    else if (PrimaryFireMode == EFireMode::Projectile && PrimaryProjectileClass)
    {
        FireProjectile(PrimaryDamage, PrimaryProjectileClass);
    }
}

void AWeaponBase::FireSecondary_Implementation()
{
    if (SecondaryFireMode == EFireMode::Raycast)
    {
        FireRaycast(SecondaryDamage);
    }
    else if (SecondaryFireMode == EFireMode::Projectile && SecondaryProjectileClass)
    {
        FireProjectile(SecondaryDamage, SecondaryProjectileClass);
    }
}

void AWeaponBase::FireRaycast_Implementation(const FDamageInfo& Damage)
{
    FVector Start = GetActorLocation();
    FVector ForwardVector = GetActorForwardVector();
    FVector End = Start + (ForwardVector * 5000.0f); 

    FHitResult HitResult;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this); 
    Params.AddIgnoredActor(GetOwner());

    bool bHit = GetWorld()->LineTraceSingleByChannel(HitResult, Start, End, ECC_Visibility, Params);

    if (bHit)
    {
        AActor* HitActor = HitResult.GetActor();
        if (HitActor && HitActor->Implements<UDamagable>())
        {
            GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Blue, "GG");
            IDamagable::Execute_TakeDamage(HitActor, Damage);
        }

        // Опционально: добавить эффект попадания
        //UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), ImpactEffect, HitResult.ImpactPoint);
    }

    DrawDebugLine(GetWorld(), Start, End, FColor::Red, false, 1.0f, 0, 2.0f);
}

void AWeaponBase::FireProjectile_Implementation(const FDamageInfo& Damage, TSubclassOf<AProjectileBase> ProjectileClass)
{
    if (!ProjectileClass) return;

    FVector SpawnLocation = GetActorLocation();
    FRotator SpawnRotation = GetActorRotation();

    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = GetOwner();
    SpawnParams.Instigator = GetInstigator();

    AProjectileBase* Projectile = GetWorld()->SpawnActorDeferred<AProjectileBase>(ProjectileClass, FTransform(SpawnRotation, SpawnLocation), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);

    if (Projectile)
    {
        Projectile->InitializeProjectile(Damage); 
        Projectile->FinishSpawning(FTransform(SpawnRotation, SpawnLocation));
    }
}