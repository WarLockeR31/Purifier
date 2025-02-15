// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "WeaponBase.h"
#include "LightningPistol.generated.h"

// Класс для пистолета с шаровой молнией
UCLASS()
class PURIFIER_API ALightningPistol : public AWeaponBase
{
    GENERATED_BODY()

public:
    ALightningPistol();

protected:
    // Переопределение FireRaycast для обычного пистолетного выстрела
    //virtual void FireRaycast_Implementation(const FDamageInfo& Damage) override;

    // Переопределение FireProjectile для выстрела с шаровой молнией
    //virtual void FireProjectile_Implementation(const FDamageInfo& Damage, TSubclassOf<AProjectileBase> ProjectileClass) override;

    // Логика для молнии, вызываемая через таймер
    //void FireLightning();

    //// Функция для нанесения урона молнии
    //void ApplyLightningDamage();
};
