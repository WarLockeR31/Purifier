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
	

public:	
	// Sets default values for this component's properties
	UHandSwayComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

	float GetCameraPitch();

public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

		
};
