// Fill out your copyright notice in the Description page of Project Settings.


#include "HandSwayComponent.h"
#include "Camera/CameraActor.h"
#include "Components/TimelineComponent.h"
#include <Kismet/KismetMathLibrary.h>

// Sets default values for this component's properties
UHandSwayComponent::UHandSwayComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	WalkingTimeline = CreateDefaultSubobject<UTimelineComponent>(TEXT("WalkingTimeline"));


	// ...
}


// Called when the game starts
void UHandSwayComponent::BeginPlay()
{
	Super::BeginPlay();

	if (AInputCharacter* OwnerCast = Cast<AInputCharacter>(GetOwner()))
	{
		OwnerInputCharacter = OwnerCast;
		HandsMesh = OwnerInputCharacter->GetMesh();
		InputCharacterMovementComponent = OwnerInputCharacter->GetInputCharacterMovement();
		MaxWalkSpeed = InputCharacterMovementComponent->MaxWalkSpeed;
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Incorrect owner of the HandSwayComponent"));
	}
	
	CameraRotationPrev = OwnerInputCharacter->Controller->GetControlRotation();

	FOnTimelineFloat WalkingProgress;
	WalkingProgress.BindUFunction(this, FName("UpdateWalkingHandSway"));
	WalkingTimeline->AddInterpFloat(WalkingLeftRightCurve, WalkingProgress);
	WalkingTimeline->SetLooping(true);
	WalkingTimeline->PlayFromStart();

	
}




// Called every frame
void UHandSwayComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	GetCameraPitch();
	HandsSway();
	AerialHandSway();
	// ...
}

float UHandSwayComponent::GetCameraPitch()
{
	FRotator Delta = UKismetMathLibrary::NormalizedDeltaRotator(OwnerInputCharacter->GetControlRotation(), OwnerInputCharacter->GetActorRotation());
	
	float Pitch = 2.f * (float)UKismetMathLibrary::NormalizeToRange(Delta.Pitch, -90.f, 90.f);
	
	PitchOffsetPos = FVector(0.f, FMath::Lerp(3.f, -3.f, Pitch), FMath::Lerp(2.f, -2.f, Pitch));
	
	Pitch = FMath::Clamp(Pitch, 0.f, 1.f);
	Pitch = FMath::Lerp(MaxDownPitch, 0.f, Pitch);
	
	FVector HandsPosition = FVector(Pitch + CameraHandsOffsetX, HandsMesh->GetRelativeLocation().Y, HandsMesh->GetRelativeLocation().Z);
	HandsMesh->SetRelativeLocation(HandsPosition);
	
	return 0.0f;
}

float UHandSwayComponent::HandsSway()
{
	CameraRotationCur = OwnerInputCharacter->Controller->GetControlRotation();
	FRotator Delta = UKismetMathLibrary::NormalizedDeltaRotator(CameraRotationCur, CameraRotationPrev);

	float Roll =  FMath::Clamp(Delta.Pitch * -1.f, -3.f, 3.f);
	float Yaw = FMath::Clamp(Delta.Yaw, -3.f, 3.f);
	FRotator Sway = FRotator(Roll, 0.f, Yaw);
	CameraRotationRate = FMath::RInterpTo(CameraRotationRate, Sway, GetWorld()->GetDeltaSeconds(), (1.f / GetWorld()->GetDeltaSeconds()) / 12.f);


	float CameraRotationOffsetX = FMath::Lerp(-10.f, 10.f, (float)UKismetMathLibrary::NormalizeToRange(CameraRotationRate.Roll, -5.f, 5.f));
	float CameraRotationOffsetZ = FMath::Lerp(-6.f, 6.f, (float)UKismetMathLibrary::NormalizeToRange(CameraRotationRate.Yaw, -5.f, 5.f));
	CameraRotationOffset = FVector(CameraRotationOffsetZ, 0.f, CameraRotationOffsetX);
	
	CameraRotationPrev = CameraRotationCur;
	return 0.0f;
}

void UHandSwayComponent::AerialHandSway()
{
	FRotator NewAerialTilt = FRotator(0.f, -2.f * LocationLagPos.Z, 0.f);
	FVector NewAerialOffset = FVector(-0.5f * LocationLagPos.Z, 0.f, 0.f);
	float InterpSpeed = (1.f / GetWorld()->GetDeltaSeconds()) / 12.f;

	AerialTilt = FMath::RInterpTo(AerialTilt, NewAerialTilt, GetWorld()->GetDeltaSeconds(), InterpSpeed);
	AerialOffset = FMath::VInterpTo(AerialOffset, NewAerialOffset, GetWorld()->GetDeltaSeconds(), InterpSpeed);
}

void UHandSwayComponent::UpdateWalkingHandSway(float Value)
{
	float CurTLTime = WalkingTimeline->GetPlaybackPosition();

	float WalkAnomOffsetX = FMath::Lerp(-0.4f, 0.4f, WalkingLeftRightCurve->GetFloatValue(CurTLTime));
	float WalkAnomOffsetZ = FMath::Lerp(-0.35f, 0.2f, WalkingUpDownCurve->GetFloatValue(CurTLTime));
	WalkAnimOffset.Set(WalkAnomOffsetX, 0.f, WalkAnomOffsetZ);

	WalkAnimTilt = FRotator(0.f, FMath::Lerp(1.f, -1.f, WalkingRollCurve->GetFloatValue(CurTLTime)), 0.f);

	float NormalizedVelocity = UKismetMathLibrary::NormalizeToRange(OwnerInputCharacter->GetVelocity().Length(), 0.f, MaxWalkSpeed);

	WalkAnimAlpha = (InputCharacterMovementComponent->IsFalling() ||
		(InputCharacterMovementComponent->MovementMode == MOVE_Custom &&
			InputCharacterMovementComponent->CustomMovementMode == static_cast<uint8>(ECustomMovementMode::CMOVE_Dash)))
		? 0.f
		: NormalizedVelocity;

	WalkingTimeline->SetPlayRate(FMath::Lerp(0.f, 1.65f, WalkAnimAlpha));
	UpdateLocationLagPos();
}

void UHandSwayComponent::UpdateLocationLagPos()
{
	const FVector Velocity = OwnerInputCharacter->GetVelocity();

	const FRotator Rotation = OwnerInputCharacter->Controller->GetControlRotation();
	const FRotator YawRotation(0, Rotation.Yaw, 0);

	const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
	const FVector UpDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Z);

	float ForwardVelocity = FVector::DotProduct(Velocity, ForwardDirection);
	float RightVelocity = FVector::DotProduct(Velocity, RightDirection);
	float UpVelocity = FVector::DotProduct(Velocity, OwnerInputCharacter->GetActorUpVector());

	FVector NewLocationLagPos = -2 * FVector(ForwardVelocity / MaxWalkSpeed, RightVelocity / MaxWalkSpeed, UpVelocity / InputCharacterMovementComponent->JumpZVelocity);
	NewLocationLagPos = NewLocationLagPos.GetClampedToSize(0.f, 6.f);

	LocationLagPos = FMath::VInterpTo(LocationLagPos, NewLocationLagPos, GetWorld()->GetDeltaSeconds(), (1.f / GetWorld()->GetDeltaSeconds()) / 9.f); //FVector::Dist(LocationLagPos, NewLocationLagPos)
}
