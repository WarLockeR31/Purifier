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
#include "NavMesh/RecastNavMesh.h"
#include "NavMesh/RecastQueryFilter.h"
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

namespace FExternalCrowdDebug
{
#define DEFINE_EXTERNAL_CVAR_ACCESSOR(FuncName, CVarString) \
	int32 FuncName() { \
		static const IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT(CVarString)); \
		return CVar ? CVar->GetInt() : 0; \
	}

	DEFINE_EXTERNAL_CVAR_ACCESSOR(DebugSelectedActors, "ai.crowd.DebugSelectedActors");
	DEFINE_EXTERNAL_CVAR_ACCESSOR(DebugVisLog, "ai.crowd.DebugVisLog");
	DEFINE_EXTERNAL_CVAR_ACCESSOR(DrawDebugCorners, "ai.crowd.DrawDebugCorners");
	DEFINE_EXTERNAL_CVAR_ACCESSOR(DrawDebugCollisionSegments, "ai.crowd.DrawDebugCollisionSegments");
	DEFINE_EXTERNAL_CVAR_ACCESSOR(DrawDebugPath, "ai.crowd.DrawDebugPath");
	DEFINE_EXTERNAL_CVAR_ACCESSOR(DrawDebugVelocityObstacles, "ai.crowd.DrawDebugVelocityObstacles");
	DEFINE_EXTERNAL_CVAR_ACCESSOR(DrawDebugPathOptimization, "ai.crowd.DrawDebugPathOptimization");
	DEFINE_EXTERNAL_CVAR_ACCESSOR(DrawDebugNeighbors, "ai.crowd.DrawDebugNeighbors");
	DEFINE_EXTERNAL_CVAR_ACCESSOR(DrawDebugBoundaries, "ai.crowd.DrawDebugBoundaries");

#undef DEFINE_EXTERNAL_CVAR_ACCESSOR

	const FVector Offset(0, 0, 20);

	const FColor Corner(128, 0, 0);
	const FColor CornerLink(192, 0, 0);
	const FColor CornerFixed(192, 192, 0);
	const FColor CollisionRange(192, 0, 128);
	const FColor CollisionSeg0(192, 0, 128);
	const FColor CollisionSeg1(96, 0, 64);
	const FColor CollisionSegIgnored(128, 128, 128);
	const FColor Path(255, 255, 255);
	const FColor PathSpecial(255, 192, 203);
	const FColor PathOpt(0, 128, 0);
	const FColor AvoidanceRange(255, 255, 255);
	const FColor Neighbor(0, 192, 128);

	const float LineThickness = 3.f;
}

UVOManager::UVOManager(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
#if WITH_RECAST
	DetourAvoidanceDebug = dtAllocObstacleAvoidanceDebugData();
	if (DetourAvoidanceDebug)
	{
		DetourAvoidanceDebug->init(2048);
	}

	DetourAgentDebug = new dtCrowdAgentDebugInfo();
	FMemory::Memzero(DetourAgentDebug, sizeof(dtCrowdAgentDebugInfo));
	DetourAgentDebug->idx = -1;
	DetourAgentDebug->vod = DetourAvoidanceDebug;
#endif
}

void UVOManager::BeginDestroy()
{
#if WITH_RECAST
	if (DetourAvoidanceDebug)
	{
		dtFreeObstacleAvoidanceDebugData(DetourAvoidanceDebug);
		DetourAvoidanceDebug = nullptr;
	}
	delete DetourAgentDebug;
	DetourAgentDebug = nullptr;
#endif
	Super::BeginDestroy();
}

void UVOManager::OnNavDataRegistered(ANavigationData& NavDataInstance)
{
	ARecastNavMesh* RecastNavMesh = Cast<ARecastNavMesh>(&NavDataInstance);
	if (RecastNavMesh == nullptr)
		return;

	dtNavMesh* DetourMesh = RecastNavMesh->GetRecastMesh();
	if (DetourMesh == nullptr)
		return;
	
	dtCrowd* NewDetourCrowd = dtAllocCrowd();

	// TODO: Remove hardcode
	const int32 MaxAgents = 50;
	const float MaxAgentRadius = RecastNavMesh->AgentRadius;

	if (NewDetourCrowd->init(MaxAgents, MaxAgentRadius, DetourMesh))
	{
		// TODO: Remove hardcode
		NewDetourCrowd->initAvoidance(6, 8, 1);

		FCrowdContext Context;
		Context.Crowd = NewDetourCrowd;
		ContextMap.Add(&NavDataInstance, Context);
	}
	else
	{
		dtFreeCrowd(NewDetourCrowd);
	}
}

