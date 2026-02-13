#include "Core/VOManager.h"

#include "Components/VOFollowingComponent.h"
#include "Utils/AOUtility.h"
#include "NavigationSystem.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PawnMovementComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "NavMesh/RecastHelpers.h"
#include "Runtime/Navmesh/Public/DetourCrowd/DetourCrowd.h"
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

LLM_DEFINE_TAG(VOAO);

void UVOManager::RegisterAgent(UVOFollowingComponent* Comp)
{
	// TODO: Check
	// Super::RegisterAgent(Comp); // Base is called by Component's OnRegister via UCrowdFollowingComponent

	if (Comp->VOManagerIndex != INDEX_NONE)
	{
		return;
	}

	Agents.Add(Comp);
	// TODO: Friend?
	Comp->VOManagerIndex = Agents.Num() - 1;
}

void UVOManager::UnregisterAgent(UVOFollowingComponent* Comp)
{
	int32 Idx = Comp->VOManagerIndex;
	if (Idx == INDEX_NONE)
	{
		return;
	}

	UVOFollowingComponent* LastComp = Agents.Last().Get();

	Agents.RemoveAtSwap(Idx, EAllowShrinking::No);

	if (LastComp && LastComp != Comp)
	{
		LastComp->VOManagerIndex = Idx;
	}

	Comp->VOManagerIndex = INDEX_NONE;
	Comp->DetourAgentIndex = INDEX_NONE;

	// TODO: Check
	// Super::UnregisterAgent(Comp); // Base is called by Component's OnUnregister
}

void UVOManager::Tick(float DeltaTime)
{
	LLM_SCOPE_BYTAG(VOAO);
	TRACE_CPUPROFILER_EVENT_SCOPE(UVOManager::Tick);

	for (const TWeakObjectPtr<UVOFollowingComponent>& It : Agents)
	{
		UVOFollowingComponent* Comp = It.Get();
		if (Comp) Comp->UpdateKinematics(DeltaTime);
	}

	if (DetourCrowd)
	{
		int32 NumActive = DetourCrowd->cacheActiveAgents();
		if (NumActive)
		{
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(UVOManager::Tick_Detour);
				MyNavData->BeginBatchQuery();

				for (auto It = ActiveAgents.CreateIterator(); It; ++It)
				{
					// collect position and velocity
					FCrowdAgentData& AgentData = It.Value();
					if (AgentData.IsValid())
					{
						// Sync indices
						if (UVOFollowingComponent* VOComp = Cast<UVOFollowingComponent>(It.Key()))
						{
							VOComp->DetourAgentIndex = AgentData.AgentIndex;
						}
					
						PrepareAgentStep(It.Key(), AgentData, DeltaTime);
					}
				}

				// corridor update from previous step
				{
					//SCOPE_CYCLE_COUNTER(STAT_AI_Crowd_StepCorridorTime);
					DetourCrowd->updateStepCorridor(DeltaTime, DetourAgentDebug);
				}

				// regular steps
				if (bAllowPathReplan)
				{
					//SCOPE_CYCLE_COUNTER(STAT_AI_Crowd_StepPathsTime);
					DetourCrowd->updateStepPaths(DeltaTime, DetourAgentDebug);
				}
				{
					//SCOPE_CYCLE_COUNTER(STAT_AI_Crowd_StepProximityTime);
					DetourCrowd->updateStepProximityData(DeltaTime, DetourAgentDebug);
					PostProximityUpdate();
				}
				{
					//SCOPE_CYCLE_COUNTER(STAT_AI_Crowd_StepNextPointTime);
					DetourCrowd->updateStepNextMovePoint(DeltaTime, DetourAgentDebug);
					PostMovePointUpdate();
				}
				{
					//SCOPE_CYCLE_COUNTER(STAT_AI_Crowd_StepSteeringTime);
					DetourCrowd->updateStepSteering(DeltaTime, DetourAgentDebug);
				}
			}
			{
				//SCOPE_CYCLE_COUNTER(STAT_AI_Crowd_StepAvoidanceTime);
				// TODO:
				//DetourCrowd->updateStepAvoidance(DeltaTime, DetourAgentDebug);
				UpdateAvoidance();
			}
			/*if (bResolveCollisions) // TODO: ?
			{
				//SCOPE_CYCLE_COUNTER(STAT_AI_Crowd_StepCollisionsTime);
				DetourCrowd->updateStepMove(DeltaTime, DetourAgentDebug);
			}*/
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(UVOManager::Tick_AfterAO);
				{
					//SCOPE_CYCLE_COUNTER(STAT_AI_Crowd_StepComponentsTime);
					UpdateAgentPaths();
				}
				{
					//SCOPE_CYCLE_COUNTER(STAT_AI_Crowd_StepNavLinkTime);
					DetourCrowd->updateStepOffMeshVelocity(DeltaTime, DetourAgentDebug);
				}

				MyNavData->FinishBatchQuery();
			}

			// velocity updates
			{
				//SCOPE_CYCLE_COUNTER(STAT_AI_Crowd_StepMovementTime);
				/*for (auto It = ActiveAgents.CreateIterator(); It; ++It)
				{
					const FCrowdAgentData& AgentData = It.Value();
					if (AgentData.bIsSimulated && AgentData.IsValid())
					{
						UCrowdFollowingComponent* CrowdComponent = Cast<UCrowdFollowingComponent>(It.Key());
						if (CrowdComponent && CrowdComponent->IsCrowdSimulationEnabled())
						{
							ApplyVelocity(CrowdComponent, AgentData.AgentIndex);
						}
					}
				}*/
			}

#if WITH_EDITOR
			// normalize samples only for debug drawing purposes
			// DetourAvoidanceDebug->normalizeSamples(); // TODO: ?
#endif
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

void UVOManager::UpdateAvoidance()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UVOManager::UpdateAvoidance);
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
		NeighborVerticesBuffer.Reset();
		TArray<FVONeighborView> Neis;
		int MaxNeisCount = 5;
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(UVOManager::GatherNeighbors);
			GatherNeighbors(Comp, Params, Neis);
		}
		
		// Prepare Buffers
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(UVOManager::PrepareArrays);
			PrepareArrays(Neis.Num());
		}

		// VELOCITY OBSTACLE
		if (Comp->GetAvoidanceStyle() == EAvoidanceStyle::VelocityObstacle)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(UVOManager::VO);

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
			Ctx.NeighborVertices = &NeighborVerticesBuffer;

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
			TRACE_CPUPROFILER_EVENT_SCOPE(UVOManager::AO);
			FVector TargetPos = FVector::ZeroVector;
			const dtCrowdAgent* dtAgent = DetourCrowd->getAgent(Comp->DetourAgentIndex);
			if (dtAgent->targetState == DT_CROWDAGENT_TARGET_NONE)
				continue;
			
			TargetPos = Recast2UnrealPoint(&dtAgent->cornerVerts[0]);
			
			FAOCalculationContext Ctx;
    
			Ctx.Comp = Comp;
			Ctx.ActorPos = Pos;
			Ctx.CurrentVelocity = CurVel;
			Ctx.TargetPos = TargetPos; 
			Ctx.Neis = &Neis;
			Ctx.Params = &Params;
			Ctx.NeighborVertices = &NeighborVerticesBuffer;

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
}

