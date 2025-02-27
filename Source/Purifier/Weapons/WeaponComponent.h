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

protected:
    UPROPERTY(VisibleAnywhere, Category = "Weapons")
    AActor* Owner;

    UPROPERTY(EditDefaultsOnly, Category = "Weapons")
    TArray<TSubclassOf<AWeaponBase>> AvailableWeapons;

    UPROPERTY(VisibleAnywhere, Category = "Weapons")
    TObjectPtr<USceneComponent> RaycastStartPoint;

    UPROPERTY()
    AWeaponBase* CurrentWeapon;

    int32 CurrentWeaponIndex = -1;

    UPROPERTY(EditDefaultsOnly, Category = "Weapons")
    USkeletalMeshComponent* OwnerMesh;



public:
    UWeaponComponent();

    virtual void BeginPlay() override;

    void FirePrimary();
    void FireSecondary();
    void SelectWeapon(int32 WeaponIndex);

    UFUNCTION(BlueprintCallable, Category = "Weapons")
    void SetRaycastStartPoint(USceneComponent* NewPoint);
    USceneComponent* GetRaycastStartPoint();

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Weapons")
    FVector CorrectRaycastPosition(FVector OldPosition) const;

protected:
    virtual FVector CorrectRaycastPosition_Implementation(FVector OldLocation) const;


private:
    

    void EquipWeapon(AWeaponBase* NewWeapon);
};