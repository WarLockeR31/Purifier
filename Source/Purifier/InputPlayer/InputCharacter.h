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

	// ��������� � �������� ������� �����
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

	// ������ ��� ��������� �������� �����
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dash")
	UCurveFloat* DashCurve;

	// �������� ��� �������� �����
	UPROPERTY()
	class UTimelineComponent* DashTimeline;

	//// ��������� �������� �����
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


	// �������, ���������� ��� ������ �����
	void StartDash();

	// �������, ���������� ������ ��� ��� ���������� `Timeline`
	UFUNCTION()
	void DashTimelineProgress(float Value);

	// �������, ���������� ����� ���������� �����
	UFUNCTION()
	void OnDashFinished();

};