void UVOManager::OnNavDataUnregistered(ANavigationData& NavDataInstance)
{
	FCrowdContext& Context = ContextMap.FindChecked(&NavDataInstance);

	UE_LOG(LogTemp, Log, TEXT("CrowdManager: Unregistering NavMesh %s"), *NavDataInstance.GetName());

	for (int32 i = GlobalAgentList.Num() - 1; i >= 0; --i)
	{
		if (GlobalAgentList[i].NavData == &NavDataInstance)
		{
			GlobalAgentList.RemoveAtSwap(i);
		}
	}

	if (Context.Crowd)
	{
		dtFreeCrowd(Context.Crowd);
		Context.Crowd = nullptr;
	}

	/*if (Context->NavQuery)
	{
		dtFreeNavMeshQuery(Context->NavQuery);
		Context->NavQuery = nullptr;
	}*/

	ContextMap.Remove(&NavDataInstance);
}

void UVOManager::RegisterAgent(UVOFollowingComponent* Agent)
{
	check(Agent != nullptr);
	ANavigationData* NavData = nullptr;

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	// TODO: Maybe give designer a choice?
	float AgentRadius = Agent->GetAgentRadius();
	// TODO: Replace hardcode
	float AgentHeight = 400.f;
	FVector Extent = FVector(AgentRadius, AgentRadius, AgentHeight * 0.5f);

	ANavigationData* BestNavData = NavSys->GetNavDataForProps(
		FNavAgentProperties(AgentRadius, AgentHeight),
		Agent->GetOwnerLocation(),
		Extent
	);

	FCrowdContext& Context = ContextMap.FindChecked(BestNavData);

	dtCrowdAgentParams Params;
	FMemory::Memzero(&Params, sizeof(Params));

	Params.radius = AgentRadius;
	Params.height = AgentHeight;
	Params.maxAcceleration = Agent->GetEffectiveParams().MaxAcceleration;
	Params.maxSpeed = Agent->GetEffectiveParams().MaxSpeed;
	Params.collisionQueryRange = AgentRadius * 12.0f; // TODO: check
	Params.pathOptimizationRange = AgentRadius * 30.f;
	Params.updateFlags = DT_CROWD_ANTICIPATE_TURNS | DT_CROWD_OPTIMIZE_VIS | DT_CROWD_OBSTACLE_AVOIDANCE;
	// TODO: Add path optimization counter

	// Add agent in detour
	FVector Loc = Agent->GetCrowdAgentLocation();
	FVector RecastLocVec = Unreal2RecastPoint(Loc);
	dtReal RecastLoc[3];
	RecastLoc[0] = RecastLocVec.X;
	RecastLoc[1] = RecastLocVec.Y;
	RecastLoc[2] = RecastLocVec.Z;

	const dtQueryFilter* Filter = Context.Crowd->getFilter(0);
	int32 DetourIdx = Context.Crowd->addAgent(RecastLoc, Params, Filter);

	if (DetourIdx != -1)
	{
		FGlobalAgentEntry Entry;
		Entry.Agent = Agent;
		Entry.NavData = BestNavData;
		Entry.DetourAgentIndex = DetourIdx;

		GlobalAgentList.Add(Entry);
		Agent->VOManagerIndex = GlobalAgentList.Num() - 1;
	}
}

void UVOManager::UnregisterAgent(UVOFollowingComponent* Agent)
{
	check(Agent != nullptr);

	int32 IdxToRemove = Agent->VOManagerIndex;
	FGlobalAgentEntry& Entry = GlobalAgentList[IdxToRemove];

	FCrowdContext Context = ContextMap.FindChecked(Entry.NavData);

	check(Context.Crowd);
	Context.Crowd->removeAgent(Entry.DetourAgentIndex);
	
	GlobalAgentList.RemoveAtSwap(IdxToRemove);
	
	GlobalAgentList[IdxToRemove].Agent->VOManagerIndex = IdxToRemove;

	Agent->VOManagerIndex = INDEX_NONE;
}

