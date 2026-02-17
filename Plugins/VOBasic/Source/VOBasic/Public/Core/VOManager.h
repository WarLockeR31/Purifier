#pragma once
#include "CoreMinimal.h"
#include "Components/VOFollowingComponent.h"
#include "Navigation/CrowdManager.h"
#include "Types/VelocityObstacleTypes.h"
#include "Types/AccelerationObstacleTypes.h"
#include "VOManager.generated.h"

class dtCrowd;
class dtNavMeshQuery;
class ANavigationData;
class ICrowdAgentInterface;

struct FCrowdContext
{
	dtCrowd* Crowd = nullptr;
	// dtNavMeshQuery* NavQuery = nullptr;
};

struct FGlobalAgentEntry
{
	UVOFollowingComponent* Agent = nullptr;
	ANavigationData* NavData = nullptr;
	int32 DetourAgentIndex = -1;
};

DECLARE_CYCLE_STAT(TEXT("VO Compute Velocity"), STAT_VOComputeVelocity, STATGROUP_Game);
LLM_DECLARE_TAG(VOAO);

UENUM(BlueprintType)
enum class EMinkowskiShapeType : uint8
{
	None,
	Circle,
	Capsule,
	ConvexPolygon,
	RoundedRectangle,
};

UENUM(BlueprintType)
enum class ENeighborType : uint8
{
	None,
	Static,
	Dynamic,
};

USTRUCT()
struct FVONeighborView
{
	GENERATED_BODY()
	FVector Pos = FVector::ZeroVector;   // world XY
	FVector Vel = FVector::ZeroVector;   // world XY
	FVector Acc = FVector::ZeroVector;   // world XY
	float Radius = 0.f;					 // cm
	float CCT = 0.f;					 // Conservative Collision Time
	EMinkowskiShapeType ShapeType = EMinkowskiShapeType::None;
	ENeighborType NeighborType = ENeighborType::None;

	// Shape data: 0 = Circle (uses Pos as center), >0 = Number of vertices in Manager's buffer
	int32 VerticesOffset = -1;
	uint8 NumVertices = 0;

	FVONeighborView() = default;
	FVONeighborView(const FVector& Pos_, const FVector& Vel_, const FVector& Acc_, float Radius_, float CCT_, EMinkowskiShapeType ShapeType_)
	{
		Pos = Pos_; Vel = Vel_; Acc = Acc_; Radius = Radius_; CCT = CCT_; ShapeType = ShapeType_;
	}

	static FVONeighborView CreateCircle(const FVector& Pos, const FVector& Vel, const FVector& Acc, float Radius, float CCT)
	{
		FVONeighborView View(Pos, Vel, Acc, Radius, CCT, EMinkowskiShapeType::Circle);
		View.NumVertices = 0;
		return View;
	}

	static FVONeighborView CreateCapsule(const FVector& PosA, const FVector& PosB, const FVector& Vel, const FVector& Acc, float Radius, float CCT, TArray<FVector2D>& VertexBuffer)
	{
		FVONeighborView View((PosA + PosB) * 0.5f, Vel, Acc, Radius, CCT, EMinkowskiShapeType::Capsule);
		View.VerticesOffset = VertexBuffer.Num();
		View.NumVertices = 2;
		VertexBuffer.Add(FVector2D(PosA.X, PosA.Y));
		VertexBuffer.Add(FVector2D(PosB.X, PosB.Y));
		return View;
	}

	// TEMP
	static FVONeighborView CreateSeg(const FVector& PosA, const FVector& PosB, float CCT, TArray<FVector2D>& VertexBuffer)
	{
		FVONeighborView View;
		View.Pos = (PosA + PosB) * 0.5f;
		View.CCT = CCT;
		View.VerticesOffset = VertexBuffer.Num();
		View.NumVertices = 2;
		View.ShapeType = EMinkowskiShapeType::Capsule;
		VertexBuffer.Add(FVector2D(PosA.X, PosA.Y));
		VertexBuffer.Add(FVector2D(PosB.X, PosB.Y));
		return View;
	}
};

UCLASS()
class VOBASIC_API UVOManager : public UCrowdManagerBase
{
	GENERATED_BODY()
public:
	UVOManager(const FObjectInitializer& ObjectInitializer);
	virtual void BeginDestroy() override;
	
