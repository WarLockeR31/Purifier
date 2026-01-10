#include "Core/VOManager.h"

#include "Utils/AOUtility.h"
#include "NavigationSystem.h"
#include "NavigationSystemTypes.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PawnMovementComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "Stats/Stats.h"
#include "Utils/VOUtility.h"

static TAutoConsoleVariable<int32> CVarVODebugShow(
    TEXT("vo.Show"), 0,
    TEXT("Show VO cones (0/1). Per-component bDebugDraw also must be true."),
    ECVF_Default);

static TAutoConsoleVariable<int32> CVarCVODebugShow(
	TEXT("cvo.Show"), 0,
	TEXT("Show VO cones (0/1). Per-component bDebugDraw also must be true."),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarAODebugShow(
	TEXT("ao.Show"), 0,
	TEXT("Show Acceleration Obstacle geometry (Tris/Quads)"), ECVF_Default);

static TAutoConsoleVariable<int32> CVarAODebugShowOutside(
	TEXT("cao.Show"), 0,
	TEXT("Show valid AO boundary segments"), ECVF_Default);

static TAutoConsoleVariable<int32> CVarAODebugShowConstraints(
	TEXT("ao.ShowConstraints"), 0,
	TEXT("Show AO acceleration constraints"), ECVF_Default);

static TAutoConsoleVariable<int32> CVarVODebugShowConstraints(
	TEXT("vo.ShowConstraints"), 0,
	TEXT("Show VO velocity constraints"), ECVF_Default);

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
    for (const TWeakObjectPtr<UVOFollowingComponent>& It : Agents)
    {
        UVOFollowingComponent* Comp = It.Get();
        if (Comp) Comp->UpdateKinematics(DeltaTime);
    }
    
    for (int32 i = Agents.Num() - 1; i >= 0; --i)
    {
        auto* Comp = Agents[i].Get();
        if (!Comp) { Agents.RemoveAtSwap(i); continue; }

        APawn* P = nullptr;
        if (const AController* C = Cast<AController>(Comp->GetOwner()))
            P = C->GetPawn();
        if (!P) continue;

        const FVector Pos = Comp->GetOwnerLocation();
        const FVector CurVel = Comp->GetCachedVelocity();
        const FVOParams& Params = Comp->GetEffectiveParams();

        // Collect Neighbors
        TArray<FVONeighborView> Neis;
        const float Range = Params.NeighborRange;
        const float R2 = Range * Range;
        for (const TWeakObjectPtr<UVOFollowingComponent>& It : Agents)
        {
            UVOFollowingComponent* Other = It.Get();
            if (!Other || Other == Comp) continue;
            if (FVector::DistSquared2D(Pos, Other->GetOwnerLocation()) > R2) continue;

            FVONeighborView V;
            V.Pos = Other->GetOwnerLocation();
            V.Vel = Other->GetCachedVelocity();
           
            V.Acc = (Other->GetAvoidanceStyle() == EAvoidanceStyle::AccelerationObstacle) ? Other->GetCachedAcceleration() : FVector::ZeroVector;
            V.Radius = Other->GetAgentRadius();
            Neis.Add(V);
        }

        // Prepare Buffers
        PrepareArrays(Neis.Num());

    	// VELOCITY OBSTACLE
    	if (Comp->GetAvoidanceStyle() == EAvoidanceStyle::VelocityObstacle)
    	{
    		SCOPE_CYCLE_COUNTER(STAT_VOComputeVelocity);

    		// Calculate Desired Velocity based on Goal
    		FVector DesiredVel = FVector::ZeroVector;
    		if (Comp->HasVOGoal())
    		{
    			const FVector To = (Comp->GetMoveGoal() - Pos);
    			const FVector2D To2D(To.X, To.Y);
    			const float Dist = To2D.Size();
    			if (Dist > 1.f)
    				DesiredVel = FVector(To2D / Dist * Params.MaxSpeed, 0.f);
    		}
    		
    		FVOCalculationContext Ctx;
			Ctx.Comp = Comp;
			Ctx.ActorPos = Pos;
			Ctx.CurrentVelocity = CurVel;
			Ctx.DesiredVelocity = DesiredVel;
			Ctx.Neis = &Neis;
			Ctx.Params = &Params;

			// Assign Buffers
			Ctx.Cones = &VO_Cones;
			Ctx.Intersections = &VO_Intersections;
			Ctx.OutsideSegments = &VO_OutsideSegments;
			
			// Debug buffers
			Ctx.OutCandidates = &VO_Candidates;
			Ctx.OutBestCandidateIdx = &VO_BestCandidateIdx;

    		const FVector OutVel = VOUtility::ComputeVelocity(Ctx);
    		
    		if (auto* Move = P->FindComponentByClass<UPawnMovementComponent>())
    		{
    			Move->RequestDirectMove(OutVel, false);
    		}
    		
#ifdef DEBUG_ON
			if (Comp->bDebugDraw)
			{
				FlushPersistentDebugLines(Comp->GetWorld());
				if (CVarCVODebugShow.GetValueOnAnyThread() != 0)
					VOUtility::DrawCombinedVO(Comp, VO_OutsideSegments);
				if (CVarVODebugShow.GetValueOnAnyThread() != 0)
					VOUtility::DrawVOCones(Comp, VO_Cones);
				for (auto N : Neis)
				{
					DrawDebugLine(Comp->GetWorld(), N.Pos, N.Pos + N.Vel, Comp->DebugDrawColor, true, 15.f, 0, 0.3f);
				}

				if (CVarVODebugShowConstraints.GetValueOnAnyThread() != 0)
				{
					VOUtility::DrawMaxSpeedCircle(Comp, Params.MaxSpeed);
				}
					
				VOUtility::DrawVelocityCandidates(Comp, VO_Candidates, VO_BestCandidateIdx, 10.f, 15.f);
			}
#endif
    		continue;
    	}

        // ACCELERATION OBSTACLE
        if (Comp->GetAvoidanceStyle() == EAvoidanceStyle::AccelerationObstacle)
        {
        	FVector TargetPos = Comp->GetMoveGoal();

        	FAOCalculationContext Ctx;
    
        	Ctx.Comp = Comp;
        	Ctx.ActorPos = Pos;
        	Ctx.CurrentVelocity = CurVel;
        	Ctx.TargetPos = TargetPos; 
        	Ctx.Neis = &Neis;
        	Ctx.Params = &Params;

        	Ctx.Cones = &AO_Cones;
        	Ctx.WorkSegments = &AO_WorkSegments;
        	Ctx.SideIntersections = &AO_SideIntersections;
        	Ctx.OutsideSegments = &AO_OutsideSegments;
    
        	Ctx.OutCandidates = &AO_Candidates;
        	Ctx.OutBestCandidateIdx = &AO_BestCandidateIdx;

        	FVector BestAccel = AOUtility::ComputeAcceleration(Ctx);

            // Apply Acceleration to Movement Component
            if (auto* Move = P->FindComponentByClass<UPawnMovementComponent>())
            {
                Move->AddInputVector(BestAccel);
            }

            // DEBUG DRAW AO
#ifdef DEBUG_ON
            if (Comp->bDebugDraw)
            {
                FlushPersistentDebugLines(Comp->GetWorld());
            	
                // ao.Show
                if (CVarAODebugShow.GetValueOnAnyThread() != 0)
                {
                    AOUtility::DrawAOCones(Comp, AO_Cones);
                }

                // cao.Show
                if (CVarAODebugShowOutside.GetValueOnAnyThread() != 0)
                {
                    AOUtility::DrawAOOutsideSegments(Comp, AO_OutsideSegments);
                }

                // ao.ShowConstraints
                if (CVarAODebugShowConstraints.GetValueOnAnyThread() != 0)
                {
                    AOUtility::DrawAccelConstraints(Comp, CurVel, Params.MaxSpeed, Params.MaxAcceleration, 1.5f);
                }

                if (CVarAODebugShow.GetValueOnAnyThread() != 0 || CVarAODebugShowOutside.GetValueOnAnyThread() != 0 || CVarAODebugShowConstraints.GetValueOnAnyThread() != 0)
                {
                    VOUtility::DrawVelocityCandidates(Comp, AO_Candidates, AO_BestCandidateIdx, 8.f, 0.f);
                    
                    DrawDebugLine(Comp->GetWorld(), Pos, Pos + CurVel, FColor::Blue, true, -1.f, 0, 2.f); // Vel
                    DrawDebugLine(Comp->GetWorld(), Pos, Pos + BestAccel, FColor::Green, true, -1.f, 0, 3.f); // Accel
                }
            }
#endif
            continue; // AO Done
        }
    }
}

void UVOManager::PrepareArrays(size_t NumNeis)
{
	// VO Buffers
	VO_Cones.Reset();
	VO_Cones.Reserve(NumNeis);

	size_t NumRays = 3 * NumNeis;
	VO_Intersections.SetNum(NumRays, EAllowShrinking::No);
	for (int32 i = 0; i < VO_Intersections.Num(); ++i)
	{
		VO_Intersections[i].Reset();
		VO_Intersections[i].Reserve(NumRays - 1); // 3 * (N - 1) + 2
	}

	VO_OutsideSegments.SetNum(NumRays, EAllowShrinking::No);
	for (int32 i = 0; i < VO_OutsideSegments.Num(); ++i)
	{
		VO_OutsideSegments[i].Reset();
		VO_OutsideSegments[i].Reserve(FMath::Max(0, VO_Intersections[i].Num()));
	}

	// AO Buffers
	if (AO_WorkSegments.Max() < (int32)NumRays * 5) 
	{
		AO_WorkSegments.Reserve(NumRays * 5);
	}
    
	if (AO_SideIntersections.Num() < (int32)NumRays)
	{
		AO_SideIntersections.SetNum(NumRays);
		AO_OutsideSegments.SetNum(NumRays);
	}
}