bool UVOManager::SetAgentMovePath(const UVOFollowingComponent* AgentComponent, const FNavMeshPath* Path, int32 PathSectionStart, int32 PathSectionEnd, const FVector& PathSectionEndLocation) const
{
	bool bSuccess = false;

#if WITH_RECAST
	if (!GlobalAgentList.IsValidIndex(AgentComponent->VOManagerIndex))
		return false;
	const FGlobalAgentEntry& Entry = GlobalAgentList[AgentComponent->VOManagerIndex];
	const int32 AgentIndex = Entry.DetourAgentIndex;

	// Get Navmesh
	const FCrowdContext* ContextPtr = ContextMap.Find(Entry.NavData);
	if (!ContextPtr)
		return false;

	dtCrowd* DetourCrowd = ContextPtr->Crowd;
	if (!DetourCrowd)
		return false;
	
	ARecastNavMesh* RecastNavData = Cast<ARecastNavMesh>(Entry.NavData);
	if (!RecastNavData)
		return false;

	if (Path && (Path->GetPathPoints().Num() > 1) &&
		Path->PathCorridor.IsValidIndex(PathSectionStart) &&
		Path->PathCorridor.IsValidIndex(PathSectionEnd))
	{
		FVector TargetPos = PathSectionEndLocation;
		if (PathSectionEnd < (Path->PathCorridor.Num() - 1))
		{
			RecastNavData->GetPolyCenter(Path->PathCorridor[PathSectionEnd], TargetPos);
		}

		TArray<dtPolyRef> PathRefs;
		for (int32 Idx = PathSectionStart; Idx <= PathSectionEnd; Idx++)
		{
			PathRefs.Add(Path->PathCorridor[Idx]);
		}

		const INavigationQueryFilterInterface* NavFilter = Path->GetFilter().IsValid() ? Path->GetFilter()->GetImplementation() : Entry.NavData->GetDefaultQueryFilterImpl();
		const dtQueryFilter* DetourFilter = ((const FRecastQueryFilter*)NavFilter)->GetAsDetourQueryFilter();

		DetourCrowd->updateAgentFilter(AgentIndex, DetourFilter);
		DetourCrowd->updateAgentState(AgentIndex, false);

		const FVector RcTargetPos = Unreal2RecastPoint(TargetPos);
		bSuccess = DetourCrowd->requestMoveTarget(AgentIndex, PathRefs.Last(), &RcTargetPos.X);
		if (bSuccess)
		{
			bSuccess = DetourCrowd->setAgentCorridor(AgentIndex, PathRefs.GetData(), PathRefs.Num());
		}
	}
#endif

	return bSuccess;
}

void UVOManager::Tick(float DeltaTime)
{
	LLM_SCOPE_BYTAG(VOAO);
	TRACE_CPUPROFILER_EVENT_SCOPE(UVOManager::Tick);

	if (GlobalAgentList.Num() == 0)
		return;
	
	for (FGlobalAgentEntry& It : GlobalAgentList)
	{
		UVOFollowingComponent* Comp = It.Agent;
		if (Comp) Comp->UpdateKinematics(DeltaTime);
	}

	PrepareAgentsStep();

	for (auto& Pair : ContextMap)
	{
		dtCrowd* Crowd = Pair.Value.Crowd;
		check(Crowd);

		int32 NumActive = Crowd->cacheActiveAgents();
	}

	for (auto& Pair : ContextMap)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(UVOManager::Tick_Detour);
		dtCrowd* Crowd = Pair.Value.Crowd;
		check(Crowd);

		Crowd->cacheActiveAgents();
		Crowd->updateStepCorridor(DeltaTime, DetourAgentDebug);
		Crowd->updateStepPaths(DeltaTime, DetourAgentDebug);
		Crowd->updateStepProximityData(DeltaTime, DetourAgentDebug);
		Crowd->updateStepNextMovePoint(DeltaTime, DetourAgentDebug);
	}

	UpdateAvoidance();

#ifdef SAVE_VO_PATHS
	for (int32 i = GlobalAgentList.Num() - 1; i >= 0; --i)
	{
		UVOFollowingComponent* Comp = GlobalAgentList[i].Agent;
		if (Comp) Comp->UpdateKinematics(DeltaTime);
	
		// vo.ShowPaths
		Comp->UpdatePathHistory(DeltaTime);
		if (CVarVODebugShowPaths.GetValueOnAnyThread() != 0)
		{
			DrawAgentPath(Comp, Comp->PathHistory, AvoidanceMath::GetColorFromSeed(i));
		}
	}
#endif

#if WITH_EDITOR
	DebugTick();
#endif
}

void UVOManager::PrepareAgentsStep() const
{
	for (const FGlobalAgentEntry& Entry : GlobalAgentList)
	{
		const FCrowdContext& Ctx = ContextMap.FindChecked(Entry.NavData);

		dtCrowdAgent* ag = (dtCrowdAgent*)Ctx.Crowd->getAgent(Entry.DetourAgentIndex);

		FVector RcLocation = Unreal2RecastPoint(Entry.Agent->GetCrowdAgentLocation());
		FVector RcVelocity = Unreal2RecastPoint(Entry.Agent->GetOwnerVelocity());

		dtVcopy(ag->npos, &RcLocation.X);
		dtVcopy(ag->vel, &RcVelocity.X);

		// TODO: Add syncing for all params (Event-Driven maybe)
		
		/*if (AgentData.bWantsPathOptimization)
		{
			AgentData.PathOptRemainingTime -= DeltaTime;
			if (AgentData.PathOptRemainingTime > 0)
			{
				ag->params.updateFlags &= ~DT_CROWD_OPTIMIZE_VIS;
			}
			else
			{
				ag->params.updateFlags |= DT_CROWD_OPTIMIZE_VIS;
				AgentData.PathOptRemainingTime = PathOptimizationInterval;
			}
		}*/
	}	
}

