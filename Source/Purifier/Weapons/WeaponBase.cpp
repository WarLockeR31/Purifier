// Fill out your copyright notice in the Description page of Project Settings.


#include "WeaponBase.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "DrawDebugHelpers.h"
#include "Purifier/DamageSystem/Damagable.h"
#include "WeaponComponent.h"

AWeaponBase::AWeaponBase()
{
    PrimaryActorTick.bCanEverTick = false;

    WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh"));
    RootComponent = WeaponMesh;
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
    if (!OwnerWeaponComponent) return;

    FVector Start = OwnerWeaponComponent->GetRaycastStartPoint()->GetComponentLocation();
    FVector ForwardVector = OwnerWeaponComponent->GetRaycastStartPoint()->GetComponentRotation().Vector();
    FVector End = Start + (ForwardVector * 5000.0f);

    FHitResult HitResult;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this);
    Params.AddIgnoredActor(GetOwner());

    bool bHit = GetWorld()->LineTraceSingleByChannel(HitResult, Start, End, ECC_Visibility, Params);
    if (!bHit)
    {
        return;
    }

    End = HitResult.ImpactPoint;

    FVector MuzzleLocation = GetMesh()->GetSocketLocation("Muzzle");
    FVector MuzzleDirection = (End - MuzzleLocation).GetSafeNormal();
    FVector FinalEnd = MuzzleLocation + (MuzzleDirection * 5000.0f);

    FHitResult FinalHitResult;
    bool bFinalHit = GetWorld()->LineTraceSingleByChannel(FinalHitResult, MuzzleLocation, FinalEnd, ECC_Visibility, Params);

    if (!bFinalHit)
    {
        return;
    }

    AActor* HitActor = FinalHitResult.GetActor();
    if (HitActor && HitActor->Implements<UDamagable>())
    {
        IDamagable::Execute_TakeDamage(HitActor, Damage);
    }
    
    //DrawDebugLine(GetWorld(), Start, End, FColor::Blue, false, 1.0f, 0, 2.0f);
    DrawDebugLine(GetWorld(), MuzzleLocation, FinalEnd, FColor::Red, false, 1.0f, 0, 2.0f);
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