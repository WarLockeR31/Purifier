#pragma once
#include "CoreMinimal.h"
#include "CrowdManagerBase.h"
#include "VOFollowingComponent.h"
#include "VelocityObstacleTypes.h"
#include "AccelerationObstacleTypes.h"
#include "VOManager.generated.h"

DECLARE_CYCLE_STAT(TEXT("VO Compute Velocity"), STAT_VOComputeVelocity, STATGROUP_Game);

USTRUCT()
struct FVONeighborView
{
	GENERATED_BODY()
	FVector Pos = FVector::ZeroVector;   // world XY
	FVector Vel = FVector::ZeroVector;   // world XY
	float Radius = 34.f;                 // cm
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

	FVOConesSoA VO_Cones;
	TArray<TArray<FVOConeIntersection>> IntersectionsByRays;
	TArray<TArray<FVOOutsideSegment>>	OutsideSegmentsByRays;
	
	FVector ComputeVelocity(const UVOFollowingComponent* Comp, const FVector& CurVel, const FVector& DesiredVel, const TArray<FVONeighborView>& Neis, const FVOParams& Params);

	mutable TArray<FVector2D> Debug_LastCandidates;
	mutable int32 Debug_BestCandidateIdx = -1;

	// Helpers
	void PrepareArrays(size_t NumNeis);
};