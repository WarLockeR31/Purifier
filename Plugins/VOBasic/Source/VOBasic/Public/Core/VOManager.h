#pragma once
#include "CoreMinimal.h"
#include "CrowdManagerBase.h"
#include "Types/VelocityObstacleTypes.h"
#include "Types/AccelerationObstacleTypes.h"
#include "VOManager.generated.h"

DECLARE_CYCLE_STAT(TEXT("VO Compute Velocity"), STAT_VOComputeVelocity, STATGROUP_Game);

USTRUCT()
struct FVONeighborView
{
	GENERATED_BODY()
	FVector Pos = FVector::ZeroVector;   // world XY
	FVector Vel = FVector::ZeroVector;   // world XY
	FVector Acc = FVector::ZeroVector;   // world XY
	float Radius = 34.f;                 // cm
	float CCT = 0.f;					 // Conservative Collision Time

	FVONeighborView() = default;
	FVONeighborView(const FVector& Pos_, const FVector& Vel_, const FVector& Acc_, float Radius_, float CCT_)
	{
		Pos = Pos_; Vel = Vel_; Acc = Acc_; Radius = Radius_; CCT = CCT_;
	}
};

UCLASS()
class VOBASIC_API UVOManager : public UCrowdManagerBase
{
	GENERATED_BODY()
public:
	virtual void Tick(float DeltaTime)								override;
	virtual void OnNavDataRegistered(ANavigationData& NavData)		override;
	virtual void OnNavDataUnregistered(ANavigationData& NavData)	override;
	virtual void CleanUp(float DeltaTime)							override;

	void RegisterAgent(UVOFollowingComponent* Comp);
	void UnregisterAgent(UVOFollowingComponent* Comp);

private:
	TArray<TWeakObjectPtr<UVOFollowingComponent>> Agents;

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

	void GatherNeighbors(const UVOFollowingComponent* Comp, const FVOParams& Params, TArray<FVONeighborView>& Neis);
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
};