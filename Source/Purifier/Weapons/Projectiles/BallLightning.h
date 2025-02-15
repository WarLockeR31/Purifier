// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ProjectileBase.h"
#include "BallLightning.generated.h"

UCLASS()
class PURIFIER_API ABallLightning : public AProjectileBase
{
    GENERATED_BODY()

public:
    ABallLightning();

protected:
    virtual void BeginPlay() override;

    // Регулярное нанесение урона
    void ApplyLightningDamage();

    // Радиус, в котором молния наносит урон
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Lightning")
    float LightningRadius = 300.0f;

    // Интервал времени между нанесением урона молнией
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Lightning")
    float LightningDamageInterval = 0.5f;

    // Таймер для вызова функции нанесения урона
    FTimerHandle LightningDamageTimer;

public:

    virtual void OnHit_Implementation(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit) override;
};
