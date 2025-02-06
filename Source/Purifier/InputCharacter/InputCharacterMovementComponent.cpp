// Fill out your copyright notice in the Description page of Project Settings.


#include "InputCharacterMovementComponent.h"

UInputCharacterMovementComponent::UInputCharacterMovementComponent()
{

}

void UInputCharacterMovementComponent::StartDash()
{
    SetMovementMode(MOVE_Custom, static_cast<uint8>(ECustomMovementMode::CMOVE_Dash));
}

void UInputCharacterMovementComponent::StopDash()
{
    SetMovementMode(MOVE_Walking); // Выход из рывка
}

void UInputCharacterMovementComponent::StartWallRun()
{
    SetMovementMode(MOVE_Custom, static_cast<uint8>(ECustomMovementMode::CMOVE_WallRun));
}

void UInputCharacterMovementComponent::StopWallRun()
{
    SetMovementMode(MOVE_Falling); // Выход из рывка
}

void UInputCharacterMovementComponent::PhysCustom(float DeltaTime, int32 Iterations)
{
    if (CustomMovementMode == static_cast<uint8>(ECustomMovementMode::CMOVE_Dash) || CustomMovementMode == static_cast<uint8>(ECustomMovementMode::CMOVE_WallRun))
    {
        MoveUpdatedComponent(Velocity * DeltaTime, UpdatedComponent->GetComponentQuat(), true);
    }
    else
    {
        Super::PhysCustom(DeltaTime, Iterations);
    }
}