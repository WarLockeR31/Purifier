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
	
	
}




// Called every frame
void UHandSwayComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	GetCameraPitch();

	// ...
}

float UHandSwayComponent::GetCameraPitch()
{
	FRotator Delta = UKismetMathLibrary::NormalizedDeltaRotator(Owner->GetControlRotation(), Owner->GetActorRotation());
	
	float Pitch = 2.f * (float)UKismetMathLibrary::NormalizeToRange(Delta.Pitch, -90.f, 90.f);
	Pitch = FMath::Clamp(Pitch, 0.f, 1.f);
	Pitch = FMath::Lerp(MaxDownPitch, 0.f, Pitch);
	
	
	FVector HandsPosition = FVector(Pitch + HandsOffsetX, hands->GetRelativeLocation().Y, hands->GetRelativeLocation().Z);
	hands->SetRelativeLocation(HandsPosition);

	GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Green, FString::Printf(TEXT("%f"), Pitch));
	return 0.0f;
}
