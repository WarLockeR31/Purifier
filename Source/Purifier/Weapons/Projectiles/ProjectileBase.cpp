// Fill out your copyright notice in the Description page of Project Settings.

#include "ProjectileBase.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Components/PrimitiveComponent.h"

AProjectileBase::AProjectileBase()
{
    PrimaryActorTick.bCanEverTick = false;

    ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
    ProjectileMovement->InitialSpeed = ProjectileSpeed;
    ProjectileMovement->MaxSpeed = ProjectileSpeed;
    ProjectileMovement->bRotationFollowsVelocity = true;
    ProjectileMovement->bShouldBounce = false;
}

void AProjectileBase::BeginPlay()
{
    Super::BeginPlay();

    if (CollisionComponent)
    {
        CollisionComponent->OnComponentHit.AddDynamic(this, &AProjectileBase::OnHit);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("ProjectileBase: CollisionComponent wasn't linked!"));
    }
}

void AProjectileBase::InitializeProjectile(const FDamageInfo& InDamageInfo)
{
    DamageInfo = InDamageInfo;
}

void AProjectileBase::OnHit_Implementation(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
    if (OtherActor && OtherActor->Implements<UDamagable>())
    {
        DamageInfo.KnockbackDirection = (OtherActor->GetActorLocation() - GetActorLocation()).GetSafeNormal();
        DamageInfo.DamageCauser = GetOwner();

        IDamagable* DamagableActor = Cast<IDamagable>(OtherActor);
        if (DamagableActor)
        {
            DamagableActor->TakeDamage(DamageInfo);
        }
    }

    Destroy(); 
}
