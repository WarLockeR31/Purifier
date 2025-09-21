#include "VOManager.h"
#include "NavigationSystem.h"
#include "NavigationSystemTypes.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PawnMovementComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "Stats/Stats.h"
#include "VOUtility.h"

static TAutoConsoleVariable<int32> CVarVODebugShow(
    TEXT("vo.Show"), 0,
    TEXT("Show VO cones (0/1). Per-component bDebugDraw also must be true."),
    ECVF_Default);

static TAutoConsoleVariable<int32> CVarCVODebugShow(
	TEXT("cvo.Show"), 0,
	TEXT("Show VO cones (0/1). Per-component bDebugDraw also must be true."),
	ECVF_Default);

#define DEBUG_ON

void UVOManager::OnNavDataRegistered(ANavigationData&){ }
void UVOManager::OnNavDataUnregistered(ANavigationData&){ }
void UVOManager::CleanUp(float){ }

void UVOManager::RegisterAgent(UVOFollowingComponent* Comp)
{
	Agents.AddUnique(Comp);
}

void UVOManager::UnregisterAgent(UVOFollowingComponent* Comp)
{
	Agents.Remove(Comp);
}

void UVOManager::Tick(float DeltaTime)
{
	for (int32 i = Agents.Num() - 1; i >= 0; --i)
	{
		auto* Comp = Agents[i].Get();
		if (!Comp) { Agents.RemoveAtSwap(i); continue; }

		APawn* P = nullptr;
		if (const AController* C = Cast<AController>(Comp->GetOwner()))
			P = C->GetPawn();
		if (!P) continue;

		const FVector Pos = Comp->GetOwnerLocation();

		FVector DesiredVel = FVector::ZeroVector;
		if (Comp->HasVOGoal())
		{
			const FVector To = (Comp->GetMoveGoal() - Pos);
			const FVector2D To2D(To.X, To.Y);
			const float Dist = To2D.Size();
			if (Dist > 1.f)
				DesiredVel = FVector(To2D / Dist * Comp->GetEffectiveParams().MaxSpeed, 0.f);
		}

		const FVector CurVel = Comp->GetOwnerVelocity();

		TArray<FVONeighborView> Neis;
		const float Range = Comp->GetEffectiveParams().NeighborRange;
		const float R2 = Range * Range;
		for (const TWeakObjectPtr<UVOFollowingComponent>& It : Agents)
		{
			UVOFollowingComponent* Other = It.Get();
			if (!Other || Other == Comp) continue;
			const FVector OP = Other->GetOwnerLocation();
			if (FVector::DistSquared2D(Pos, OP) > R2) continue;
			FVONeighborView V; V.Pos = OP; V.Vel = Other->GetOwnerVelocity(); V.Radius = Other->GetAgentRadius();
			Neis.Add(V);
		}

		PrepareArrays(Neis.Num());;

		const FVector OutVel = ComputeVelocity(Comp, CurVel, DesiredVel, Neis, Comp->GetEffectiveParams());

		if (auto* Move = P->FindComponentByClass<UPawnMovementComponent>())
		{
			Move->RequestDirectMove(OutVel, false);
		}
	}
}

FVector UVOManager::ComputeVelocity(const UVOFollowingComponent* Comp, const FVector& CurVel, const FVector& DesiredVel, const TArray<FVONeighborView>& Neis, const FVOParams& Params)
{
	SCOPE_CYCLE_COUNTER(STAT_VOComputeVelocity);
	
	const FVector ActorPos = Comp->GetOwnerLocation();
	
	const bool bDesiredForbidden = VOUtility::IsVelocityForbidden(FVector2D(DesiredVel.X, DesiredVel.Y), Neis, ActorPos, Params);
	if (!bDesiredForbidden)
		return DesiredVel.GetClampedToMaxSize2D(Params.MaxSpeed);
	
	VOUtility::BuildVOCones(Neis, ActorPos, Params, VO_Cones);
	VOUtility::CollectIntersections(VO_Cones, IntersectionsByRays);
	VOUtility::SortIntersectionsByRays(IntersectionsByRays);
	VOUtility::ClassifySegments(VO_Cones, Params, IntersectionsByRays, OutsideSegmentsByRays);

	const FVector2D best2D = VOUtility::SelectBestVelocityFromOutsideSegments(
		FVector2D(DesiredVel.X, DesiredVel.Y),
		FVector2D(CurVel.X, CurVel.Y),
		Neis,
		ActorPos,
		Params,
		OutsideSegmentsByRays,
		Debug_LastCandidates,
		Debug_BestCandidateIdx
	);
	FVector OutVel = FVector(best2D.X, best2D.Y, 0.f);

#ifdef DEBUG_ON
	UWorld* W = Comp->GetWorld();
	if (Comp->bDebugDraw)
	{
		FlushPersistentDebugLines(Comp->GetWorld());
		if (CVarCVODebugShow.GetValueOnAnyThread() != 0)
			VOUtility::DrawCombinedVO(Comp, OutsideSegmentsByRays);
		if (CVarVODebugShow.GetValueOnAnyThread() != 0)
			VOUtility::DrawVOCones(Comp, VO_Cones);
		for (auto N : Neis)
		{
			DrawDebugLine(W, N.Pos, N.Pos + N.Vel, Comp->DebugDrawColor, true, 15.f, 0, 0.3f);
		}
		DrawDebugCircle(W, Comp->GetOwnerLocation(), Params.MaxSpeed, 20, Comp->DebugDrawColor, true, 15.f, 0, 0.6f, FVector(0.f, 1.f, 0.f), FVector(1.f, 0.f, 0.f));
			
		VOUtility::DrawVelocityCandidates(Comp, Debug_LastCandidates, Debug_BestCandidateIdx, 10.f, 15.f);
	}
#endif
	return OutVel;
}

void UVOManager::PrepareArrays(size_t NumNeis)
{
	// TODO: Think about shrinking
	
	VO_Cones.Reset();
	VO_Cones.Reserve(NumNeis);

	size_t NumRays = 3 * NumNeis;
	IntersectionsByRays.SetNum(NumRays, EAllowShrinking::No);
	for (int32 i = 0; i < IntersectionsByRays.Num(); ++i)
	{
		IntersectionsByRays[i].Reset();
		IntersectionsByRays[i].Reserve(NumRays - 1); // 3 * (N - 1) + 2
	}

	OutsideSegmentsByRays.SetNum(NumRays, EAllowShrinking::No);
	for (int32 i = 0; i < OutsideSegmentsByRays.Num(); ++i)
	{
		OutsideSegmentsByRays[i].Reset();
		OutsideSegmentsByRays[i].Reserve(FMath::Max(0, IntersectionsByRays[i].Num()));
	}
}
