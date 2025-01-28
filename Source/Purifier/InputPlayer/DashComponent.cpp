// Fill out your copyright notice in the Description page of Project Settings.


#include "Purifier/InputPlayer/DashComponent.h"

#include "GameFramework/CharacterMovementComponent.h"
#include "Components/TimelineComponent.h"

// Sets default values for this component's properties
UDashComponent::UDashComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	DashTimeline = CreateDefaultSubobject<UTimelineComponent>(TEXT("DashTimeline"));
}


// Called when the game starts
void UDashComponent::BeginPlay()
{
	Super::BeginPlay();

	if (ACharacter* OwnerCast = Cast<ACharacter>(GetOwner()))
	{
		Owner = OwnerCast;
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Incorrect owner of the HandSwayComponent"));
	}

	

}


// Called every frame
void UDashComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}


void UDashComponent::StartDash()
{
	if (bDashing)
		return;

	//GetCharacterMovement()->StopMovementImmediately();
	bDashing = true;
	Owner->GetCharacterMovement()->BrakingFrictionFactor = 0.f;


	const FRotator Rotation = Owner->Controller->GetControlRotation();
	const FRotator YawRotation(0, Rotation.Yaw, 0);

	const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	DashVector = (ForwardDirection * 1 + RightDirection * 0).GetSafeNormal();
	//DashVector = (ForwardDirection * MoveInputVector.Y + RightDirection * MoveInputVector.X).GetSafeNormal();

	//Timeline start
	DashTimeline->PlayFromStart();
}

void UDashComponent::DashTimelineProgress(float Value)
{
	Owner->LaunchCharacter(DashVector * Value * DashSpeedCoefficient, true, true);
}

void UDashComponent::OnDashFinished()
{
	Owner->LaunchCharacter(DashVector * DashDistance * 50.f, true, true);
	Owner->GetCharacterMovement()->BrakingFrictionFactor = 2.f;

	Owner->GetWorldTimerManager().SetTimer(DashHandle, this, &UDashComponent::ResetDashCooldown, DashCooldown, false);
}

void UDashComponent::ResetDashCooldown()
{
	bDashing = false;
}

float UDashComponent::GetSpeedCoefficient() const
{
	float MinTime, MaxTime;
	DashCurve->GetTimeRange(MinTime, MaxTime);

	float step = (MaxTime - MinTime) / 200.f;
	float ApproximateCurveS = 0.f;
	for (float i = MinTime; i < MaxTime; i += step)
	{
		ApproximateCurveS += DashCurve->GetFloatValue(i + step) * step;
	}

	return DashDistance / ApproximateCurveS / DashDuration * 100.f;
}

FVector2D UDashComponent::GetMoveInputVector()
{
	return FVector2D();
}

