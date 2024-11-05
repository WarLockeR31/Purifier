// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "InputCharacter.generated.h"



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
	bool bIsDashing;

	UPROPERTY(EditAnywhere, Category = "Dash") //???
	float DashDistance;

	UPROPERTY(EditAnywhere, Category = "Dash") //???
	float DashDuration;

	UPROPERTY(EditAnywhere, Category = "Dash") //???
	float DashCooldown;

	//Dash curve
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dash")
	UCurveFloat* DashCurve;

	//Dash timeline
	UPROPERTY()
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


};
