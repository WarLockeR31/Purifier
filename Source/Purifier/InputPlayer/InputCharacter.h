// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "InputCharacter.generated.h"

UENUM(BlueprintType)
enum class EWallRunSide : uint8 {
	Left = 0 UMETA(DisplayName = "LEFT"),
	Right = 1  UMETA(DisplayName = "RIGHT"),
};

//UENUM(BlueprintType)
//enum class EWallRunEndReason : uint8 {
//	FallOffWall = 0 UMETA(DisplayName = "FallOffWall"),
//	JumpedOffWall = 1  UMETA(DisplayName = "JumpedOffWall"),
//};

UCLASS()
class PURIFIER_API AInputCharacter : public ACharacter
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, meta = (AllowPrivateAccess = "true"))
	class UCameraComponent* Camera;


	UPROPERTY(VisibleAnywhere, Category = "EnhancedInput")
	FVector2D MoveInputVector;

#pragma region Dash
	UPROPERTY(VisibleAnywhere, Category = "Dash")
	FTimerHandle DashHandle;

	UPROPERTY(VisibleAnywhere, Category = "Dash")
	FVector DashVector;

	UPROPERTY(VisibleAnywhere, Category = "Dash")
	float DashSpeedCoefficient;

	UPROPERTY(VisibleAnywhere, Category = "Dash")
	bool bDashing;

	UPROPERTY(VisibleAnywhere, Category = "Dash")
	class UTimelineComponent* DashTimeline;
#pragma endregion Dash

#pragma region WallRun
	UPROPERTY(VisibleAnywhere, Category = "WallRun")
	FVector WallRunDirection;

	UPROPERTY(VisibleAnywhere, Category = "WallRun")
	bool bWallRunning;

	UPROPERTY(VisibleAnywhere, Category = "WallRun")
	EWallRunSide WallRunSide;

	UPROPERTY(VisibleAnywhere, Category = "WallRun")
	float BaseAirControl;

	UPROPERTY(VisibleAnywhere, Category = "WallRun")
	class UTimelineComponent* WallRunTimeline;
#pragma endregion WallRun

protected:

#pragma region Input
	UPROPERTY(EditAnywhere, Category = "EnhancedInput")
	class UInputMappingContext* InputMapping;

	UPROPERTY(EditAnywhere, Category = "EnhancedInput")
	class UInputAction* MoveAction;

	UPROPERTY(EditAnywhere, Category = "EnhancedInput")
	class UInputAction* JumpAction;

	UPROPERTY(EditAnywhere, Category = "EnhancedInput")
	class UInputAction* LookAction;

	UPROPERTY(EditAnywhere, Category = "EnhancedInput")
	class UInputAction* DashAction;
#pragma endregion Input

#pragma region Dash
	UPROPERTY(EditAnywhere, Category = "Dash") 
	float DashDistance;

	UPROPERTY(EditAnywhere, Category = "Dash") 
	float DashDuration;

	UPROPERTY(EditAnywhere, Category = "Dash") 
	float DashCooldown;

	//Dash curve
	UPROPERTY(EditAnywhere, Category = "Dash")
	UCurveFloat* DashCurve;
#pragma endregion Dash

public:
	// Sets default values for this character's properties
	AInputCharacter();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	

protected:
	void Move(const FInputActionValue& InputValue);
	void Look(const FInputActionValue& InputValue);
	void Jump();

	//Dash
	void StartDash();
	UFUNCTION()
	void DashTimelineProgress(float Value);
	UFUNCTION()
	void OnDashFinished();
	void ResetDashCooldown();
	//Integrating curve
	float GetSpeedCoefficient() const;

	//WallRun trigger
	UFUNCTION() 
	void OnCollisionHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);
	//WallRun helper
	bool SurfaceIsWallRunnable(const FVector SurfaceNormal) const;
	//WallRun helper
	bool AreRequiredKeysDown() const;

	//WallRun
	void StartWallRun();
	UFUNCTION()
	void UpdateWallRun();
	void EndWallRun();
};
