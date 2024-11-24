// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InputPlayer/InputCharacter.h"	

#include "HandSwayComponent.generated.h"


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class PURIFIER_API UHandSwayComponent : public UActorComponent
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category = "HandsSway")
	AInputCharacter* Owner;

	UPROPERTY(VisibleAnywhere, Category = "HandsSway")
	USkeletalMeshComponent* hands;

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
	
public:
	UPROPERTY(EditAnywhere, Category = "HandsSway")
	float CameraHandsOffsetX;

	UPROPERTY(EditAnywhere, Category = "HandsSway")
	float MaxDownPitch;

public:	
	// Sets default values for this component's properties
	UHandSwayComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

	float GetCameraPitch();

	float HandsSway();

public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

		
};
