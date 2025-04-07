// Fill out your copyright notice in the Description page of Project Settings.


#include "DroneAIController.h"
#include "Drone.h"
#include "NavigationSystem.h"
#include "Navigation/PathFollowingComponent.h"
#include "GameFramework/PawnMovementComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Components/SphereComponent.h"
#include "Engine/EngineTypes.h"

ADroneAIController::ADroneAIController()
{
    PrimaryActorTick.bCanEverTick = true;
}

void ADroneAIController::BeginPlay()
{
    Super::BeginPlay();

    ControlledDrone = Cast<ADrone>(GetPawn());
}

void ADroneAIController::IdleWithFlocking(float DeltaTime)
{
    TArray<ADrone*> Neighbors = ControlledDrone->GetAllDrones();

    FVector Separation = CalculateSeparationForce(Neighbors) * SeparationWeight;
    FVector Cohesion = CalculateCohesionForce(Neighbors) * CohesionWeight;
    FVector Avoidance = CalculateObstacleAvoidanceForce() * AvoidanceWeight;

    FVector TotalForce;
    if ((Separation + Cohesion).Size() < IdleAcceptance)
    {
        TotalForce = Avoidance;
    }
    else
    {
        TotalForce = Separation + Cohesion + Avoidance;
    }

    TotalForce = TotalForce.GetClampedToMaxSize(MaxForce);



    FVector FlockingVelocity = IdleFlockingVelocityCoefficient * TotalForce * DeltaTime;
    FlockingVelocity = FlockingVelocity.GetClampedToMaxSize(MaxSpeed);

    ControlledDrone->AddMovementInput(FlockingVelocity.GetSafeNormal());
}

void ADroneAIController::MoveToLocationWithFlocking(FVector& TargetLocation, float DeltaTime)
{
    FVector FlockingVelocity = CalculateFlockingForce() * DeltaTime;
    FVector ToTargetVelocity = (TargetLocation - ControlledDrone->GetActorLocation()).GetSafeNormal() * DeltaTime;
    FVector TotalVelocity = FlockingVelocityCoefficient * FlockingVelocity + ToTargetVelocityCoefficient * ToTargetVelocity;

    TotalVelocity = TotalVelocity.GetClampedToMaxSize(MaxSpeed);
    //UE_LOG(LogTemp, Warning, TEXT("TotalVel: %s"), *TotalVelocity.ToString());

    ControlledDrone->AddMovementInput(TotalVelocity.GetSafeNormal());
    //ControlledDrone->GetMovementComponent()->MoveUpdatedComponent(TotalVelocity, ControlledDrone->GetMovementComponent()->UpdatedComponent->GetComponentRotation(), true);
    //ControlledDrone->GetMovementComponent()->Velocity = TotalVelocity;
    //ControlledDrone->GetMovementComponent()->UpdateComponentVelocity();
}

void ADroneAIController::RotateTowards(const FVector& TargetPoint, float DeltaTime)
{
    FVector Direction = (TargetPoint - ControlledDrone->GetActorLocation()).GetSafeNormal();

    FRotator TargetRotation = Direction.Rotation();
    //UE_LOG(LogTemp, Warning, TEXT("TotalVel: %s"), *TargetRotation.ToString());
    
    ControlledDrone->SetActorRotation(FMath::RInterpTo(ControlledDrone->GetActorRotation(), TargetRotation, DeltaTime, 7.0f));
  
    //ControlledDrone->SetActorRotation(TargetRotation);
    // SetControlRotation(TargetRotation);
}

FVector ADroneAIController::CalculateFlockingForce() const
{
    //TArray<AActor*> Neighbors = FindNeighborDrones();
    TArray<ADrone*> Neighbors = ControlledDrone->GetAllDrones();

    FVector Separation = CalculateSeparationForce(Neighbors) * SeparationWeight;
    FVector Cohesion = CalculateCohesionForce(Neighbors) * CohesionWeight;
    FVector Alignment = CalculateAlignmentForce(Neighbors) * AlignmentWeight;
    FVector Avoidance = CalculateObstacleAvoidanceForce() * AvoidanceWeight;

    FVector TotalForce = Separation + Cohesion + Alignment + Avoidance;
    TotalForce = TotalForce.GetClampedToMaxSize(MaxForce);

    return TotalForce;
}

