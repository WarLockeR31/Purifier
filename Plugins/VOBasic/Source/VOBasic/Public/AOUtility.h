#pragma once

#include "CoreMinimal.h"
#include "VOFollowingComponent.h"

struct FVONeighborView;
struct FAOConesSoA;
struct FAOSegment;
struct FAOSide;
struct FAOCone;
struct FAOConeIntersection;

class VOBASIC_API AOUtility
{
public:
	static void BuildVOCones(const TArray<FVONeighborView>& Neis, const FVector& ActorPos, 
		const FVOParams& Params, FAOConesSoA& OutVOCones);
	
	static FAOCone ComputeVOCone(const float R, const FVector2D& C, const FVector2D& Vel, 
		const FVOParams& Params);

	static bool TryFindIntersections(FAOSide S1, FAOSide S2, FVector2D* OutPoint, float* OutT);

	// TODO: Do this functionality in FAOSide construction
	/*static bool TryFindSubSegmentInCircle(const FVector2D& P1, const FVector2D& P2, const float Radius,
		FVOSegment* OutSegment);*/
	
	static float ScoreAccelerationCandidate(const FVector2D& A, const FVector2D& DesiredAccel2D,
		const FVector2D& CurAccel2D, const TArray<FVONeighborView>& Neis,
		const FVector& ActorPos, const FVOParams& Params);
	
	static void DrawVOCones(const UVOFollowingComponent* Comp, const FAOConesSoA& Cones);
	
	static void DrawCombinedVO(const UVOFollowingComponent* Comp, 
		const TArray<TArray<FAOSegment>>& OutsideSegmentsByRays);
	
	static void DrawVelocityCandidates(const UVOFollowingComponent* Comp, 
		const TArray<FVector2D>& Candidates, int32 BestCandidateIdx,
		float PointSize, float LifeTime);

	static void CollectIntersections(const FAOConesSoA& VOCones, 
		TArray<TArray<FAOConeIntersection>>& IntersectionsByRays);
	
	static void SortIntersectionsByRays(TArray<TArray<FAOConeIntersection>>& IntersectionsByRays);
	
	static int32 CountVOsForPoint(const FAOConesSoA& VOCones, int32 ConeIndexToSkip, const FVector2D& P);
	
	static void ClassifySegments(const FAOConesSoA& VOCones, const FVOParams& Params, 
		const TArray<TArray<FAOConeIntersection>>& IntersectionsByRays,
		TArray<TArray<FAOSegment>>& OutsideSegmentsByRays);

	static FVector2D SelectBestVelocityFromOutsideSegments(
		const FVector2D& DesiredVel2D,
		const FVector2D& CurVel2D,
		const TArray<FVONeighborView>& Neis,
		const FVector& ActorPos,
		const FVOParams& Params,
		const TArray<TArray<FAOSegment>>& OutsideSegmentsByRays,
		TArray<FVector2D>& Debug_LastCandidates,
		int32& Debug_BestCandidateIdx
	);

	static FVector ComputeVelocity(const UVOFollowingComponent* Comp, 
		const FVector& CurVel, 
		const FVector& DesiredVel, 
		const TArray<FVONeighborView>& Neis, 
		const FVOParams& Params);
};
