// Fill out your copyright notice in the Description page of Project Settings.


#include "InputCharacterWeaponComponent.h"
#include "InputCharacter.h"
#include "Camera/CameraComponent.h"

void UInputCharacterWeaponComponent::BeginPlay()
{
	Super::BeginPlay();

	if (AInputCharacter* OwnerCast = Cast<AInputCharacter>(GetOwner()))
	{
		OwnerInputCharacter = OwnerCast;
        OwnerInputCharacterCamera = OwnerInputCharacter->GetCamera();
        AController* OwnerInputCharacterController = OwnerInputCharacter->GetController();
        if (!OwnerInputCharacterController->IsPlayerController())
        {
            UE_LOG(LogTemp, Error, TEXT("OwnerInputCharacterController is not PlayerController"));
        }
        OwnerPlayerController = Cast<APlayerController>(OwnerInputCharacterController);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Incorrect owner of the InputCharacterWeaponComponent"));
	}
}

FVector UInputCharacterWeaponComponent::CorrectRaycastPosition_Implementation(FVector OldLocation) const
{
    FVector CameraLocation = OwnerInputCharacterCamera->GetComponentLocation();
    
    // Проецируем мировые координаты в экранные (основной FOV)
    FVector2D ScreenPositionOldFOV;  
    OwnerPlayerController->ProjectWorldLocationToScreen(OldLocation, ScreenPositionOldFOV);

    // Смещение точки относительно центра экрана 
    int32 ScreenWidth, ScreenHeight;
    OwnerPlayerController->GetViewportSize(ScreenWidth, ScreenHeight);
    FVector2D ScreenCenter = FVector2D(ScreenWidth / 2.0f, ScreenHeight / 2.0f);
    FVector2D OffsetFromCenter = ScreenPositionOldFOV - ScreenCenter;

    // FOV
    float OldFOV = OwnerInputCharacterCamera->FieldOfView;  
    float NewFOV = OwnerInputCharacterCamera->FirstPersonFieldOfView;  

    // Вычисляем коэффициент масштабирования FOV
    float OldFovRad = FMath::DegreesToRadians(OldFOV);
    float NewFovRad = FMath::DegreesToRadians(NewFOV);
    float ScaleFactor = FMath::Tan(OldFovRad / 2.0f) / FMath::Tan(NewFovRad / 2.0f);

    // Масштабируем смещение от центра экрана
    FVector2D NewOffsetFromCenter = OffsetFromCenter * ScaleFactor;

    // Вычисляем новые экранные координаты
    FVector2D ScreenPositionNewFOV = ScreenCenter + NewOffsetFromCenter;

    // Проецируем экранные координаты в мировые
    FVector WorldLocationNewFOV;
    FVector WorldDirectionNewFOV;
    OwnerPlayerController->DeprojectScreenPositionToWorld(ScreenPositionNewFOV.X, ScreenPositionNewFOV.Y, WorldLocationNewFOV, WorldDirectionNewFOV);
    return WorldLocationNewFOV + WorldDirectionNewFOV * (FVector::Dist(OldLocation, CameraLocation));
}
