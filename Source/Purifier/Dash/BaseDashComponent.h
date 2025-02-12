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
	TScriptInterface<IDashable> OwnerDashable;

	APawn* OwnerPawn;
	bool bIsDashing;

public:	
	// Sets default values for this component's properties
	UBaseDashComponent();

protected:
	virtual void BeginPlay() override;

	void SetOwners();

	

public:	
	UFUNCTION()
	virtual void StartDash() PURE_VIRTUAL(StartDash, );
	UFUNCTION()
	virtual void OnDashEnd() PURE_VIRTUAL(OnDashEnd, );
	UFUNCTION()
	virtual void CancelDash() PURE_VIRTUAL(CancelDash, );
	UFUNCTION()
	virtual bool IsInstantDash() const PURE_VIRTUAL(IsInstantDash, return false;);
};
