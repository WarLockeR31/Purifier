// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "DroneAIController.generated.h"

/**
 * 
 */
UCLASS()
class PURIFIER_API ADroneAIController : public AAIController
{
	GENERATED_BODY()
	
    UPROPERTY(VisibleAnywhere)
    class ADrone* ControlledDrone;

public:
    ADroneAIController();
    virtual void BeginPlay() override;


    UPROPERTY(EditDefaultsOnly, Category = "Flocking")
    float NeighborhoodRadius = 1000.0f; // Радиус поиска соседей

    UPROPERTY(EditDefaultsOnly, Category = "Flocking")
    float SeparationWeight = 1.5f;      // Вес разделения

    UPROPERTY(EditDefaultsOnly, Category = "Flocking")
    float CohesionWeight = 1.0f;        // Вес сплочения

    UPROPERTY(EditDefaultsOnly, Category = "Flocking")
    float AlignmentWeight = 1.0f;       // Вес выравнивания

    UPROPERTY(EditDefaultsOnly, Category = "Avoidance")
    float AvoidanceWeight = 2.0f;       // Вес избегания препятствий

    UPROPERTY(EditDefaultsOnly, Category = "Avoidance")
    float AvoidanceRayLength = 500.0f;  // Длина лучей для обнаружения

    // Максимальная скорость и сила
    UPROPERTY(EditDefaultsOnly, Category = "Movement")
    float MaxSpeed = 800.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Movement")
    float MaxForce = 200.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Movement")
    float FlockingVelocityCoefficient = 0.5f;

    UPROPERTY(EditDefaultsOnly, Category = "Movement")
    float ToTargetVelocityCoefficient = 0.5f;

    UPROPERTY(EditDefaultsOnly, Category = "Movement")
    float IdleFlockingVelocityCoefficient = 0.5f;

    UPROPERTY(EditDefaultsOnly, Category = "Movement")
    float IdleAcceptance = 0.5f;

    // Цель движения (можно задать через Blackboard)
    UPROPERTY(EditInstanceOnly, Category = "AI")
    AActor* TargetActor;


    void MoveToLocationWithFlocking(FVector& TargetLocation, float DeltaTime);
    void IdleWithFlocking(float DeltaTime);
    void RotateTowards(const FVector& TargetPoint, float DeltaTime);

protected:
    FVector CalculateFlockingForce() const;

    // Рассчитать силы Flocking
    FVector CalculateSeparationForce(const TArray<ADrone*>& Neighbors) const;
    FVector CalculateCohesionForce(const TArray<ADrone*>& Neighbors) const;
    FVector CalculateAlignmentForce(const TArray<ADrone*>& Neighbors) const;

    // Рассчитать силу избегания препятствий
    FVector CalculateObstacleAvoidanceForce() const;

    // Найти соседних дронов
    //TArray<AActor*> FindNeighborDrones() const;
};
