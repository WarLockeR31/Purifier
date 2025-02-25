// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WeaponBase.h"
#include "WeaponComponent.generated.h"

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class PURIFIER_API UWeaponComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UWeaponComponent();

    virtual void BeginPlay() override;

    void FirePrimary();
    void FireSecondary();
    void SelectWeapon(int32 WeaponIndex);

    UFUNCTION(BlueprintCallable, Category = "Weapons")
    void SetRaycastStartPoint(USceneComponent* NewPoint);
    USceneComponent* GetRaycastStartPoint();

private:
    UPROPERTY(EditDefaultsOnly, Category = "Weapons")
    TArray<TSubclassOf<AWeaponBase>> AvailableWeapons;

    UPROPERTY(VisibleAnywhere, Category = "Weapons")
    TObjectPtr<USceneComponent> RaycastStartPoint;

    UPROPERTY()
    AWeaponBase* CurrentWeapon;

    int32 CurrentWeaponIndex = -1;

    UPROPERTY(EditDefaultsOnly, Category = "Weapons")
    USkeletalMeshComponent* OwnerMesh;

    void EquipWeapon(AWeaponBase* NewWeapon);
};