void UVOManager::UpdateAvoidance()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UVOManager::UpdateAvoidance);
	for (int32 i = GlobalAgentList.Num() - 1; i >= 0; --i)
	{
		auto* Comp = GlobalAgentList[i].Agent;

		if (!Comp) { GlobalAgentList.RemoveAtSwap(i); continue; }

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
			GatherNeighbors(GlobalAgentList[i], Params, Neis);
		}
		
		// Prepare Buffers
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(UVOManager::PrepareArrays);
			PrepareArrays(Neis.Num());
		}

		const FCrowdContext& dtCtx = ContextMap.FindChecked(GlobalAgentList[i].NavData);
		const dtCrowdAgent* dtAgent = dtCtx.Crowd->getAgent(GlobalAgentList[i].DetourAgentIndex);
		FVector CurrentTarget;
		if (dtAgent->ncorners > 0)
		{
			CurrentTarget = Recast2UnrealPoint(&dtAgent->cornerVerts[0]);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("No corners for agent %s"), *Comp->GetName());
			continue;
		}

		// VELOCITY OBSTACLE
		if (Comp->GetAvoidanceStyle() == EAvoidanceStyle::VelocityObstacle)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(UVOManager::VO);

			// Calculate Desired Velocity based on Goal
			FVector DesiredVel = FVector::ZeroVector;
			FVector TargetPos = FVector::ZeroVector;
			bool bHasTarget = false;

			if (Comp->HasVOGoal())
			{
				TargetPos = Comp->GetMoveGoal()->GetActorLocation();
				bHasTarget = true;
			}
			else
			{
				if (dtAgent->targetState != DT_CROWDAGENT_TARGET_NONE)
				{
					TargetPos = Recast2UnrealPoint(&dtAgent->cornerVerts[0]);
					bHasTarget = true;
				}
			}

			if (bHasTarget)
			{
				const FVector To = (TargetPos - Pos);
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

			/*
			const FCrowdContext& dtCtx = ContextMap.FindChecked(GlobalAgentList[i].NavData);
			const dtCrowdAgent* dtAgent = dtCtx.Crowd->getAgent(GlobalAgentList[i].DetourAgentIndex);
			*/
			if (dtAgent->targetState == DT_CROWDAGENT_TARGET_NONE)
				continue;
			
			TargetPos = CurrentTarget;
			
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
	const FGlobalAgentEntry& AgentEntry,
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
	
	UVOFollowingComponent* Agent = AgentEntry.Agent;
	const FVector Pos = Agent->GetOwnerLocation();
	const float NeighborRangeSqr = FMath::Square(Params.NeighborRange);

	// Gather agents
    for (FGlobalAgentEntry& It : GlobalAgentList)
    {
       	UVOFollowingComponent* Other = It.Agent;
    	const FVector& OtherPos = Other->GetOwnerLocation();
       	
       	if (!Other || Other == Agent)
       		continue;
       	if (FVector::DistSquared2D(Pos, OtherPos) > NeighborRangeSqr)
       		continue;
    	if (FMath::Abs(Pos.Z - OtherPos.Z) >= (Params.AgentHeight+Other->GetAgentHeight()) / 2.0f)
    		continue;
       	// TODO: Add FOV for Agents
	
       	float t = -1.f;
       	switch (Params.AvoidanceStyle)
       	{
       	    case EAvoidanceStyle::VelocityObstacle:
       	       t = CalculateCCT_VO(Agent, Params, Other);
       	       break;
       	    case EAvoidanceStyle::AccelerationObstacle:
       	       t = CalculateCCT_AO(Agent, Params, Other);
       	       break;
       	    default:
       	       continue; 
       	}

    	TryAddCandidate(FCandidate(Other, t));
    }

	// Gather static segments
	const FCrowdContext& dtCtx = ContextMap.FindChecked(AgentEntry.NavData);
	const dtCrowdAgent* dtAgent = dtCtx.Crowd->getAgent(AgentEntry.DetourAgentIndex);
	for (int j = 0; j < dtAgent->boundary.getSegmentCount(); ++j)
	{
		const dtReal* s = dtAgent->boundary.getSegment(j);
		const dtReal* q = s + 3;
			
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
			t = CalculateCCT_VO(Agent, Params, P1, P2);
			break;
		case EAvoidanceStyle::AccelerationObstacle:
			t = CalculateCCT_AO(Agent, Params, P1, P2);
			break;
		default:
			continue; 
		} 

		Candidates.Add(FCandidate(P1, P2, t));
		//TryAddCandidate(FCandidate(P1, P2, t));
	}

	Neis.Reserve(Candidates.Num());

	for (const FCandidate& Cand : Candidates)
	{
		if (Cand.bIsAgent)
		{
			FVector NPos = Cand.AgentComp->GetOwnerLocation();
			FVector NVel = Cand.AgentComp->GetCachedVelocity();
			FVector NAcc = Cand.AgentComp->GetCachedAcceleration();
			float MinkRadius = Cand.AgentComp->GetAgentRadius() + Params.AgentRadius;

			Neis.Add(FVONeighborView::CreateCircle(NPos, NVel, NAcc, MinkRadius, Cand.t, ENeighborType::Dynamic));
		}
		else
		{
			FVector P1 = FVector(Cand.SegP1.X, Cand.SegP1.Y, 0);
			FVector P2 = FVector(Cand.SegP2.X, Cand.SegP2.Y, 0);
			
			Neis.Add(FVONeighborView::CreateStaticSegment(P1, P2, Cand.t, NeighborVerticesBuffer));
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
			true,
			15.f, 
			0, 
			2.0f
		);
	}
}

