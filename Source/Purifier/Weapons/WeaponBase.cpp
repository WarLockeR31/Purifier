// Fill out your copyright notice in the Description page of Project Settings.


#include "WeaponBase.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "DrawDebugHelpers.h"
#include "Purifier/DamageSystem/Damagable.h"

#include "Camera/CameraComponent.h"
#include "Purifier/CustomCollisionChannels.h"
#include "WeaponComponent.h"

AWeaponBase::AWeaponBase()
{
    PrimaryActorTick.bCanEverTick = false;

    WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh"));
    RootComponent = WeaponMesh;
}

void AWeaponBase::FirePrimary_Implementation()
{
    if (bPrimaryFireOnCooldown)
        return;
    bPrimaryFireOnCooldown = true;

    if (PrimaryFireMode == EFireMode::Raycast)
    {
        FireRaycast(PrimaryDamage);
    }
    else if (PrimaryFireMode == EFireMode::Projectile && PrimaryProjectileClass)
    {
        FireProjectile(PrimaryDamage, PrimaryProjectileClass);
    }

    FTimerHandle TimerHandle;
    FTimerDelegate TimerDelegate;
    TimerDelegate.BindLambda([this]() { bPrimaryFireOnCooldown = false; });
    GetWorld()->GetTimerManager().SetTimer(TimerHandle, TimerDelegate, PrimaryFireCooldown, false);
}

void AWeaponBase::FireSecondary_Implementation()
{
    if (bSecondaryFireOnCooldown)
        return;
    bSecondaryFireOnCooldown = true;

    if (SecondaryFireMode == EFireMode::Raycast)
    {
        FireRaycast(SecondaryDamage);
    }
    else if (SecondaryFireMode == EFireMode::Projectile && SecondaryProjectileClass)
    {
        FireProjectile(SecondaryDamage, SecondaryProjectileClass);
    }

    FTimerHandle TimerHandle;
    FTimerDelegate TimerDelegate;
    TimerDelegate.BindLambda([this]() { bSecondaryFireOnCooldown = false; });
    GetWorld()->GetTimerManager().SetTimer(TimerHandle, TimerDelegate, SecondaryFireCooldown, false);
}

void AWeaponBase::FireRaycast_Implementation(const FDamageInfo& Damage)
{
    if (!OwnerWeaponComponent) return;


    FVector MuzzleLocation = OwnerWeaponComponent->CorrectRaycastPosition(WeaponMesh->GetSocketLocation("Muzzle"));
    
    FVector TargetLocation; FHitResult HitResult;

    if (FindTargetLocation(TargetLocation, HitResult))
    {
        AActor* HitActor = HitResult.GetActor();
        if (HitActor && HitActor->Implements<UDamagable>())
        {
            IDamagable::Execute_TakeDamage(HitActor, Damage);
        }
    }
    
    //DrawDebugLine(GetWorld(), Start, End, FColor::Blue, false, 1.0f, 0, 2.0f);
    DrawDebugLine(GetWorld(), MuzzleLocation, TargetLocation, FColor::Red, false, 1.0f, 0, 2.0f);
}

void AWeaponBase::FireProjectile_Implementation(const FDamageInfo& Damage, TSubclassOf<AProjectileBase> ProjectileClass)
{
    if (!ProjectileClass) return;

    FVector SpawnLocation = OwnerWeaponComponent->CorrectRaycastPosition(WeaponMesh->GetSocketLocation("Muzzle"));


    FVector TargetLocation; FHitResult HitResult;
    FindTargetLocation(TargetLocation, HitResult);
    FRotator SpawnRotation = FRotationMatrix::MakeFromX(TargetLocation - SpawnLocation).Rotator();

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

bool AWeaponBase::FindTargetLocation(FVector& TargetLocation, FHitResult& FinalHitResult) const 
{
    FVector Start = OwnerWeaponComponent->GetRaycastStartPoint()->GetComponentLocation();
    FVector ForwardVector = OwnerWeaponComponent->GetRaycastStartPoint()->GetComponentRotation().Vector();
    FVector End = Start + (ForwardVector * 5000.0f);

    FHitResult HitResult;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this);
    Params.AddIgnoredActor(GetOwner());

    bool bHit = GetWorld()->LineTraceSingleByChannel(HitResult, Start, End, ECollisionChannel(ECustomCollision::PlayerProjectile), Params);


    End = bHit ? HitResult.ImpactPoint : End;

    FVector MuzzleLocation = OwnerWeaponComponent->CorrectRaycastPosition(WeaponMesh->GetSocketLocation("Muzzle"));
    FVector MuzzleDirection = (End - MuzzleLocation).GetSafeNormal();
    FVector FinalEnd = MuzzleLocation + (MuzzleDirection * 5000.0f);

    bool bFinalHit = GetWorld()->LineTraceSingleByChannel(FinalHitResult, MuzzleLocation, FinalEnd, ECollisionChannel(ECustomCollision::PlayerProjectile), Params);
    TargetLocation = bFinalHit ? FinalHitResult.Location : End;
    return bFinalHit;
}
