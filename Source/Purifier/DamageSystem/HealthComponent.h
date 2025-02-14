// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HealthComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDeath, AActor*, DeadActor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHealthChanged, float, CurrentHealth, float, MaxHealth);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class PURIFIER_API UHealthComponent : public UActorComponent
{
    GENERATED_BODY()

private:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Health", meta = (AllowPrivateAccess = "true"))
    float CurrentHealth;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Health", meta = (AllowPrivateAccess = "true"))
    float MaxHealth;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Health", meta = (AllowPrivateAccess = "true"))
    bool bIsDead;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health", meta = (AllowPrivateAccess = "true"))
    bool bCanBeStunned;

public:
    UHealthComponent();

protected:
    virtual void BeginPlay() override;

    //Functions
public:
    UFUNCTION(BlueprintCallable, Category = "Health")
    float Heal(float HealAmount);

    UFUNCTION(BlueprintCallable, Category = "Health")
    float TakeDamage(const struct FDamageInfo& DamageInfo);

    //Getters
public:
    UFUNCTION(BlueprintCallable, Category = "Health")
    bool IsDead() const { return bIsDead; }

    UFUNCTION(BlueprintCallable, Category = "Health")
    bool CanBeStunned() const { return bCanBeStunned; }

    UFUNCTION(BlueprintCallable, Category = "Health")
    float GetCurrentHealth() const { return CurrentHealth; }

    UFUNCTION(BlueprintCallable, Category = "Health")
    float GetMaxHealth() const { return MaxHealth; }

    //Delegates
public:
    UPROPERTY(BlueprintAssignable, Category = "Health")
    FOnDeath OnDeath;

    UPROPERTY(BlueprintAssignable, Category = "Health")
    FOnHealthChanged OnHealthChanged;
};