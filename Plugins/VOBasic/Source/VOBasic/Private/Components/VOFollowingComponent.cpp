#include "Components/VOFollowingComponent.h"
#include "Core/VOManager.h"
#include "NavigationSystem.h"
#include "Settings/VOSettings.h"
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



void UVOFollowingComponent::SetVOProfile(FName InProfileName)
{
	//TODO: Check for validity
	VOProfile = InProfileName; 
	MarkEffectiveDirty();
}

void UVOFollowingComponent::SetVOOverrides(const FVOParams& InOverrides, bool bEnable)
{
	Overrides     = InOverrides;
	bUseOverrides = bEnable;
	MarkEffectiveDirty();
}



int32 UVOFollowingComponent::AddParamModifier(const FVOParamModifier& InMod)
{
	FVOParamModifier Copy = InMod;
	Copy.Id = NextModId++;
	ActiveMods.Add(Copy);
	MarkEffectiveDirty();
	return Copy.Id;
}

bool UVOFollowingComponent::RemoveParamModifierById(int32 Id)
{
	const int32 Removed = ActiveMods.RemoveAll([&](const FVOParamModifier& M){ return M.Id == Id; });
	if (Removed > 0) MarkEffectiveDirty();
	return Removed > 0;
}

int32 UVOFollowingComponent::RemoveParamModifiersByTag(FName Tag)
{
	const int32 Removed = ActiveMods.RemoveAll([&](const FVOParamModifier& M){ return M.Tag == Tag; });
	if (Removed > 0) MarkEffectiveDirty();
	return Removed;
}

void UVOFollowingComponent::SetAvoidanceStyle(EAvoidanceStyle NewStyle)
{
	EffectiveParams.AvoidanceStyle = NewStyle; // For instant application
	AvoidanceStyleOverride = NewStyle;
	bHasAvoidanceStyleOverride = true;
}

void UVOFollowingComponent::ResetAvoidanceStyle()
{
	bHasAvoidanceStyleOverride = false;
	MarkEffectiveDirty();
}


const FVOParams& UVOFollowingComponent::GetEffectiveParams() const
{
	// Return effective params if they are already computed
	if (!bEffectiveDirty)
		return EffectiveParams;

	const UVOSettings* Settings = UVOSettings::Get();

	// Base params (overrides or existing preset)
	FVOParams Base = bUseOverrides ? Overrides : Settings->GetPresetOrDefault(VOProfile); // TODO: Caching current profile

	EffectiveParams = Base;

	// Apply modifiers
	if (!ActiveMods.IsEmpty())
	{
		TArray<FVOParamModifier> Sorted = ActiveMods;
		Sorted.Sort([](const auto& A, const auto& B){ return A.Priority < B.Priority; });
		for (const FVOParamModifier& M : Sorted)
		{
			ApplyModifierTo(EffectiveParams, M);
		}
	}

	// Clamp values
	EffectiveParams.AgentRadius   = FMath::Max(0.f, EffectiveParams.AgentRadius);
	EffectiveParams.MaxSpeed      = FMath::Max(0.f, EffectiveParams.MaxSpeed);
	EffectiveParams.NeighborRange = FMath::Max(0.f, EffectiveParams.NeighborRange);
	EffectiveParams.TauHorizon    = FMath::Max(0.01f, EffectiveParams.TauHorizon);
	EffectiveParams.MaxAcceleration = FMath::Max(0.f, EffectiveParams.MaxAcceleration);

	// Apply avoidance style override
	if (bHasAvoidanceStyleOverride)
		EffectiveParams.AvoidanceStyle = AvoidanceStyleOverride;
	
	bEffectiveDirty = false;
	return EffectiveParams;
}

static void ApplyOp(float& Field, EVOOp Op, float Magnitude)
{
	switch (Op)
	{
		case EVOOp::Add: Field += Magnitude; break;
		case EVOOp::Mul: Field *= Magnitude; break;
		case EVOOp::Set: Field  = Magnitude; break;
		default: break;
	}
}

void UVOFollowingComponent::ApplyModifierTo(FVOParams& P, const FVOParamModifier& M) const
{
	switch (M.Key)
	{
		case EVOParamKey::TauHorizon:    	ApplyOp(P.TauHorizon,    M.Op, M.Magnitude); 	break;
		case EVOParamKey::MaxSpeed:      	ApplyOp(P.MaxSpeed,      M.Op, M.Magnitude); 	break;
		case EVOParamKey::NeighborRange: 	ApplyOp(P.NeighborRange, M.Op, M.Magnitude); 	break;
		case EVOParamKey::AgentRadius:   	ApplyOp(P.AgentRadius,   M.Op, M.Magnitude); 	break;

		case EVOParamKey::MaxAcceleration:	ApplyOp(P.MaxAcceleration, M.Op, M.Magnitude);	break;
	}
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
	if (bUseFakeVelocity)
	{
		return FakeVelocity;
	}
	
	const APawn* P = GetControlledPawn_Local(this);
	if (!P) return FVector::ZeroVector;
	if (const UMovementComponent* Move = P->FindComponentByClass<UMovementComponent>()) //TODO: Maybe unneeded
	{
		return Move->Velocity;
	}
	return FVector::ZeroVector;
}

void UVOFollowingComponent::UpdateKinematics(float DeltaTime)
{
	const APawn* P = GetControlledPawn_Local(this);
	if (!P) return;

	const UMovementComponent* Move = P->FindComponentByClass<UMovementComponent>();
	/*const*/ FVector NewVel = Move ? Move->Velocity : FVector::ZeroVector;
	
	if (bUseFakeVelocity)
	{
		NewVel = FakeVelocity;
	}
	
	if (bHasPrevVelocity && DeltaTime > KINDA_SMALL_NUMBER)
	{
		CachedAcceleration = (NewVel - CachedVelocity) / DeltaTime;
		CachedAcceleration.Z = 0.f;
	}

	CachedVelocity   = NewVel;
	bHasPrevVelocity = true;

	if (const UCharacterMovementComponent* CharMove = Cast<UCharacterMovementComponent>(Move)) //TODO: Move to optimize
	{
		FVector Acc2D = CharMove->GetCurrentAcceleration();
		Acc2D.Z = 0.f;
		CachedAcceleration = Acc2D;
	}
}

#ifdef SAVE_VO_PATHS
void UVOFollowingComponent::UpdatePathHistory(float DeltaTime)
{
	float CurrentTime = GetWorld()->GetTimeSeconds();
		
	if (CurrentTime - LastPathSaveTime > PathSaveInterval)
	{
		PathHistory.Add(GetOwnerLocation());
		LastPathSaveTime = CurrentTime;

		if (PathHistory.Num() > PathHistorySize)
		{
			PathHistory.RemoveAt(0);
		}
	}
}
#endif

// TODO: Delete
void UVOFollowingComponent::SetFakeVelocity(const FVector& InVelocity)
{
	FakeVelocity = InVelocity;
	bUseFakeVelocity = true;
}

void UVOFollowingComponent::ClearFakeVelocity()
{
	bUseFakeVelocity = false;
	FakeVelocity = FVector::ZeroVector;
}
