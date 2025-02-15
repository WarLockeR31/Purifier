// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Purifier/DamageSystem/Damagable.h"
#include "ProjectileBase.generated.h"

UCLASS(Abstract, Blueprintable)
class PURIFIER_API AProjectileBase : public AActor
{
    GENERATED_BODY()

public:
    AProjectileBase();

    // Позволяет переопределить InitializeProjectile в Blueprint
    UFUNCTION(BlueprintCallable, Category = "Projectile")
    virtual void InitializeProjectile(const FDamageInfo& InDamageInfo);

protected:
    virtual void BeginPlay() override;

    // Делаем DamageInfo доступным для Blueprints
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Projectile")
    FDamageInfo DamageInfo;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile")
    float ProjectileSpeed = 2000.0f;

    // Составляющие для движения и коллизий
    UPROPERTY(VisibleDefaultsOnly, BlueprintReadWrite, Category = "Projectile", meta = (AllowPrivateAccess = "true"))
    class UProjectileMovementComponent* ProjectileMovement;

    UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Projectile", meta = (AllowPrivateAccess = "true"))
    UPrimitiveComponent* CollisionComponent;

    // Переопределяемую функцию при попадании с коллайдером
    UFUNCTION(BlueprintNativeEvent, Category = "Projectile")
    void OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);
    virtual void OnHit_Implementation(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

};
