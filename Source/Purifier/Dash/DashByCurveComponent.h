// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BaseDashComponent.h"
#include "DashByCurveComponent.generated.h"


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class PURIFIER_API UDashByCurveComponent : public UBaseDashComponent
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category = "Dash")
	FTimerHandle DashHandle;

	UPROPERTY(VisibleAnywhere, Category = "Dash")
	FVector DashVector;

	UPROPERTY(VisibleAnywhere, Category = "Dash")
	float DashSpeedCoefficient;

	UPROPERTY(VisibleAnywhere, Category = "Dash")
	class UTimelineComponent* DashTimeline;

protected:
	UPROPERTY(EditAnywhere, Category = "Dash")
	float DashDistance;

	UPROPERTY(EditAnywhere, Category = "Dash")
	float DashDuration;

	UPROPERTY(EditAnywhere, Category = "Dash")
	float DashCooldown;

	//Dash curve
	UPROPERTY(EditAnywhere, Category = "Dash")
	UCurveFloat* DashCurve;
public:	
	// Sets default values for this component's properties
	UDashByCurveComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;
	
	UFUNCTION()
	virtual void DashTimelineProgress(float Value);
	virtual void OnDashEnd() override;
	void ResetDashCooldown();
	
	//Integrating curve
	float GetSpeedCoefficient() const;

	virtual FVector2D GetMoveInputVector();


public:
	//Dash
	virtual void StartDash() override;

	

	virtual void CancelDash() override;

	virtual bool IsInstantDash() const override;


};
