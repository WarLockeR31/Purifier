// Fill out your copyright notice in the Description page of Project Settings.
#include "Drone.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "NavigationSystem.h"
#include "Purifier/Dash/BaseDashComponent.h"
#include "GameFramework/PawnMovementComponent.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "DroneAIController.h"
#include "Navigation/PathFollowingComponent.h"
#include "Components/SphereComponent.h"

TArray<ADrone*> ADrone::AllDrones;

ADrone::ADrone()
{
    PrimaryActorTick.bCanEverTick = true;

    //bUseControllerRotationYaw = false;
    
    SphereComponent = CreateDefaultSubobject<USphereComponent>(TEXT("SphereComponent"));
    SetRootComponent(SphereComponent); 

    AvoidanceCollider = CreateDefaultSubobject<USphereComponent>(TEXT("AvoidanceCollider"));
    AvoidanceCollider->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    AvoidanceCollider->SetHiddenInGame(true);

    MovementComponent = CreateDefaultSubobject<UFloatingPawnMovement>(TEXT("FloatingPawnMovement"));
}

void ADrone::BeginPlay()
{
    Super::BeginPlay();

    DashComponent = FindComponentByClass<UBaseDashComponent>();
    DroneAIController = Cast<ADroneAIController>(GetController());
    
    AllDrones.Add(this);
}

void ADrone::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    AllDrones.Remove(this);

    Super::EndPlay(EndPlayReason);
}

UBaseDashComponent* ADrone::GetDash()
{
    return DashComponent;
}

USphereComponent* ADrone::GetAvoidanceCollider() const
{
    return AvoidanceCollider;
}

#pragma region Dash

void ADrone::OnDashStart()
{
}

void ADrone::OnDashEnd()
{
}

FVector ADrone::GetMoveDirection() const
{
    return FVector();
}

FVector2D ADrone::GetInputDirection() const
{
    return DashDirection;
}

void ADrone::SetDashDirection(FVector2D NewDashDirection)
{
    DashDirection = NewDashDirection;
}

#pragma endregion

