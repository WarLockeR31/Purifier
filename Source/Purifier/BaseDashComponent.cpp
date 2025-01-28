// Fill out your copyright notice in the Description page of Project Settings.


#include "BaseDashComponent.h"

// Sets default values for this component's properties
UBaseDashComponent::UBaseDashComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
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

	// Проверяем, поддерживает ли владелец интерфейс IDashable
	if (GetOwner()->GetClass()->ImplementsInterface(UDashable::StaticClass()))
	{
		OwnerDashable = TScriptInterface<IDashable>(GetOwner());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Owner of DashComponent does not implement IDashable interface"));
	}
}


// Called every frame
void UBaseDashComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

void UBaseDashComponent::StartDash()
{
}

void UBaseDashComponent::EndDash()
{
}