UWorld* UVOManager::GetWorld() const
{
	UNavigationSystemV1* NavSys = Cast<UNavigationSystemV1>(GetOuter());
	return NavSys ? NavSys->GetWorld() : NULL;
}

UVOManager* UVOManager::GetCurrent(UObject* WorldContextObject)
{
	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(WorldContextObject);
	return NavSys ? Cast<UVOManager>(NavSys->GetCrowdManager()) : NULL;
}

UVOManager* UVOManager::GetCurrent(UWorld* World)
{
	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	return NavSys ? Cast<UVOManager>(NavSys->GetCrowdManager()) : NULL;
}

#pragma region DEBUG_CROWD
#if WITH_RECAST

#if ENABLE_DRAW_DEBUG
UWorld* UVOManager::GetDebugDrawingWorld() const
{
	UWorld* DebugDrawingWorld = GetWorld();

#if WITH_EDITORONLY_DATA
	// note that being ENetMode::NM_DedicatedServer implies DebugDrawingWorld is a game world, which is exactly what we need
	if (DebugDrawingWorld != nullptr && DebugDrawingWorld->GetNetMode() == ENetMode::NM_DedicatedServer)
	{
		// no point in trying to draw on dedicated server. Let's see if there's a client world we can use for drawing!
		const TIndirectArray<FWorldContext>& WorldContexts = GEngine->GetWorldContexts();
		for (const FWorldContext& Context : WorldContexts)
		{
			if (Context.World()->IsGameWorld() && Context.World()->GetNetMode() != ENetMode::NM_DedicatedServer)
			{
				DebugDrawingWorld = Context.World();
				break;
			}
		}
	}
#endif

	return DebugDrawingWorld;
}

void UVOManager::DrawDebugCorners(const FGlobalAgentEntry* Agent) const
{
	UWorld* DebugDrawingWorld = GetDebugDrawingWorld();

	ANavigationData* NavData = Agent->NavData;
	const dtCrowdAgent* CrowdAgent = ContextMap.FindChecked(NavData).Crowd->getAgent(Agent->DetourAgentIndex);

	{
		FVector P0 = Recast2UnrealPoint(CrowdAgent->npos);
		for (int32 Idx = 0; Idx < CrowdAgent->ncorners; Idx++)
		{
			FVector P1 = Recast2UnrealPoint(&CrowdAgent->cornerVerts[Idx * 3]);
			DrawDebugLine(DebugDrawingWorld, P0 + FExternalCrowdDebug::Offset, P1 + FExternalCrowdDebug::Offset, FExternalCrowdDebug::Corner, false, -1.0f, SDPG_World, 2.0f);
			P0 = P1;
		}
	}

	if (CrowdAgent->ncorners > 0 && (CrowdAgent->cornerFlags[CrowdAgent->ncorners - 1] & DT_STRAIGHTPATH_OFFMESH_CONNECTION))
	{
		FVector P0 = Recast2UnrealPoint(&CrowdAgent->cornerVerts[(CrowdAgent->ncorners - 1) * 3]);
		DrawDebugLine(DebugDrawingWorld, P0, P0 + FExternalCrowdDebug::Offset * 2.0f, FExternalCrowdDebug::CornerLink, false, -1.0f, SDPG_World, 2.0f);
	}
}

