#include "VOWorldSubsystem.h"
#include "VOFollowingComponent.h"

void UVOWorldSubsystem::Register(UVOFollowingComponent* Comp)
{
	Agents.Add(Comp);
}

void UVOWorldSubsystem::Unregister(UVOFollowingComponent* Comp)
{
	Agents.Remove(Comp);
}

void UVOWorldSubsystem::QueryNeighbors(const UVOFollowingComponent* Querier, const FVector& P, float Range, TArray<FVONeighborView>& Out) const
{
	const float R2 = Range * Range;
	
	for (const TWeakObjectPtr<UVOFollowingComponent>& It : Agents)
	{
		UVOFollowingComponent* Other = It.Get();
		if (!Other || Other == Querier) continue;
		const FVector OP = Other->GetOwnerLocation();
		if (FVector::DistSquared2D(P, OP) > R2) continue;
		FVONeighborView V;
		V.Pos = OP;
		V.Vel = Other->GetOwnerVelocity();
		V.Radius = Other->GetAgentRadius();
		Out.Add(V);
	}
}
