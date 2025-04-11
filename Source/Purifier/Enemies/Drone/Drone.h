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

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
    class ADroneAIController* DroneAIController;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
    class USphereComponent* AvoidanceCollider;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
    class USphereComponent* SphereComponent;

protected:
    UPROPERTY(EditAnywhere, Category = "Flocking")
    float Speed = 200.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
    class UFloatingPawnMovement* MovementComponent;

    //Components
    UPROPERTY(VisibleAnywhere, Category = "Dash")
    class UBaseDashComponent* DashComponent;

public:
    ADrone();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    /* List of all drones */
    static TArray<ADrone*> AllDrones;

public:
    //Getters
    class UBaseDashComponent* GetDash();

    UFUNCTION(BlueprintCallable)
    TArray<ADrone*> GetAllDrones() { return AllDrones; };
    class USphereComponent* GetAvoidanceCollider() const;

    //Dash
    virtual void OnDashStart() override;
    virtual void OnDashEnd() override;

    virtual FVector GetMoveDirection() const override;  
    virtual FVector2D GetInputDirection() const override;

    void SetDashDirection(FVector2D NewDashDirection);
};