void UVOManager::DrawDebugCollisionSegments(const FGlobalAgentEntry* Agent) const
{
	UWorld* DebugDrawingWorld = GetDebugDrawingWorld();
	
	ANavigationData* NavData = Agent->NavData;
	const dtCrowdAgent* CrowdAgent = ContextMap.FindChecked(NavData).Crowd->getAgent(Agent->DetourAgentIndex);

	FVector Center = Recast2UnrealPoint(CrowdAgent->boundary.getCenter()) + FExternalCrowdDebug::Offset;
	DrawDebugCylinder(DebugDrawingWorld, Center - FExternalCrowdDebug::Offset, Center, UE_REAL_TO_FLOAT_CLAMPED_MAX(CrowdAgent->params.collisionQueryRange), 32, FExternalCrowdDebug::CollisionRange);

	for (int32 Idx = 0; Idx < CrowdAgent->boundary.getSegmentCount(); Idx++)
	{
		const FVector::FReal* s = CrowdAgent->boundary.getSegment(Idx);
		const int32 SegFlags = CrowdAgent->boundary.getSegmentFlags(Idx);
		const FColor Color = (SegFlags & DT_CROWD_BOUNDARY_IGNORE) ? FExternalCrowdDebug::CollisionSegIgnored :
			(dtTriArea2D(CrowdAgent->npos, s, s + 3) < 0.0f) ? FExternalCrowdDebug::CollisionSeg1 :
			FExternalCrowdDebug::CollisionSeg0;

		FVector Pt0 = Recast2UnrealPoint(s);
		FVector Pt1 = Recast2UnrealPoint(s + 3);

		DrawDebugLine(DebugDrawingWorld, Pt0 + FExternalCrowdDebug::Offset, Pt1 + FExternalCrowdDebug::Offset, Color, false, -1.0f, SDPG_World, 3.5f);
	}
}

void UVOManager::DrawDebugPath(const FGlobalAgentEntry* Agent) const
{
	UWorld* DebugDrawingWorld = GetDebugDrawingWorld();

	ANavigationData* NavData = Agent->NavData;
	const dtCrowdAgent* CrowdAgent = ContextMap.FindChecked(NavData).Crowd->getAgent(Agent->DetourAgentIndex);
	
	ARecastNavMesh* NavMesh = Cast<ARecastNavMesh>(NavData);
	if (NavMesh == NULL)
	{
		return;
	}

	NavMesh->BeginBatchQuery();
	
	const dtPolyRef* Path = CrowdAgent->corridor.getPath();
	TArray<FVector> Verts;

	for (int32 Idx = 0; Idx < CrowdAgent->corridor.getPathCount(); Idx++)
	{
		Verts.Reset();
		NavMesh->GetPolyVerts(Path[Idx], Verts);

		uint16 PolyFlags = 0;
		uint16 AreaFlags = 0;
		NavMesh->GetPolyFlags(Path[Idx], PolyFlags, AreaFlags);
		const FColor PolyColor = AreaFlags != 1 ? FExternalCrowdDebug::Path : FExternalCrowdDebug::PathSpecial;

		for (int32 VertIdx = 0; VertIdx < Verts.Num(); VertIdx++)
		{
			const FVector Pt0 = Verts[VertIdx];
			const FVector Pt1 = Verts[(VertIdx + 1) % Verts.Num()];

			DrawDebugLine(DebugDrawingWorld, Pt0 + FExternalCrowdDebug::Offset * 0.5f, Pt1 + FExternalCrowdDebug::Offset * 0.5f, PolyColor, false
				, /*LifeTime*/-1.f, /*DepthPriority*/0
				, /*Thickness*/FExternalCrowdDebug::LineThickness);
		}
	}

	NavMesh->FinishBatchQuery();
}

void UVOManager::DrawDebugPathOptimization(const FGlobalAgentEntry* Agent) const
{
	UWorld* DebugDrawingWorld = GetDebugDrawingWorld();

	FVector Pt0 = Recast2UnrealPoint(DetourAgentDebug->optStart) + FExternalCrowdDebug::Offset * 1.25f;
	FVector Pt1 = Recast2UnrealPoint(DetourAgentDebug->optEnd) + FExternalCrowdDebug::Offset * 1.25f;

	DrawDebugLine(DebugDrawingWorld, Pt0, Pt1, FExternalCrowdDebug::PathOpt, false, -1.0f, SDPG_World, 2.5f);
}

/*void UVOManager::DrawDebugNeighbors(const FGlobalAgentEntry* Agent) const
{
	UWorld* DebugDrawingWorld = GetDebugDrawingWorld();

	ANavigationData* NavData = Agent->NavData;
	const dtCrowdAgent* CrowdAgent = ContextMap.FindChecked(NavData).Crowd->getAgent(Agent->DetourAgentIndex);

	FVector Center = Recast2UnrealPoint(CrowdAgent->npos) + FExternalCrowdDebug::Offset;
	DrawDebugCylinder(DebugDrawingWorld, Center - FExternalCrowdDebug::Offset, Center, UE_REAL_TO_FLOAT_CLAMPED_MAX(CrowdAgent->params.collisionQueryRange), 32, FExternalCrowdDebug::CollisionRange);

	for (int32 Idx = 0; Idx < CrowdAgent->nneis; Idx++)
	{
		const dtCrowdAgent* nei = DetourCrowd->getAgent(CrowdAgent->neis[Idx].idx);
		if (nei)
		{
			FVector Pt0 = Recast2UnrealPoint(nei->npos) + FExternalCrowdDebug::Offset;
			DrawDebugLine(DebugDrawingWorld, Center, Pt0, FExternalCrowdDebug::Neighbor);
		}
	}
}*/

