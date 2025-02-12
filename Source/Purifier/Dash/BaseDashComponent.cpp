// Fill out your copyright notice in the Description page of Project Settings.


#include "BaseDashComponent.h"

// Sets default values for this component's properties
UBaseDashComponent::UBaseDashComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}


// Called when the game starts
void UBaseDashComponent::BeginPlay()
{
    Super::BeginPlay();

	SetOwners();
}

void UBaseDashComponent::SetOwners()
{
	if (APawn* OwnerCast = Cast<APawn>(GetOwner()))
	{
		OwnerPawn = OwnerCast;
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Owner of DashComponent is not Pawn"));
	}

	if (GetOwner()->GetClass()->ImplementsInterface(UDashable::StaticClass()))
	{
		OwnerDashable = TScriptInterface<IDashable>(GetOwner());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Owner of DashComponent does not implement IDashable interface"));
	}
}