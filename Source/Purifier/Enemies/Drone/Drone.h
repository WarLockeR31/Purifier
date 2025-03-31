// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Purifier/Dash/Dashable.h"

#include "Drone.generated.h"

UCLASS()
class PURIFIER_API ADrone : public APawn, public IDashable
{
    GENERATED_BODY()

    FVector2D DashDirection;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movement", meta = (AllowPrivateAccess = "true"))
    class UFloatingPawnMovement* MovementComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movement", meta = (AllowPrivateAccess = "true"))
    class AAIController* AIController;

protected:
    UPROPERTY(EditAnywhere, Category = "Flocking")
    float Speed = 200.f;

    UPROPERTY(EditAnywhere, Category = "Flocking")
    float FlockRadius = 500.f;

    UPROPERTY(EditAnywhere, Category = "Flocking")
    float CohesionStrength = 1.0f;

    UPROPERTY(EditAnywhere, Category = "Flocking")
    float SeparationStrength = 1.0f;

    //Components
    UPROPERTY(VisibleAnywhere, Category = "Dash")
    class UBaseDashComponent* DashComponent;

public:
    ADrone();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void Tick(float DeltaTime) override;

    /* List of all drones */
    static TArray<ADrone*> AllDrones;


public:
    //Getters
    class UBaseDashComponent* GetDash();

    //Flocking
    FVector GetFlockingVector(float DeltaTime) const;
    void MoveToLocationWithFlocking(FVector TargetLocation, FVector FlockingVector);

    void RotateTowards(const FVector& TargetPoint);

    //Dash
    virtual void OnDashStart() override;
    virtual void OnDashEnd() override;

    virtual FVector GetMoveDirection() const override;  
    virtual FVector2D GetInputDirection() const override;

    void SetDashDirection(FVector2D NewDashDirection);
};
