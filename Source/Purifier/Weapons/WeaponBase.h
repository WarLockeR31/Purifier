// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Projectiles/ProjectileBase.h"
#include "WeaponBase.generated.h"

UENUM(BlueprintType)
enum class EFireMode : uint8
{
    Raycast    UMETA(DisplayName = "Raycast"),
    Projectile UMETA(DisplayName = "Projectile")
};

UCLASS(Abstract, Blueprintable)
class PURIFIER_API AWeaponBase : public AActor
{
    GENERATED_BODY()

public:
    AWeaponBase();

    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Weapon")
    void FirePrimary();
    virtual void FirePrimary_Implementation();

    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Weapon")
    void FireSecondary();
    virtual void FireSecondary_Implementation();

protected:
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Weapon")
    EFireMode PrimaryFireMode;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Weapon")
    EFireMode SecondaryFireMode;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Weapon", meta = (EditCondition = "PrimaryFireMode == EFireMode::Projectile", EditConditionHides))
    TSubclassOf<AProjectileBase> PrimaryProjectileClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Weapon", meta = (EditCondition = "SecondaryFireMode == EFireMode::Projectile", EditConditionHides))
    TSubclassOf<AProjectileBase> SecondaryProjectileClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Weapon")
    FDamageInfo PrimaryDamage;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Weapon")
    FDamageInfo SecondaryDamage;

    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Weapon")
    void FireRaycast(const FDamageInfo& Damage);
    virtual void FireRaycast_Implementation(const FDamageInfo& Damage);

    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Weapon")
    void FireProjectile(const FDamageInfo& Damage, TSubclassOf<AProjectileBase> ProjectileClass);
    virtual void FireProjectile_Implementation(const FDamageInfo& Damage, TSubclassOf<AProjectileBase> ProjectileClass);
};