void UVOManager::DrawDebugSharedBoundary() const
{
	UWorld* DebugDrawingWorld = GetDebugDrawingWorld();

	FColor Colors[] = { FColorList::Red, FColorList::Orange };

	for (auto& Pair : ContextMap)
	{
		const dtCrowd* DetourCrowd = Pair.Value.Crowd;

		const dtSharedBoundary* sharedBounds = DetourCrowd->getSharedBoundary();
		for (int32 Idx = 0; Idx < sharedBounds->Data.Num(); Idx++)
		{
			FColor Color = Colors[Idx % UE_ARRAY_COUNT(Colors)];
			const FVector Center = Recast2UnrealPoint(sharedBounds->Data[Idx].Center);
			DrawDebugCylinder(DebugDrawingWorld, Center - FExternalCrowdDebug::Offset, Center, UE_REAL_TO_FLOAT_CLAMPED_MAX(sharedBounds->Data[Idx].Radius), 32, Color);

			for (int32 WallIdx = 0; WallIdx < sharedBounds->Data[Idx].Edges.Num(); WallIdx++)
			{
				const FVector WallV0 = Recast2UnrealPoint(sharedBounds->Data[Idx].Edges[WallIdx].v0) + FExternalCrowdDebug::Offset;
				const FVector WallV1 = Recast2UnrealPoint(sharedBounds->Data[Idx].Edges[WallIdx].v1) + FExternalCrowdDebug::Offset;

				DrawDebugLine(DebugDrawingWorld, WallV0, WallV1, Color);
			}
		}
	}
}
#endif // ENABLE_DRAW_DEBUG

void UVOManager::UpdateSelectedDebug(const ICrowdAgentInterface* Agent, int32 AgentIndex) const
{
#if WITH_EDITOR
	const UObject* Obj = Cast<const UObject>(Agent);
	if (GIsEditor && Obj)
	{
		const AController* TestController = Cast<const AController>(Obj->GetOuter());
		if (TestController && TestController->GetPawn() && TestController->GetPawn()->IsSelected())
		{
			DetourAgentDebug->idx = AgentIndex;
		}
	}
#endif
}

#endif // WITH_RECAST

#if WITH_EDITOR

