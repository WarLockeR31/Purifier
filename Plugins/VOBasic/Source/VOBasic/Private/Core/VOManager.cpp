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
#include "Utils/AvoidanceMath.h"
#include "Utils/VOUtility.h"

static TAutoConsoleVariable<int32> CVarVODebugShow(
    TEXT("vo.Show"), 0,
    TEXT("Show VO cones (0/1). Per-component bDebugDraw also must be true."),
    ECVF_Default);

static TAutoConsoleVariable<int32> CVarCVODebugShow(
	TEXT("vo.cvo.Show"), 0,
	TEXT("Show VO cones (0/1). Per-component bDebugDraw also must be true."),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarAODebugShow(
	TEXT("ao.Show"), 0,
	TEXT("Show Acceleration Obstacle geometry (Tris/Quads)"), ECVF_Default);

static TAutoConsoleVariable<int32> CVarAODebugShowOutside(
	TEXT("ao.cao.Show"), 0,
	TEXT("Show valid AO boundary segments"), ECVF_Default);

static TAutoConsoleVariable<int32> CVarAODebugShowConstraints(
	TEXT("ao.ShowConstraints"), 0,
	TEXT("Show AO acceleration constraints"), ECVF_Default);

static TAutoConsoleVariable<int32> CVarVODebugShowConstraints(
	TEXT("vo.ShowConstraints"), 0,
	TEXT("Show VO velocity constraints"), ECVF_Default);

#ifdef SAVE_VO_PATHS
static TAutoConsoleVariable<int32> CVarVODebugShowPaths(
	TEXT("vo.ShowPaths"), 0,
	TEXT("Save and show paths"), ECVF_Default);
#endif

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
		int MaxNeisCount = 5;
		GatherNeighbors(Comp, Params, Neis);

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
			if (!Comp->HasVOGoal())
				continue;
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
				Move->AddInputVector(BestAccel / Params.MaxAcceleration);
			}

			// DEBUG DRAW AO
#ifdef DEBUG_ON
			FlushPersistentDebugLines(Comp->GetWorld());
			if (Comp->bDebugDraw)
			{
               
            	
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
					AOUtility::DrawAccelConstraints(Comp, CurVel, Params.MaxSpeed, Params.MaxAcceleration, Params.TauAcceleration);
				}

				if (CVarAODebugShow.GetValueOnAnyThread() != 0 || CVarAODebugShowOutside.GetValueOnAnyThread() != 0 || CVarAODebugShowConstraints.GetValueOnAnyThread() != 0)
				{
					VOUtility::DrawVelocityCandidates(Comp, AO_Candidates, AO_BestCandidateIdx, 16.f, 0.f);
                    
					DrawDebugLine(Comp->GetWorld(), Pos, Pos + CurVel, FColor::Blue, true, -1.f, 0, 2.f); // Vel
					DrawDebugLine(Comp->GetWorld(), Pos, Pos + BestAccel, FColor::Green, true, -1.f, 0, 3.f); // Accel
				}
			}


        	
#endif
			continue; // AO Done
		}
	}

#ifdef SAVE_VO_PATHS
	for (int32 i = Agents.Num() - 1; i >= 0; --i)
	{
		UVOFollowingComponent* Comp = Agents[i].Get();
		if (Comp) Comp->UpdateKinematics(DeltaTime);
	
		// vo.ShowPaths
		Comp->UpdatePathHistory(DeltaTime);
		if (CVarVODebugShowPaths.GetValueOnAnyThread() != 0)
		{
			DrawAgentPath(Comp, Comp->PathHistory, AvoidanceMath::GetColorFromSeed(i));
		}
	}
#endif
}

