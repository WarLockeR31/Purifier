// Fill out your copyright notice in the Description page of Project Settings.


#include "Damagable.h"

float IDamagable::GetCurrentHealth_Implementation() const
{
    UE_LOG(LogTemp, Error, TEXT("GetCurrentHealth() is not implemented in %s!"), *GetNameSafe(Cast<AActor>(this)));
    return 0.0f;
}

float IDamagable::GetMaxHealth_Implementation() const
{
    UE_LOG(LogTemp, Error, TEXT("GetMaxHealth() is not implemented in %s!"), *GetNameSafe(Cast<AActor>(this)));
    return 0.0f;
}

float IDamagable::Heal_Implementation(float HealAmount)
{
    UE_LOG(LogTemp, Error, TEXT("Heal() is not implemented in %s!"), *GetNameSafe(Cast<AActor>(this)));
    return 0.0f;
}

float IDamagable::TakeDamage_Implementation(const FDamageInfo& DamageInfo)
{
    UE_LOG(LogTemp, Error, TEXT("TakeDamage() is not implemented in %s!"), *GetNameSafe(Cast<AActor>(this)));
    return 0.0f;
}