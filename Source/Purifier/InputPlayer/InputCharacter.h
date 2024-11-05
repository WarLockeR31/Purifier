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

	//???
	FTimerHandle DashHandle;

	UPROPERTY(VisibleAnywhere, Category = "Movement") //???
	FVector2D MoveInputVector;

	// Начальная и конечная позиции рывка
	FVector DashStartLocation;
	FVector DashEndLocation;

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
	float DashCooldown;

	// Кривая для изменения скорости рывка
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dash")
	UCurveFloat* DashCurve;

	// Таймлайн для контроля рывка
	UPROPERTY()
	class UTimelineComponent* DashTimeline;

	//// Начальная скорость рывка
	//UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dash")
	//float DashSpeed;

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


	// Функция, вызываемая для начала рывка
	void StartDash();

	// Функция, вызываемая каждый раз при обновлении `Timeline`
	UFUNCTION()
	void DashTimelineProgress(float Value);

	// Функция, вызываемая после завершения рывка
	UFUNCTION()
	void OnDashFinished();

};
