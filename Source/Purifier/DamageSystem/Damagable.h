// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Damagable.generated.h"

USTRUCT(BlueprintType)
struct FDamageInfo
{
    GENERATED_USTRUCT_BODY()

public:
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Damage")
    float DamageAmount;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Damage")
    AActor* DamageCauser;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Damage")
    bool bKnockback;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Damage")
    FVector KnockbackDirection;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Damage")
    bool bStun;

    FDamageInfo()
        : DamageAmount(0.f), DamageCauser(nullptr), bKnockback(false), KnockbackDirection(FVector::ZeroVector), bStun(false)
    {}
};

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UDamagable : public UInterface
{
    GENERATED_BODY()
};

/**
 * Интерфейс для объектов, получающих урон
 */
class PURIFIER_API IDamagable
{
    GENERATED_BODY()

public:
    // Возвращает текущее здоровье
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Damagable")
    float GetCurrentHealth() const;
    virtual float GetCurrentHealth_Implementation() const;

    // Возвращает максимальное здоровье
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Damagable")
    float GetMaxHealth() const;
    virtual float GetMaxHealth_Implementation() const;

    // Лечит объект
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Damagable")
    float Heal(float HealAmount);
    virtual float Heal_Implementation(float HealAmount);

    // Наносит урон
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Damagable")
    float TakeDamage(const FDamageInfo& DamageInfo);
    virtual float TakeDamage_Implementation(const FDamageInfo& DamageInfo);
};