void UVOManager::GatherNeighbors(
    const UVOFollowingComponent* Comp,
    const FVOParams& Params,
    TArray<FVONeighborView>& Neis)
{
    Neis.Reset(); 
    //Neis.Reserve(5);

	struct FCandidate {
		UVOFollowingComponent* Component;
		float t;
    
		FCandidate(UVOFollowingComponent* InComp, float InT) 
			: Component(InComp), t(InT) 
		{}
	};

	TArray<FCandidate, TInlineAllocator<5>> Candidates;

    const FVector Pos = Comp->GetOwnerLocation();
    const float NeighborRangeSqr = FMath::Square(Params.NeighborRange);
    
    int32 MaxTIdx = -1;

    for (const TWeakObjectPtr<UVOFollowingComponent>& It : Agents)
    {
       	UVOFollowingComponent* Other = It.Get();
       	
       	if (!Other || Other == Comp)
       		continue;
       	if (FVector::DistSquared2D(Pos, Other->GetOwnerLocation()) > NeighborRangeSqr)
       		continue;
	
       	// TODO: Add FOV for Agents
	
       	float t = -1.f;
       	switch (Params.AvoidanceStyle)
       	{
       	    case EAvoidanceStyle::VelocityObstacle:
       	       t = CalculateCCT_VO(Comp, Params, Other);
       	       break;
       	    case EAvoidanceStyle::AccelerationObstacle:
       	       t = CalculateCCT_AO(Comp, Params, Other);
       	       break;
       	    default:
       	       continue; 
       	}

    	if (Candidates.Num() < 5)
    	{
    		Candidates.Emplace(Other, t);
           
    		if (MaxTIdx == -1 || t > Candidates[MaxTIdx].t)
    		{
    			MaxTIdx = Candidates.Num() - 1;
    		}
    	}
    	else if (t < Candidates[MaxTIdx].t)
    	{
    		Candidates[MaxTIdx] = {Other, t};

    		// New max
    		float NewMax = -1.f;
    		for (int32 i = 0; i < 5; ++i)
    		{
    			if (Candidates[i].t > NewMax)
    			{
    				NewMax = Candidates[i].t;
    				MaxTIdx = i;
    			}
    		}
    	}
    }

	Neis.Reserve(Candidates.Num());

	for (const FCandidate& Cand : Candidates)
	{
		FVector NPos = Cand.Component->GetOwnerLocation();
		FVector NVel = Cand.Component->GetCachedVelocity();
		FVector NAcc = Cand.Component->GetCachedAcceleration();
		float NRad = Cand.Component->GetAgentRadius();

		Neis.Emplace(NPos, NVel, NAcc, NRad, Cand.t);
	}
}

// CCT for agents
float UVOManager::CalculateCCT_VO(
	const UVOFollowingComponent* Agent,
	const FVOParams& AgentParams,
	const UVOFollowingComponent* Obstacle)
{
	float RoughGap = FVector::Dist2D(Agent->GetOwnerLocation(), Obstacle->GetOwnerLocation()) - (AgentParams.AgentRadius + Obstacle->GetAgentRadius());
	float MaxApproachSpeed = AgentParams.MaxSpeed + (FVector2D(Obstacle->GetCachedVelocity()) | FVector2D(Agent->GetOwnerLocation() - Obstacle->GetOwnerLocation()));
	// TODO: Negative case & colliding case
	return RoughGap / MaxApproachSpeed;
}

