// Fill out your copyright notice in the Description page of Project Settings.


#include "HealthComponent.h"
#include "Damagable.h"
#include "GameFramework/Actor.h"

UHealthComponent::UHealthComponent()
{
    PrimaryComponentTick.bCanEverTick = false;

    MaxHealth = 100.0f;
    CurrentHealth = MaxHealth;
    bIsDead = false;
}

void UHealthComponent::BeginPlay()
{
    Super::BeginPlay();

    OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);
}

float UHealthComponent::Heal(float HealAmount)
{
    if (bIsDead || HealAmount <= 0.0f)
    {
        return CurrentHealth;
    }

    CurrentHealth = FMath::Clamp(CurrentHealth + HealAmount, 0.0f, MaxHealth);
    OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);

    return CurrentHealth;
}

float UHealthComponent::TakeDamage(const FDamageInfo& DamageInfo)
{
    if (bIsDead || DamageInfo.DamageAmount <= 0.0f)
    {
        return CurrentHealth;
    }

    CurrentHealth = FMath::Clamp(CurrentHealth - DamageInfo.DamageAmount, 0.0f, MaxHealth);
    OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);

    if (CurrentHealth <= 0.0f)
    {
        bIsDead = true;
        OnDeath.Broadcast(GetOwner());
    }

    return CurrentHealth;
}