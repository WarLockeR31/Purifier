#include "VOFollowingComponent.h"
#include "UVOManager.h"
#include "NavigationSystem.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/CharacterMovementComponent.h"

UVOFollowingComponent::UVOFollowingComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	bWantsInitializeComponent = true;
}

void UVOFollowingComponent::OnRegister()
{
	Super::OnRegister(); // TODO: Check UCrowdManager::GetCurrent(GetWorld()); in CrowdFollowComp.cpp
	if (UWorld* W = GetWorld())
		if (auto* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(W))
			if (auto* CM = Cast<UVOManager>(Nav->GetCrowdManager()))
				CM->RegisterAgent(this);
}

void UVOFollowingComponent::OnUnregister()
{
	if (UWorld* W = GetWorld())
		if (auto* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(W))
			if (auto* CM = Cast<UVOManager>(Nav->GetCrowdManager()))
				CM->UnregisterAgent(this);
	Super::OnUnregister();
}

void UVOFollowingComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

APawn* GetControlledPawn_Local(const UVOFollowingComponent* Comp)
{
	if (const AController* C = Cast<AController>(Comp->GetOwner()))
		return C->GetPawn();
	return nullptr;
}

FVector UVOFollowingComponent::GetOwnerLocation() const
{
	const APawn* P = GetControlledPawn_Local(this);
	return P ? P->GetActorLocation() : FVector::ZeroVector;
}

FVector UVOFollowingComponent::GetOwnerVelocity() const
{
	const APawn* P = GetControlledPawn_Local(this);
	if (!P) return FVector::ZeroVector;
	if (auto* Move = P->FindComponentByClass<UMovementComponent>())
		return Move->Velocity;
	return FVector::ZeroVector;
}
