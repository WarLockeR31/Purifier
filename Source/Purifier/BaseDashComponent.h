// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Dashable.h"
#include "BaseDashComponent.generated.h"


UCLASS(Abstract, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class PURIFIER_API UBaseDashComponent : public UActorComponent
{
	GENERATED_BODY()

protected:
	// Указатель на владельца, реализующего интерфейс IDashable
	TScriptInterface<IDashable> OwnerDashable;

	APawn* OwnerPawn;

	// Флаг, указывающий, активен ли рывок
	bool bIsDashing;

public:	
	// Sets default values for this component's properties
	UBaseDashComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

	void SetOwners();

public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// Начало рывка
	UFUNCTION()
	virtual void StartDash();

	// Завершение рывка
	virtual void EndDash();
};
