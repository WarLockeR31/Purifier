// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "HandSwayComponent.generated.h"


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class PURIFIER_API UHandSwayComponent : public UActorComponent
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category = "HandsSway")
	class AInputCharacter* OwnerInputCharacter;

	UPROPERTY(VisibleAnywhere, Category = "HandsSway")
	USkeletalMeshComponent* HandsMesh;

	UPROPERTY()
	class UInputCharacterMovementComponent* InputCharacterMovementComponent;

protected:

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "HandsSway")
	FVector PitchOffsetPos;

	UPROPERTY(VisibleAnywhere, Category = "HandsSway")
	FRotator CameraRotationPrev;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "HandsSway")
	FRotator CameraRotationCur;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "HandsSway")
	FRotator CameraRotationRate;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "HandsSway")
	FVector CameraRotationOffset;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "HandsSway")
	FRotator AerialTilt;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "HandsSway")
	FVector AerialOffset;
	
	UPROPERTY(EditAnywhere, Category = "HandsSway")
	float CameraHandsOffsetX = -10.f;

	UPROPERTY(EditAnywhere, Category = "HandsSway")
	float MaxDownPitch = 10.f;

#pragma region Walking

	UPROPERTY(VisibleAnywhere, Category = "Walking")
	class UTimelineComponent* WalkingTimeline;

	UPROPERTY(EditAnywhere, Category = "Walking")
	UCurveFloat* WalkingLeftRightCurve;

	UPROPERTY(EditAnywhere, Category = "Walking")
	UCurveFloat* WalkingUpDownCurve;

	UPROPERTY(EditAnywhere, Category = "Walking")
	UCurveFloat* WalkingRollCurve;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movement")
	FVector WalkAnimOffset;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movement")
	FRotator WalkAnimTilt;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movement")
	float WalkAnimAlpha;

#pragma endregion Walking

	UPROPERTY(EditAnywhere, Category = "Movement")
	float MaxWalkSpeed;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movement")
	FVector LocationLagPos;

	

public:
	

public:	
	// Sets default values for this component's properties
	UHandSwayComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

	float GetCameraPitch();

	float HandsSway();

	void AerialHandSway();

	UFUNCTION()
	void UpdateWalkingHandSway(float Value);

public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

public:
	void UpdateLocationLagPos(); //???
};