void UVOManager::GatherNeighbors(
    const UVOFollowingComponent* Comp,
    const FVOParams& Params,
    TArray<FVONeighborView>& Neis)
{
    Neis.Reset(); 
    //Neis.Reserve(5);

	struct FCandidate {
		bool bIsAgent;
		UVOFollowingComponent* AgentComp;
		FVector2D SegP1;
		FVector2D SegP2; 
		float t;
    
		FCandidate(UVOFollowingComponent* InComp, float InT) 
		   : bIsAgent(true), AgentComp(InComp), SegP1(FVector2D::ZeroVector), SegP2(FVector2D::ZeroVector), t(InT) 
		{}

		FCandidate(const FVector2D& P1, const FVector2D& P2, float InT) 
		   : bIsAgent(false), AgentComp(nullptr), SegP1(P1), SegP2(P2), t(InT) 
		{}
	};

	TArray<FCandidate, TInlineAllocator<5>> Candidates;
    
    int32 MaxTIdx = -1;

	auto TryAddCandidate = [&](const FCandidate& NewCand)
	{
		if (Candidates.Num() < 5)
		{
			Candidates.Add(NewCand);
			
			if (MaxTIdx == -1 || NewCand.t > Candidates[MaxTIdx].t)
			{
				MaxTIdx = Candidates.Num() - 1;
			}
		}
		else if (NewCand.t < Candidates[MaxTIdx].t)
		{
			Candidates[MaxTIdx] = NewCand;

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
	};

	const FVector Pos = Comp->GetOwnerLocation();
	const float NeighborRangeSqr = FMath::Square(Params.NeighborRange);

	// Gather agents
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

    	TryAddCandidate(FCandidate(Other, t));
    }

	// Gather static segments
	if (DetourCrowd)
	{
		const dtCrowdAgent* dtAgent = DetourCrowd->getAgent(Comp->DetourAgentIndex);
		for (int j = 0; j < dtAgent->boundary.getSegmentCount(); ++j)
		{
			const dtReal* s = dtAgent->boundary.getSegment(j);
			const dtReal* q = s + 3;
			UE_LOG(LogTemp, Log, TEXT("%f"), *dtAgent->npos);
			if (dtTriArea2D(dtAgent->npos, s, q) < 0.0f)
				continue;
			
			const FVector2D P1 = FVector2D(Recast2UnrealPoint(s));
			const FVector2D P2 = FVector2D(Recast2UnrealPoint(q));
			const FVector2D Pos2D = FVector2D(Pos); 

			FVector2D ClosestPt = FMath::ClosestPointOnSegment2D(Pos2D, P1, P2);
			if (FVector2D::DistSquared(Pos2D, ClosestPt) > NeighborRangeSqr)
				continue;

			float t = -1.f;
			switch (Params.AvoidanceStyle)
			{
			case EAvoidanceStyle::VelocityObstacle:
				t = CalculateCCT_VO(Comp, Params, P1, P2);
				break;
			case EAvoidanceStyle::AccelerationObstacle:
				t = CalculateCCT_AO(Comp, Params, P1, P2);
				break;
			default:
				continue; 
			} 

			Candidates.Add(FCandidate(P1, P2, t));
			//TryAddCandidate(FCandidate(P1, P2, t));
		}

		UE_LOG(LogTemp, Log, TEXT("%d"), Candidates.Num() - 5);
	}
	

	Neis.Reserve(Candidates.Num());

	for (const FCandidate& Cand : Candidates)
	{
		if (Cand.bIsAgent)
		{
			FVector NPos = Cand.AgentComp->GetOwnerLocation();
			FVector NVel = Cand.AgentComp->GetCachedVelocity();
			FVector NAcc = Cand.AgentComp->GetCachedAcceleration();
			float NRad = Cand.AgentComp->GetAgentRadius();

			Neis.Emplace(NPos, NVel, NAcc, NRad, Cand.t, EMinkowskiShapeType::Circle);
			Neis.Last().NeighborType = ENeighborType::Dynamic;
		}
		else
		{
			FVector P1 = FVector(Cand.SegP1.X, Cand.SegP1.Y, 0);
			FVector P2 = FVector(Cand.SegP2.X, Cand.SegP2.Y, 0);
			
			// TODO: Only 2D
			Neis.Add(FVONeighborView::CreateSeg(P1, P2, Cand.t, NeighborVerticesBuffer));
			Neis.Last().NeighborType = ENeighborType::Static;
			//Neis.Emplace(MidPoint3D, FVector::ZeroVector, FVector::ZeroVector, 0.0f /*Radius*/, Cand.t);
		}
	}
}