float UVOManager::CalculateCCT_AO(
	const UVOFollowingComponent* Agent,
	const FVOParams& AgentParams,
	const UVOFollowingComponent* Obstacle)
{
	const FVOParams& ObstacleParams = Obstacle->GetEffectiveParams();
	
	FVector2D ToObstacle = FVector2D(Obstacle->GetOwnerLocation() - Agent->GetOwnerLocation());
	const float CombinedRadius = AgentParams.AgentRadius + ObstacleParams.AgentRadius;
	float DistSq = ToObstacle.SizeSquared();
	if (DistSq <= FMath::Square(CombinedRadius))
	{
		return 0.0f;
	}
	float Dist = FMath::Sqrt(DistSq);
	float RoughGap = Dist - CombinedRadius;
	FVector2D ToObstacleNorm = ToObstacle.GetSafeNormal();
	FVector2D ToAgentNorm = -ToObstacleNorm;
	
	float AgentVel = FVector2D(Agent->GetCachedVelocity()) | ToObstacleNorm;
	
	float ObstacleVel = FVector2D(Obstacle->GetCachedVelocity()) | ToAgentNorm;
	float ObstacleAcc = FVector2D(Obstacle->GetCachedAcceleration()) | ToAgentNorm;

	float AgentSaturationT = 0.f;
	if (AgentVel < AgentParams.MaxSpeed)
		AgentSaturationT = (AgentParams.MaxSpeed - AgentVel) / AgentParams.MaxAcceleration;
	float ObstacleSaturationT = 0.f;
	if (ObstacleVel < ObstacleParams.MaxSpeed)
		ObstacleSaturationT = (ObstacleParams.MaxSpeed - ObstacleVel) / ObstacleAcc;

	float T1, T2;
	float Accel1, Accel2; 
	const float TotalMaxAccel = AgentParams.MaxAcceleration + ObstacleParams.MaxAcceleration;
	if (AgentSaturationT < ObstacleSaturationT)
	{
		T1 = AgentSaturationT;
		T2 = ObstacleSaturationT;
		Accel1 = TotalMaxAccel;                
		Accel2 = ObstacleParams.MaxAcceleration;
	}
	else
	{
		T1 = ObstacleSaturationT;
		T2 = AgentSaturationT;
		Accel1 = TotalMaxAccel;
		Accel2 = AgentParams.MaxAcceleration;
	}
	
	float CurrentGap = RoughGap;
	float CurrentVel = AgentVel + ObstacleVel;
	float CurrentTime = 0.0f;

	// [0 -> T1]
	{
		float DT = T1;
		// s = v*t + 0.5*a*t^2
		float DistCovered = (CurrentVel * DT) + (0.5f * Accel1 * DT * DT);

		if (DistCovered >= CurrentGap)
		{
			// 0.5*a*t^2 + v*t - Gap = 0
			// t = (-v + sqrt(v^2 + 2*a*Gap)) / a
			float Discriminant = (CurrentVel * CurrentVel) + (2.0f * Accel1 * CurrentGap);
			if (Discriminant < 0.f)
				return MAX_flt;
            
			return (-CurrentVel + FMath::Sqrt(Discriminant)) / Accel1;
		}

		CurrentGap -= DistCovered;
		CurrentVel += Accel1 * DT;
		CurrentTime += DT;
	}

	// [T1 -> T2]
	{
		float DT = T2 - T1;
		if (DT > KINDA_SMALL_NUMBER)
		{
			float DistCovered = (CurrentVel * DT) + (0.5f * Accel2 * DT * DT);

			if (DistCovered >= CurrentGap)
			{
				float Discriminant = (CurrentVel * CurrentVel) + (2.0f * Accel2 * CurrentGap);
				if (Discriminant < 0.f)
					return MAX_flt;

				return CurrentTime + ((-CurrentVel + FMath::Sqrt(Discriminant)) / Accel2);
			}

			CurrentGap -= DistCovered;
			CurrentVel += Accel2 * DT;
			CurrentTime += DT;
		}
	}

	// [T2 -> Infinity]
	if (CurrentVel <= 0.f)
	{
		return MAX_flt;
	}

	return CurrentTime + (CurrentGap / CurrentVel);
}

// CCT for static obstacles
float UVOManager::CalculateCCT_VO(
	const UVOFollowingComponent* Comp,
	const FVOParams& AgentParams,
	const FVector2D& P,
	const FVector2D& Q)
{
	// TODO
	return 0;
}
float UVOManager::CalculateCCT_AO(
	const UVOFollowingComponent* Comp,
	const FVOParams& AgentParams,
	const FVector2D& P,
	const FVector2D& Q)
{
	// TODO
	return 0;
}


void UVOManager::PrepareArrays(size_t NumNeis)
{
	// TODO:
	
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

void UVOManager::DrawAgentPath(const UVOFollowingComponent* Comp, const TArray<FVector>& PathHistory, const FColor& PathColor)
{
	UWorld* W = Comp->GetWorld();
	if (!W || PathHistory.Num() < 2) return;
        
	for (int32 i = 0; i < PathHistory.Num() - 1; ++i)
	{
		DrawDebugLine(
			W, 
			PathHistory[i], 
			PathHistory[i + 1], 
			PathColor, 
			true,  // false = рисовать только на один кадр
			15.f, 
			0, 
			2.0f    // Толщина линии
		);
	}
}