void UVOManager::DebugTick() const
{
#if WITH_RECAST
	if (ContextMap.Num() == 0 || DetourAgentDebug == NULL)
	{
		return;
	}

	for (int32 i = 0; i < GlobalAgentList.Num(); ++i)
	{
		const FGlobalAgentEntry& It = GlobalAgentList[i];
		const UVOFollowingComponent* Agent = It.Agent;
		ANavigationData* NavData = It.NavData;
		const dtCrowdAgent* CrowdAgent = ContextMap.FindChecked(NavData).Crowd->getAgent(It.DetourAgentIndex);
		if (CrowdAgent)
		{
			UpdateSelectedDebug(Agent, i);
		}
	}
	
#if ENABLE_DRAW_DEBUG
	// on screen debugging
	const FGlobalAgentEntry* SelectedAgent = NULL;
	if (DetourAgentDebug->idx >= 0)
		SelectedAgent = &GlobalAgentList[DetourAgentDebug->idx];
	if (SelectedAgent && FExternalCrowdDebug::DebugSelectedActors())
	{
		if (FExternalCrowdDebug::DrawDebugCorners())
		{
			DrawDebugCorners(SelectedAgent);
		}

		if (FExternalCrowdDebug::DrawDebugCollisionSegments())
		{
			DrawDebugCollisionSegments(SelectedAgent);
		}

		if (FExternalCrowdDebug::DrawDebugPath())
		{
			DrawDebugPath(SelectedAgent);
		}

		if (FExternalCrowdDebug::DrawDebugPathOptimization())
		{
			DrawDebugPathOptimization(SelectedAgent);
		}

		/*if (FExternalCrowdDebug::DrawDebugNeighbors)
		{
			DrawDebugNeighbors(SelectedAgent);
		}*/
	}

	if (FExternalCrowdDebug::DrawDebugBoundaries())
	{
		DrawDebugSharedBoundary();
	}
#endif // ENABLE_DRAW_DEBUG

	// vislog debugging
	if (FExternalCrowdDebug::DebugVisLog())
	{
		for (int i = 0; i < GlobalAgentList.Num(); ++i)
		{
			const FGlobalAgentEntry& It = GlobalAgentList[i];
			ANavigationData* NavData = It.NavData;
			const dtCrowdAgent* CrowdAgent = ContextMap.FindChecked(NavData).Crowd->getAgent(It.DetourAgentIndex);
			
			const ICrowdAgentInterface* IAgent = It.Agent;
			const UObject* AgentOb = IAgent ?  Cast<const UObject>(IAgent) : NULL;
			const AActor* LogOwner = AgentOb ? Cast<const AActor>(AgentOb->GetOuter()) : NULL;

			if (CrowdAgent && LogOwner)
			{
				FString LogData = DetourAgentDebug->agentLog.FindRef(i);
				if (LogData.Len() > 0)
				{
					UE_VLOG(LogOwner, LogVOFollowing, Log, TEXT("%s"), *LogData);
				}

				{
					FVector P0 = Recast2UnrealPoint(CrowdAgent->npos);
					for (int32 Idx = 0; Idx < CrowdAgent->ncorners; Idx++)
					{
						FVector P1 = Recast2UnrealPoint(&CrowdAgent->cornerVerts[Idx * 3]);
						UE_VLOG_SEGMENT(LogOwner, LogVOFollowing, Log, P0 + FExternalCrowdDebug::Offset, P1 + FExternalCrowdDebug::Offset, FExternalCrowdDebug::Corner, TEXT(""));
						UE_VLOG_BOX(LogOwner, LogVOFollowing, Log, FBox::BuildAABB(P1 + FExternalCrowdDebug::Offset, FVector(2, 2, 2)), FExternalCrowdDebug::Corner, TEXT("%d"), CrowdAgent->cornerFlags[Idx]);
						P0 = P1;
					}
				}

				ARecastNavMesh* RecastNavData = Cast<ARecastNavMesh>(NavData);
				if (RecastNavData)
				{
					for (int32 Idx = 0; Idx < CrowdAgent->corridor.getPathCount(); Idx++)
					{
						dtPolyRef PolyRef = CrowdAgent->corridor.getPath()[Idx];
						TArray<FVector> PolyPoints;
						RecastNavData->GetPolyVerts(PolyRef, PolyPoints);

						UE_VLOG_CONVEXPOLY(LogOwner, LogVOFollowing, Verbose, PolyPoints, FColor::Cyan, TEXT(""));
					}
				}

				if (CrowdAgent->ncorners && (CrowdAgent->cornerFlags[CrowdAgent->ncorners - 1] & DT_STRAIGHTPATH_OFFMESH_CONNECTION))
				{
					FVector P0 = Recast2UnrealPoint(&CrowdAgent->cornerVerts[(CrowdAgent->ncorners - 1) * 3]);
					UE_VLOG_SEGMENT(LogOwner, LogVOFollowing, Log, P0, P0 + FExternalCrowdDebug::Offset * 2.0f, FExternalCrowdDebug::CornerLink, TEXT(""));
				}

				if (CrowdAgent->corridor.hasNextFixedCorner())
				{
					FVector P0 = Recast2UnrealPoint(CrowdAgent->corridor.getNextFixedCorner());
					UE_VLOG_BOX(LogOwner, LogVOFollowing, Log, FBox::BuildAABB(P0 + FExternalCrowdDebug::Offset, FVector(10, 10, 10)), FExternalCrowdDebug::CornerFixed, TEXT(""));
				}

				if (CrowdAgent->corridor.hasNextFixedCorner2())
				{
					FVector P0 = Recast2UnrealPoint(CrowdAgent->corridor.getNextFixedCorner2());
					UE_VLOG_BOX(LogOwner, LogVOFollowing, Log, FBox::BuildAABB(P0 + FExternalCrowdDebug::Offset, FVector(10, 10, 10)), FExternalCrowdDebug::CornerFixed, TEXT(""));
				}

				for (int32 Idx = 0; Idx < CrowdAgent->boundary.getSegmentCount(); Idx++)
				{
					const FVector::FReal* s = CrowdAgent->boundary.getSegment(Idx);
					const int32 SegFlags = CrowdAgent->boundary.getSegmentFlags(Idx);
					const FColor Color = (SegFlags & DT_CROWD_BOUNDARY_IGNORE) ? FExternalCrowdDebug::CollisionSegIgnored :
						(dtTriArea2D(CrowdAgent->npos, s, s + 3) < 0.0f) ? FExternalCrowdDebug::CollisionSeg1 :
						FExternalCrowdDebug::CollisionSeg0;

					FVector Pt0 = Recast2UnrealPoint(s);
					FVector Pt1 = Recast2UnrealPoint(s + 3);

					UE_VLOG_SEGMENT_THICK(LogOwner, LogVOFollowing, Log, Pt0 + FExternalCrowdDebug::Offset, Pt1 + FExternalCrowdDebug::Offset, Color, 3, TEXT(""));
				}
			}
		}
	}

	DetourAgentDebug->agentLog.Reset();
#endif	// WITH_RECAST
}

#endif // WITH_EDITOR
#pragma endregion