	virtual void Tick(float DeltaTime) override;
	
#if WITH_EDITOR
	void DebugTick() const;
#endif

	virtual void OnNavDataRegistered(ANavigationData& NavDataInstance) override;
	virtual void OnNavDataUnregistered(ANavigationData& NavDataInstance) override;

	void RegisterAgent(UVOFollowingComponent* Agent);
	void UnregisterAgent(UVOFollowingComponent* Agent);

	bool SetAgentMovePath(
		const UVOFollowingComponent* AgentComponent,
		const FNavMeshPath* Path,
		int32 PathSectionStart,
		int32 PathSectionEnd,
		const FVector& PathSectionEndLocation) const; // Copy-pasted from UE source-code, adapted for UVOController

	UWorld* GetWorld() const override;

	static UVOManager* GetCurrent(UObject* WorldContextObject);
	static UVOManager* GetCurrent(UWorld* World);

	const ANavigationData* GetNavData(const UVOFollowingComponent* Agent) const { return GlobalAgentList[Agent->VOManagerIndex].NavData; }
protected:
	void PrepareAgentsStep() const;
	
	void UpdateAvoidance();
	
private:
	TMap<ANavigationData*, FCrowdContext> ContextMap;
	TArray<FGlobalAgentEntry> GlobalAgentList;

	//TArray<TWeakObjectPtr<UVOFollowingComponent>> Agents;

	// Global buffer for current agent's neighbors' vertices (cleared per-agent)
	TArray<FVector2D> NeighborVerticesBuffer;

	// VO Buffers
	FVOConesSoA VO_Cones;
	TArray<TArray<FVOConeIntersection>> VO_Intersections;
	TArray<TArray<FVOOutsideSegment>>	VO_OutsideSegments;
	TArray<FVector2D> VO_Candidates;
	int32 VO_BestCandidateIdx = -1;

	// AO Buffers
	FAOConesSoA AO_Cones;
	TArray<FAOWorkSegment> AO_WorkSegments;
	TArray<TArray<FAOConeIntersection>> AO_SideIntersections;
	TArray<TArray<FAOSegment>> AO_OutsideSegments;
	TArray<FVector2D> AO_Candidates;
	int32 AO_BestCandidateIdx;

	void GatherNeighbors(const FGlobalAgentEntry& AgentEntry, const FVOParams& Params, TArray<FVONeighborView>& Neis);
	// CCT for agents
	float CalculateCCT_VO(
		const UVOFollowingComponent* Agent,
		const FVOParams& AgentParams,
		const UVOFollowingComponent* Obstacle);
	float CalculateCCT_AO(
		const UVOFollowingComponent* Agent,
		const FVOParams& AgentParams,
		const UVOFollowingComponent* Obstacle);
	// CCT for static obstacles
	float CalculateCCT_VO(
		const UVOFollowingComponent* Comp,
		const FVOParams& AgentParams,
		const FVector2D& P,
		const FVector2D& Q);
	float CalculateCCT_AO(
		const UVOFollowingComponent* Comp,
		const FVOParams& AgentParams,
		const FVector2D& P,
		const FVector2D& Q);

	// Helpers
	void PrepareArrays(size_t NumNeis);

	void DrawAgentPath(const UVOFollowingComponent* Comp, const TArray<FVector>& PathHistory, const FColor& Color = FColor::Red);

protected:
#if WITH_RECAST
	dtCrowdAgentDebugInfo* DetourAgentDebug = nullptr;
	dtObstacleAvoidanceDebugData* DetourAvoidanceDebug = nullptr;

	void UpdateSelectedDebug(const ICrowdAgentInterface* Agent, int32 AgentIndex) const;
#if ENABLE_DRAW_DEBUG
	UWorld* GetDebugDrawingWorld() const;
	void DrawDebugCorners(const FGlobalAgentEntry* Agent) const;
	void DrawDebugCollisionSegments(const FGlobalAgentEntry* Agent) const;
	void DrawDebugPath(const FGlobalAgentEntry* Agent) const;
	void DrawDebugPathOptimization(const FGlobalAgentEntry* Agent) const;
	//void DrawDebugNeighbors(const FGlobalAgentEntry* Agent) const;
	void DrawDebugSharedBoundary() const;
#endif // ENABLE_DRAW_DEBUG

#endif
};