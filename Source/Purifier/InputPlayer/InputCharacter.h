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

UENUM(BlueprintType)
enum class EWallRunEndReason : uint8 {
	FallOffWall = 0 UMETA(DisplayName = "FallOffWall"),
	JumpedOffWall = 1  UMETA(DisplayName = "JumpedOffWall"),
};

UCLASS()
class PURIFIER_API AInputCharacter : public ACharacter
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, meta = (AllowPrivateAccess = "true"))
	class UCameraComponent* Camera;

	UPROPERTY(VisibleAnywhere, Category = "Movement") //???
	FVector2D MoveInputVector;

	FTimerHandle DashHandle;
	FVector DashVector;
	float   DashSpeedCoefficient;

	UPROPERTY(VisibleAnywhere, Category = "WallRunning")
	FVector WallRunDirection;

	UPROPERTY(VisibleAnywhere, Category = "WallRunning")
	bool bWallRunning;

	UPROPERTY(VisibleAnywhere, Category = "WallRunning")
	EWallRunSide WallRunSide;

	UCapsuleComponent* CapsuleComponent;

	float BaseAirControl;
protected:
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

	UPROPERTY(EditAnywhere, Category = "Dash") //???
	bool bDashing;

	UPROPERTY(EditAnywhere, Category = "Dash") //???
	float DashDistance;

	UPROPERTY(EditAnywhere, Category = "Dash") //???
	float DashDuration;

	UPROPERTY(EditAnywhere, Category = "Dash") //???
	float DashCooldown;

	//Dash curve
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dash")
	UCurveFloat* DashCurve;

	//Dash curve
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WallRunning")
	UCurveFloat* WallRunCurve;

	//Dash timeline
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WallRunning")
	class UTimelineComponent* DashTimeline;

	

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

	//Dash timeline
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WallRunning")
	class UTimelineComponent* WallRunTimeline;
protected:
	void Move(const FInputActionValue& InputValue);
	void Look(const FInputActionValue& InputValue);
	void Jump();
	void StartDash();


	//Timeline tick
	UFUNCTION() //???
	void DashTimelineProgress(float Value);
	
	//Timeline End
	UFUNCTION() //???
	void OnDashFinished();
	void ResetDashCooldown();

	//Integrating curve
	float GetSpeedCoefficient() const;

	UFUNCTION()
	void OnCollisionHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	bool SurfaceIsWallRunnable(FVector SurfaceNormal);

	bool AreRequiredKeysDown() const;

	void StartWallRun();
	void UpdateWallRun(float Value);
	void EndWallRun();
};