FVector ADroneAIController::CalculateSeparationForce(const TArray<ADrone*>& Neighbors) const
{
    FVector Force = FVector::ZeroVector;
    FVector CurrentLocation = GetPawn()->GetActorLocation();

    for (AActor* Neighbor : Neighbors)
    {
        FVector ToNeighbor = CurrentLocation - Neighbor->GetActorLocation();
        float Distance = ToNeighbor.Size();
        if (Distance > 0)
        {
            Force += ToNeighbor.GetSafeNormal() / Distance; // Чем ближе, тем сильнее отталкивание
        }
    }

    return Force;
}

FVector ADroneAIController::CalculateCohesionForce(const TArray<ADrone*>& Neighbors) const
{
    if (Neighbors.IsEmpty())
        return FVector::ZeroVector;

    FVector CenterOfMass = FVector::ZeroVector;
    for (ADrone* Neighbor : Neighbors)
    {
        CenterOfMass += Neighbor->GetActorLocation();
    }
    CenterOfMass /= Neighbors.Num();

    return (CenterOfMass - GetPawn()->GetActorLocation());
}

FVector ADroneAIController::CalculateAlignmentForce(const TArray<ADrone*>& Neighbors) const
{
    if (Neighbors.IsEmpty())
        return FVector::ZeroVector;

    FVector AvgVelocity = FVector::ZeroVector;
    for (ADrone* Neighbor : Neighbors)
    {
        AvgVelocity += Neighbor->GetVelocity();
    }
    AvgVelocity /= Neighbors.Num();

    return AvgVelocity;
}

FVector ADroneAIController::CalculateObstacleAvoidanceForce() const
{
    FVector Force = FVector::ZeroVector;
    FVector Start = GetPawn()->GetActorLocation();
    FRotator VelocityRotation = ControlledDrone->GetMovementComponent()->Velocity.ToOrientationRotator();

    // Проверка лучей: вперед, влево, вправо, вверх, вниз
    TArray<FVector> RayDirections = {
        VelocityRotation.Vector(),
        (VelocityRotation + FRotator(0, -30, 0)).Vector(),
        (VelocityRotation + FRotator(0, 30, 0)).Vector(),
        (VelocityRotation + FRotator(30, 0, 0)).Vector(),
        (VelocityRotation + FRotator(-30, 0, 0)).Vector()
    };

    TArray<FHitResult> Hits;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(GetPawn());

    TArray <AActor*> ActorsToIgnore;
    ActorsToIgnore.Add(GetPawn());

    for (FVector& Dir : RayDirections)
    {
        FVector End = Start + Dir * AvoidanceRayLength;
        FHitResult Hit;

        //// 1. Рисуем луч
        //DrawDebugLine(
        //    GetWorld(),
        //    Start,
        //    End,
        //    FColor::Green,
        //    false, // bPersistentLines (не сохранять между кадрами)
        //    0,
        //    0, // DepthPriority (0 = обычная)
        //    2
        //);

        bool bHit = UKismetSystemLibrary::SphereTraceSingle(
            GetWorld(), 
            Start, 
            End, 
            ControlledDrone->GetAvoidanceCollider()->GetScaledSphereRadius(), 
            UEngineTypes::ConvertToTraceType(ECC_Visibility), 
            false, 
            ActorsToIgnore, 
            EDrawDebugTrace::None, 
            Hit, 
            true
        );

        if (bHit)
        {
            // Рассчитать силу избегания (перпендикулярно нормали препятствия)
            FVector AvoidDir = Hit.ImpactNormal;
            Force += AvoidDir * (1.0f - Hit.Time); // Hit.Time = [0,1]
        }
    }

    return Force;
}
