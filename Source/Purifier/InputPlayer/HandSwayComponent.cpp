// Fill out your copyright notice in the Description page of Project Settings.


#include "HandSwayComponent.h"
#include "Camera/CameraActor.h"
#include <Kismet/KismetMathLibrary.h>

// Sets default values for this component's properties
UHandSwayComponent::UHandSwayComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}


// Called when the game starts
void UHandSwayComponent::BeginPlay()
{
	Super::BeginPlay();

	if (AInputCharacter* OwnerCast = Cast<AInputCharacter>(GetOwner()))
	{
		Owner = OwnerCast;
		hands = Owner->GetMesh();
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Incorrect owner of the HandSwayComponent"));
	}
	
	CameraRotationPrev = Owner->Controller->GetControlRotation();
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
	FRotator Delta = UKismetMathLibrary::NormalizedDeltaRotator(Owner->GetControlRotation(), Owner->GetActorRotation());
	
	float Pitch = 2.f * (float)UKismetMathLibrary::NormalizeToRange(Delta.Pitch, -90.f, 90.f);
	
	PitchOffsetPos = FVector(0.f, FMath::Lerp(3.f, -3.f, Pitch), FMath::Lerp(2.f, -2.f, Pitch));
	
	
	Pitch = FMath::Clamp(Pitch, 0.f, 1.f);
	Pitch = FMath::Lerp(MaxDownPitch, 0.f, Pitch);
	
	
	FVector HandsPosition = FVector(Pitch + CameraHandsOffsetX, hands->GetRelativeLocation().Y, hands->GetRelativeLocation().Z);
	hands->SetRelativeLocation(HandsPosition);

	
	return 0.0f;
}

float UHandSwayComponent::HandsSway()
{
	CameraRotationCur = Owner->Controller->GetControlRotation();
	FRotator Delta = UKismetMathLibrary::NormalizedDeltaRotator(CameraRotationCur, CameraRotationPrev);

	float Roll =  FMath::Clamp(Delta.Pitch * -1.f, -5.f, 5.f);
	float Yaw = FMath::Clamp(Delta.Yaw, -5.f, 5.f);
	//GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Green, FString::Printf(TEXT("%f   %f"), Roll, Yaw));
	FRotator Sway = FRotator(Roll, 0.f, Yaw);
	CameraRotationRate = FMath::RInterpTo(CameraRotationRate, Sway, GetWorld()->GetDeltaSeconds(), (1.f / GetWorld()->GetDeltaSeconds()) / 6.f);


	float CameraRotationOffsetX = FMath::Lerp(-10.f, 10.f, (float)UKismetMathLibrary::NormalizeToRange(CameraRotationRate.Roll, -5.f, 5.f));
	float CameraRotationOffsetZ = FMath::Lerp(-6.f, 6.f, (float)UKismetMathLibrary::NormalizeToRange(CameraRotationRate.Yaw, -5.f, 5.f));
	CameraRotationOffset = FVector(CameraRotationOffsetZ, 0.f, CameraRotationOffsetX);
	
	CameraRotationPrev = CameraRotationCur;
	return 0.0f;
}

void UHandSwayComponent::AerialHandSway()
{
	FRotator NewAerialTilt = FRotator(0.f, -2.f * Owner->GetLocationLagPos().Z, 0.f);
	FVector NewAerialOffset = FVector(-0.5f * Owner->GetLocationLagPos().Z, 0.f, 0.f);
	float InterpSpeed = (1.f / GetWorld()->GetDeltaSeconds()) / 12.f;

	AerialTilt = FMath::RInterpTo(AerialTilt, NewAerialTilt, GetWorld()->GetDeltaSeconds(), InterpSpeed);
	AerialOffset = FMath::VInterpTo(AerialOffset, NewAerialOffset, GetWorld()->GetDeltaSeconds(), InterpSpeed);
	GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Green, FString::Printf(TEXT("%f   %f"), AerialTilt.Yaw, AerialOffset.X));
}


