// Fill out your copyright notice in the Description page of Project Settings.


#include "DashByCurveComponent.h"
#include "Components/TimelineComponent.h"
#include "GameFramework/PawnMovementComponent.h"

// Sets default values for this component's properties
UDashByCurveComponent::UDashByCurveComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	DashTimeline = CreateDefaultSubobject<UTimelineComponent>(TEXT("DashTimeline"));
}


// Called when the game starts
void UDashByCurveComponent::BeginPlay()
{
	Super::BeginPlay();

	FOnTimelineFloat DashProgress;
	DashProgress.BindUFunction(this, FName("DashTimelineProgress"));
	DashTimeline->AddInterpFloat(DashCurve, DashProgress);
	DashTimeline->SetPlayRate(1.f / DashDuration);

	FOnTimelineEvent TimelineFinishedCallback;
	TimelineFinishedCallback.BindUFunction(this, FName("OnDashEnd"));
	DashTimeline->SetTimelineFinishedFunc(TimelineFinishedCallback);

	DashSpeedCoefficient = GetSpeedCoefficient();
}

void UDashByCurveComponent::StartDash()
{
	if (bIsDashing)
		return;

	bIsDashing = true;
	OwnerDashable->OnDashStart();

	const FRotator Rotation = OwnerPawn->Controller->GetControlRotation();
	const FRotator YawRotation(0, Rotation.Yaw, 0);

	const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	FVector2D DashDirection = GetMoveInputVector();
	DashVector = (ForwardDirection * DashDirection.Y + RightDirection * DashDirection.X).GetSafeNormal();

	//Timeline start
	DashTimeline->PlayFromStart();
}

void UDashByCurveComponent::DashTimelineProgress(float Value)
{
	OwnerPawn->GetMovementComponent()->Velocity = DashVector * Value * DashSpeedCoefficient;
}

void UDashByCurveComponent::OnDashEnd()
{
	OwnerPawn->GetMovementComponent()->Velocity = DashVector * DashDistance * 50.f;
	OwnerDashable->OnDashEnd();

	OwnerPawn->GetWorldTimerManager().SetTimer(DashHandle, this, &UDashByCurveComponent::ResetDashCooldown, DashCooldown, false);
}

void UDashByCurveComponent::ResetDashCooldown()
{
	bIsDashing = false;
}

float UDashByCurveComponent::GetSpeedCoefficient() const
{
	float MinTime, MaxTime;
	DashCurve->GetTimeRange(MinTime, MaxTime);

	float step = (MaxTime - MinTime) / 500.f;
	float ApproximateCurveS = 0.f;
	for (float i = MinTime; i < MaxTime; i += step)
	{
		ApproximateCurveS += DashCurve->GetFloatValue(i + step) * step;
	}

	return DashDistance / ApproximateCurveS / DashDuration * 100.f;
}

FVector2D UDashByCurveComponent::GetMoveInputVector()
{
	return OwnerDashable->GetInputDirection();
}

void UDashByCurveComponent::CancelDash()
{
	DashTimeline->Stop();
	OwnerDashable->OnDashEnd();
	OwnerPawn->GetWorldTimerManager().SetTimer(DashHandle, this, &UDashByCurveComponent::ResetDashCooldown, DashCooldown, false);
}

bool UDashByCurveComponent::IsInstantDash() const
{
	return false;
}
