// Fill out your copyright notice in the Description page of Project Settings.


#include "WeaponComponent.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"

UWeaponComponent::UWeaponComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UWeaponComponent::BeginPlay()
{
    Super::BeginPlay();

    AActor* Owner = GetOwner();
    if (Owner)
    {
        OwnerMesh = Owner->FindComponentByClass<USkeletalMeshComponent>();
    }

    if (AvailableWeapons.Num() > 0)
    {
        SelectWeapon(0); // ¬ыбираем первое оружие по умолчанию
    }
}

void UWeaponComponent::FirePrimary()
{
    if (CurrentWeapon)
    {
        CurrentWeapon->FirePrimary();
    }
}

void UWeaponComponent::FireSecondary()
{
    if (CurrentWeapon)
    {
        CurrentWeapon->FireSecondary();
    }
}

void UWeaponComponent::SelectWeapon(int32 WeaponIndex)
{
    if (AvailableWeapons.IsValidIndex(WeaponIndex))
    {
        if (CurrentWeapon)
        {
            CurrentWeapon->Destroy();
            CurrentWeapon = nullptr;
        }

        AWeaponBase* NewWeapon = GetWorld()->SpawnActor<AWeaponBase>(AvailableWeapons[WeaponIndex]);
        if (NewWeapon)
        {
            EquipWeapon(NewWeapon);
            CurrentWeaponIndex = WeaponIndex;
        }
    }
}

void UWeaponComponent::EquipWeapon(AWeaponBase* NewWeapon)
{
    if (!NewWeapon) return;

    NewWeapon->AttachToComponent(OwnerMesh, FAttachmentTransformRules::SnapToTargetIncludingScale, "GripPoint");
    CurrentWeapon = NewWeapon;
}
