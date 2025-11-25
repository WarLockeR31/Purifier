#pragma once

#include "CoreMinimal.h"
#include "VelocityObstacleTypes.h"
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
	static void BuildAOCones(const TArray<FVONeighborView>& Neis, const FVector& ActorPos, const FVector& ActorVel,
		const FVOParams& Params, FAOConesSoA& OutVOCones);
	
	static FAOCone ComputeAOCone(const float R, const FVector2D& C, const FVector2D& Vel, const FVector2D& Acc,
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

	static bool FindLineAndSegmentIntersection(
		const FVector2D& LineNormal,
		const float LineC,
		const FVector2D& SegmentP1,
		const FVector2D& SegmentP2,
		float& OutT,
		FVector2D& OutPoint);

	static TArray<FVector2D> BuildConvexSide(
		const TArray<FVector2D>& Points,
		const TArray<FVector2D>& Normals);

	static bool TryFindSubSegmentInCircle(
		const FVector2D& P1, const FVector2D& P2,
		float Radius,
		FVOSegment* OutSegment);

	struct RemoveSelfIntersectionsResult
	{
		int NumRemoved = 0;
		int FirstGoodPointInd = 0;
		int LastGoodPointInd = 0;
		bool bIsIntersectsWithTH = false;
		bool bFoundIntersection = false;
	};
	static bool TryFindSelfIntersections(
		const TArray<FVector2D>& Points, const TArray<FVector2D>& Normals,
		const FVOSegment& TimeHorizonSegment, bool bIsRight,
		RemoveSelfIntersectionsResult& OutResult);

	static bool SegmentIntersection2D(
		const FVector2D& P1, const FVector2D& P2,
		const FVector2D& Q1, const FVector2D& Q2,
		FVector2D& OutIntersection);

	/*static float SideOfSegment2D(const FVector2D& A, const FVector2D& B, const FVector2D& P)
	{
		const FVector2D AB = B - A;
		const FVector2D AP = P - A;

		return AB.X * AP.Y - AB.Y * AP.X;
	}*/
};
