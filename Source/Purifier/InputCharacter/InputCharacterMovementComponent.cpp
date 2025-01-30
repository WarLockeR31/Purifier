// Fill out your copyright notice in the Description page of Project Settings.


#include "InputCharacterMovementComponent.h"

UInputCharacterMovementComponent::UInputCharacterMovementComponent()
{

}

void UInputCharacterMovementComponent::StartDash()
{
    if (MovementMode != MOVE_Custom)
    {
        SetMovementMode(MOVE_Custom, static_cast<uint8>(ECustomMovementMode::CMOVE_Dash));
    }
}

void UInputCharacterMovementComponent::StopDash()
{
    if (MovementMode == MOVE_Custom && CustomMovementMode == static_cast<uint8>(ECustomMovementMode::CMOVE_Dash))
    {
        SetMovementMode(MOVE_Walking); // Выход из рывка
    }
}

void UInputCharacterMovementComponent::PhysCustom(float DeltaTime, int32 Iterations)
{
    if (CustomMovementMode == static_cast<uint8>(ECustomMovementMode::CMOVE_Dash))
    {
        MoveUpdatedComponent(Velocity * DeltaTime, UpdatedComponent->GetComponentQuat(), true);
    }
    else
    {
        Super::PhysCustom(DeltaTime, Iterations);
    }
}