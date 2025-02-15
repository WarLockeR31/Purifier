// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "InputCharacterMovementComponent.generated.h"

UENUM(BlueprintType)
enum ECustomMovementMode : uint8
{
    CMOVE_None UMETA(Hidden),
    CMOVE_Dash UMETA(DisplayName = "Dash"),
    CMOVE_WallRun UMETA(DisplayName = "WallRun"),
    CMOVE_MAX UMETA(Hidden),
};

/**
 * 
 */
UCLASS()
class PURIFIER_API UInputCharacterMovementComponent : public UCharacterMovementComponent
{
    GENERATED_BODY() 

    UPROPERTY(EditDefaultsOnly) UCapsuleComponent* WallRunCollider;
    UPROPERTY(EditDefaultsOnly) float MinApproachSpeedForWallRun = 200.f;
    UPROPERTY(EditDefaultsOnly) float MinWallRunSpeed = 200.f;
    UPROPERTY(EditDefaultsOnly) float MaxWallRunSpeed = 800.f;
    UPROPERTY(EditDefaultsOnly) float MaxVerticalWallRunSpeed = 200.f;
    UPROPERTY(EditDefaultsOnly) float WallRunPullAwayAngle = 75;
    UPROPERTY(EditDefaultsOnly) float WallAttractionForce = 200.f;
    UPROPERTY(EditDefaultsOnly) float MinWallRunHeight = 50.f;
    UPROPERTY(EditDefaultsOnly) float TryWallRunTraceDisstance = 50.f;
    UPROPERTY(EditDefaultsOnly) UCurveFloat* WallRunGravityScaleCurve;
    UPROPERTY(EditDefaultsOnly) float WallJumpOffForce = 300.f;
    UPROPERTY(EditDefaultsOnly) float WallDetachForce = 100.f;
    UPROPERTY(EditDefaultsOnly) float MaxWallTurnAngle = 30.f;
    UPROPERTY(EditDefaultsOnly) float WallRunFriction = 1.f;

    UPROPERTY(EditDefaultsOnly) UCurveFloat* WallRunAccelerationUpCurve;
    UPROPERTY(EditDefaultsOnly) float AccelerationUpTime = 2.f;
    UPROPERTY(EditDefaultsOnly) float AccelerationUp = 5.f;
    UPROPERTY(EditDefaultsOnly) float FallStartTime = 3.f;
    UPROPERTY(EditDefaultsOnly) float WallRunSameSideCooldown = 3.f;
    UPROPERTY(EditDefaultsOnly) float MaxDashSlideAngle = 45.f;

    UPROPERTY(EditDefaultsOnly) float FallingGravityScale = 2.f;

    float BaseGravityScale = 1.f;

    class AInputCharacter* InputCharacterOwner;

    bool Safe_bWallRunIsLeft;

    bool bCanWallRunSameSide;

    float TimeFromStart;

    FTimerHandle WallRunSameSideCooldownHandle;

public:
    UInputCharacterMovementComponent();
   
    // Actor Component
protected:
    virtual void InitializeComponent() override;
    virtual void BeginPlay() override;

    // Events
private:
    DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE_OneParam(FComponentWallRelativeRotationChangedSignature, UInputCharacterMovementComponent, OnComponentWallRelativeRotationChanged, float, NewAlpha);

public:
    UPROPERTY(BlueprintAssignable, Category = "WallRun")
    FComponentWallRelativeRotationChangedSignature OnComponentWallRelativeRotationChanged;

    // Character Movement Component
public:
    virtual float GetMaxSpeed() const override;
    virtual float GetMaxBrakingDeceleration() const override;
    virtual bool CanAttemptJump() const override;
    virtual bool DoJump(bool bReplayingMoves) override;



public:
    virtual void UpdateCharacterStateBeforeMovement(float DeltaSeconds) override;

protected:
    virtual void PhysCustom(float DeltaTime, int32 Iterations) override;
    virtual void OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode) override;



    // Wall Run
private:
    bool TryWallRun();
    //WallRun helper
    bool SurfaceIsWallRunnable(const FVector SurfaceNormal) const;
    virtual void PhysFalling(float deltaTime, int32 Iterations) override;
    void PhysWallRun(float deltaTime, int32 Iterations);

    // Interface
public:
    UFUNCTION(BlueprintPure) bool IsCustomMovementMode(ECustomMovementMode InCustomMovementMode) const;
    UFUNCTION(BlueprintPure) bool IsMovementMode(EMovementMode InMovementMode) const;
    UFUNCTION(BlueprintPure) bool IsWallRunning() const { return IsCustomMovementMode(CMOVE_WallRun); }
    UFUNCTION(BlueprintPure) bool IsDashing() const { return IsCustomMovementMode(CMOVE_Dash); }
    UFUNCTION(BlueprintPure) bool WallRunningIsRight() const { return Safe_bWallRunIsLeft; }
    

public:
    void StartDash();
    void StopDash();

    // Helpers
private:
    float CapR() const;
    float CapHH() const;
    bool TraceToWall(FHitResult& OutHit) const;
    float CalculateRelativeRotationAlpha() const;
    void ResetWallRunSameSideCooldown();
};