#pragma region CCT
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
	FVector2D Pos = FVector2D(Comp->GetOwnerLocation());
	FVector2D ClosestPt = FMath::ClosestPointOnSegment2D(Pos, P, Q);

	FVector2D ToObstacle = ClosestPt - Pos;
	float DistSq = ToObstacle.SizeSquared();
	if (DistSq <= FMath::Square(AgentParams.AgentRadius))
	{
		return 0.0f;
	}

	float Dist = FMath::Sqrt(DistSq);
	float Gap = Dist - AgentParams.AgentRadius;

	if (AgentParams.MaxSpeed <= KINDA_SMALL_NUMBER)
	{
		return MAX_flt;
	}

	return Gap / AgentParams.MaxSpeed;
}

float UVOManager::CalculateCCT_AO(
	const UVOFollowingComponent* Comp,
	const FVOParams& AgentParams,
	const FVector2D& P,
	const FVector2D& Q)
{
	FVector2D Pos = FVector2D(Comp->GetOwnerLocation());
	FVector2D ClosestPt = FMath::ClosestPointOnSegment2D(Pos, P, Q);

	FVector2D ToObstacle = ClosestPt - Pos;
	float DistSq = ToObstacle.SizeSquared();
	if (DistSq <= FMath::Square(AgentParams.AgentRadius))
	{
		return 0.0f;
	}

	float Dist = FMath::Sqrt(DistSq);
	float Gap = Dist - AgentParams.AgentRadius;
	FVector2D ToObstacleNorm = ToObstacle / Dist;

	float AgentVel = FVector2D(Comp->GetCachedVelocity()) | ToObstacleNorm;
	float Accel = AgentParams.MaxAcceleration;

	float SaturationT = 0.f;
	if (AgentVel < AgentParams.MaxSpeed)
	{
		SaturationT = (AgentParams.MaxSpeed - AgentVel) / Accel;
	}

	// [0 -> SaturationT]
	{
		float DT = SaturationT;
		float DistCovered = (AgentVel * DT) + (0.5f * Accel * DT * DT);

		if (DistCovered >= Gap)
		{
			float Discriminant = (AgentVel * AgentVel) + (2.0f * Accel * Gap);
			if (Discriminant < 0.f)
				return MAX_flt;

			return (-AgentVel + FMath::Sqrt(Discriminant)) / Accel;
		}

		Gap -= DistCovered;
	}

	// [SaturationT -> Infinity]
	if (AgentParams.MaxSpeed <= KINDA_SMALL_NUMBER)
	{
		return MAX_flt;
	}

	return SaturationT + (Gap / AgentParams.MaxSpeed);
}
#pragma endregion

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