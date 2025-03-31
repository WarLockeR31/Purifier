// Fill out your copyright notice in the Description page of Project Settings.
#include "Drone.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "NavigationSystem.h"
#include "Purifier/Dash/BaseDashComponent.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "AIController.h"
#include "Navigation/PathFollowingComponent.h"

TArray<ADrone*> ADrone::AllDrones;

ADrone::ADrone()
{
    PrimaryActorTick.bCanEverTick = true;

    bUseControllerRotationYaw = false;
    MovementComponent = CreateDefaultSubobject<UFloatingPawnMovement>(TEXT("MovementComponent"));
}

void ADrone::BeginPlay()
{
    Super::BeginPlay();

    DashComponent = FindComponentByClass<UBaseDashComponent>();
    AIController = Cast<AAIController>(GetController());
    
    AllDrones.Add(this);
}

void ADrone::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    AllDrones.Remove(this);

    Super::EndPlay(EndPlayReason);
}

void ADrone::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // Здесь можно выполнять другие задачи, но flocking теперь обновляется исключительно через BT Task
    // Например, можно обновлять анимации, эффекты и прочее.
}

UBaseDashComponent* ADrone::GetDash()
{
    return DashComponent;
}

FVector ADrone::GetFlockingVector(float DeltaTime) const
{
    FVector CohesionVector = FVector::ZeroVector;
    FVector SeparationVector = FVector::ZeroVector;
    int NeighborCount = 0;

    // Перебираем только зарегистрированных дронов
    for (ADrone* OtherDrone : AllDrones)
    {
        if (OtherDrone == this) continue;

        float Distance = FVector::Dist(GetActorLocation(), OtherDrone->GetActorLocation());
        if (Distance < FlockRadius)
        {
            // Cohesion: стремимся быть рядом с соседями
            CohesionVector += OtherDrone->GetActorLocation();
            // Separation: избегаем слишком близкого расположения
            SeparationVector += (GetActorLocation() - OtherDrone->GetActorLocation()) / Distance;
            
            NeighborCount++;
        }
    }

    if (NeighborCount > 0)
    {
        CohesionVector = (CohesionVector / NeighborCount - GetActorLocation()).GetSafeNormal() * CohesionStrength;
        SeparationVector = SeparationVector.GetSafeNormal() * SeparationStrength;
    }

    // Итоговый вектор движения для flocking behavior
    FVector FlockMove = CohesionVector + SeparationVector;

    // Для отладки можно отрисовать вектор
    DrawDebugLine(GetWorld(), GetActorLocation(), GetActorLocation() + FlockMove * 100.f, FColor::Green, false, 0.1f, 0, 2.f);

    return FlockMove;
}

void ADrone::MoveToLocationWithFlocking(FVector TargetLocation, FVector FlockingVector)
{
    FAIMoveRequest MoveRequest;
    MoveRequest.SetAcceptanceRadius(50.0f);

    // Объединяем основную цель и смещение flocking
    FVector DesiredLocation = TargetLocation + FlockingVector;

    // Получаем навигационную систему
    UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld());
    if (NavSys)
    {
        FNavLocation ProjectedLocation;
        // Проецируем вычисленную точку на NavMesh с радиусом поиска 200 единиц
        if (NavSys->ProjectPointToNavigation(DesiredLocation, ProjectedLocation, FVector(200.f)))
        {
            MoveRequest.SetGoalLocation(DesiredLocation);
            //SetActorLocation(ProjectedLocation.Location);
        }
        else
        {
            MoveRequest.SetGoalLocation(TargetLocation);
            //SetActorLocation(TargetLocation);
        }
    }
    else
    {
        MoveRequest.SetGoalLocation(TargetLocation);
        //SetActorLocation(TargetLocation);
    }

    AIController->MoveTo(MoveRequest);
}

void ADrone::RotateTowards(const FVector& TargetPoint)
{
    // Вычисляем вектор направления
    FVector Direction = (TargetPoint - GetActorLocation()).GetSafeNormal();

    // Преобразуем его в угол поворота
    FRotator TargetRotation = Direction.Rotation();

    // Устанавливаем поворот дрона через AIController
    AIController->SetControlRotation(TargetRotation);
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

