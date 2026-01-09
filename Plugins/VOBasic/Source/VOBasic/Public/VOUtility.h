#pragma once

#include "CoreMinimal.h"
#include "VOFollowingComponent.h"

struct FVONeighborView;
struct FVOCone;
struct FVOConesSoA;
struct FVOOutsideSegment;
struct FVOSegment;
struct FVOConeIntersection;

class VOBASIC_API VOUtility
{
public:
	static bool IsVelocityForbidden(const FVector2D& CandidateVA, const TArray<FVONeighborView>& Neis, 
		const FVector& ActorPos, const FVOParams& Params);
	
	static void BuildVOCones(const TArray<FVONeighborView>& Neis, const FVector& ActorPos, 
		const FVOParams& Params, FVOConesSoA& OutVOCones);
	
	static FVOCone ComputeVOCone(const float R, const FVector2D& C, const FVector2D& Vel, 
		const FVOParams& Params);
		
	static bool WillCollideWithinTau(const FVector2D& RelativePosition, const FVector2D& RelativeVelocity, 
		float Radius, float TimeHorizon, float* OutTOI);
	
	static float ScoreVelocityCandidate(const FVector2D& V, const FVector2D& DesiredVel2D,
		const FVector2D& CurVel2D, const TArray<FVONeighborView>& Neis,
		const FVector& ActorPos, const FVOParams& Params);
	
	static void DrawVOCones(const UVOFollowingComponent* Comp, const FVOConesSoA& Cones);
	
	static void DrawCombinedVO(const UVOFollowingComponent* Comp, 
		const TArray<TArray<FVOOutsideSegment>>& OutsideSegmentsByRays);
	
	static void DrawVelocityCandidates(const UVOFollowingComponent* Comp, 
		const TArray<FVector2D>& Candidates, int32 BestCandidateIdx,
		float PointSize, float LifeTime);

	static void CollectIntersections(const FVOConesSoA& VOCones, 
		TArray<TArray<FVOConeIntersection>>& IntersectionsByRays);
	
	static void SortIntersectionsByRays(TArray<TArray<FVOConeIntersection>>& IntersectionsByRays);
	
	static int32 CountVOsForPoint(const FVOConesSoA& VOCones, int32 ConeIndexToSkip, const FVector2D& P);
	
	static void ClassifySegments(const FVOConesSoA& VOCones, const FVOParams& Params, 
		const TArray<TArray<FVOConeIntersection>>& IntersectionsByRays,
		TArray<TArray<FVOOutsideSegment>>& OutsideSegmentsByRays);

	static FVector2D SelectBestVelocityFromOutsideSegments(
		const FVector2D& DesiredVel2D,
		const FVector2D& CurVel2D,
		const TArray<FVONeighborView>& Neis,
		const FVector& ActorPos,
		const FVOParams& Params,
		const TArray<TArray<FVOOutsideSegment>>& OutsideSegmentsByRays,
		TArray<FVector2D>& Debug_LastCandidates,
		int32& Debug_BestCandidateIdx
	);

	static FVector ComputeVelocity(const UVOFollowingComponent* Comp, 
		const FVector& CurVel, 
		const FVector& DesiredVel, 
		const TArray<FVONeighborView>& Neis, 
		const FVOParams& Params);
};