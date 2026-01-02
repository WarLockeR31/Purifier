#pragma once

#include "CoreMinimal.h"
#include "AccelerationObstacleTypes.h"
#include "VelocityObstacleTypes.h"
#include "VOFollowingComponent.h"

struct FVONeighborView;
struct FAOConesSoA;
struct FAOSegment;
struct FAOSide;
struct FAOCone;
struct FAOConeIntersection;
struct FAOWorkSegment;

class VOBASIC_API AOUtility
{
public:
	static FVector ComputeBestAcceleration(
		const UVOFollowingComponent* Comp,
		const FVector& CurVel,
		const FVector& TargetPos,
		const TArray<FVONeighborView>& Neis,
		const FVOParams& Params,
		// External buffers (owned by Manager)
		FAOConesSoA& AOCones,
		TArray<FAOWorkSegment>& WorkSegments,
		TArray<TArray<FAOConeIntersection>>& SideIntersections,
		TArray<TArray<FAOSegment>>& OutsideSegments,
		TArray<FVector2D>& Candidates,
		int32& BestCandidateIdx
	);
	
	static void BuildAOCones(
		const TArray<FVONeighborView>& Neis,
		const FVector& ActorPos, const FVector& ActorVel,
		const FVOParams& Params,
		FAOConesSoA& OutVOCones
	);

	static void PrepareAndSortWorkSegments(
		const FAOConesSoA& InCones,
		TArray<FAOWorkSegment>& OutWorkSegments, int32& OutTotalSides
	);

	static void CollectIntersections(const TArray<FAOWorkSegment>& WorkSegments, TArray<TArray<FAOConeIntersection>>& OutSideIntersections);

	static void SortSideIntersections(TArray<TArray<FAOConeIntersection>>& SideIntersections);

	static void ClassifySegments(
		const FAOConesSoA& Cones,
		const TArray<TArray<FAOConeIntersection>>& SideIntersections,
		TArray<TArray<FAOSegment>>& OutOutsideSegments
	);

	static FParabolaResult FindParabolaIntersection(
		const FAOParabola& Curve,
		const FAOCircle& C1,
		const FAOCircle& C2,
		int32 DiscSegments = 16
	);

	static int32 SolveQuadratic(double A, double B, double C, double& OutX1, double& OutX2);
	
	static bool FindCircleCircleIntersections(
		const FAOCircle& C1,
		const FAOCircle& C2,
		FVector2D& OutP1,
		FVector2D& OutP2
	);

	/*static void ProcessCandidates(
		const FAOConesSoA& Cones,
		const TArray<TArray<FAOConeIntersection>>& SideIntersections,
		const FVector2D& DesiredAcc,
		TArray<FVector2D>& OutCandidates
	);*/

	/*static FVector2D SelectBestCandidate(
		const TArray<FVector2D>& Candidates,
		const FVector2D& DesiredAcc,
		const FVector2D& CurAcc,
		const TArray<FVONeighborView>& Neis,
		const FVector& ActorPos,
		const FVOParams& Params,
		int32& OutBestIdx
	);*/

	static FVector2D SelectBestAccelerationFromOutsideSegments(
	const FVector2D& DesiredAcc,
	const FVector2D& CurAcc,
	const TArray<FVONeighborView>& Neis,
	const FVector& ActorPos,
	const FVOParams& Params,
	const FAOConesSoA& Cones,
	const TArray<TArray<FAOSegment>>& OutsideSegmentsByRays,
	TArray<FVector2D>& OutCandidates,
	int32& OutBestIdx
);

	// Функция оценки (Score), которую я забыл в прошлый раз
	static float ScoreAccelerationCandidate(
		const FVector2D& CandidateAcc,
		const FVector2D& DesiredAcc,
		const FVector2D& CurAcc,
		const TArray<FVONeighborView>& Neis,
		const FVector& ActorPos,
		const FVOParams& Params
	);

	// Helpers
	static FAOCone ComputeAOCone(float R, const FVector2D& C, const FVector2D& Vel, const FVector2D& Acc, const FVOParams& Params);
	static bool IsPointInsideAnyAO(const FAOConesSoA& Cones, const FVector2D& P, int32 IgnoreConeIdx);
	static bool IsPointInsideAO(const FAOConesSoA& Cones, int32 ConeIdx, const FVector2D& P);
	static int32 CountAOsForPoint(const FAOConesSoA& Cones, int32 ConeIndexToSkip, const FVector2D& P);
	static bool IsPointInTriangle(const FVector2D& P, const FVector2D& A, const FVector2D& B, const FVector2D& C);

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

	enum class ERemoveInnerPointsResultType : uint8
	{
		None,
		SegmentIntersection,
		THIntersection,
		SinglePoint
	};
	struct RemoveSelfIntersectionsResult
	{
		ERemoveInnerPointsResultType ResultType = ERemoveInnerPointsResultType::None;
		int NumRemoved = 0;
		int FirstGoodPointInd = 0;
		int LastGoodPointInd = 0;
	};
	static bool TryFindSelfIntersections(
		const TArray<FVector2D>& Points, const TArray<FVector2D>& Normals,
		const FVOSegment& TimeHorizonSegment,
		RemoveSelfIntersectionsResult& OutResult);

	static bool SegmentIntersection2D(
		const FVector2D& P1, const FVector2D& P2,
		const FVector2D& Q1, const FVector2D& Q2,
		FVector2D& OutIntersection);

	static void DrawAOCones(
		const UVOFollowingComponent* Comp,
		const FAOConesSoA& Cones
	);

	static void DrawAOOutsideSegments(
		const UVOFollowingComponent* Comp, 
		const TArray<TArray<FAOSegment>>& OutsideSegments
	);
	
	/*static float SideOfSegment2D(const FVector2D& A, const FVector2D& B, const FVector2D& P)
	{
		const FVector2D AB = B - A;
		const FVector2D AP = P - A;

		return AB.X * AP.Y - AB.Y * AP.X;
	}*/

	static FColor GetColorFromSeed(int32 Seed)
	{
		uint32 Hash = Seed * 2654435761; 
    
		uint8 Hue = (Hash) & 0xFF; 

		return FLinearColor::MakeFromHSV8(Hue, 255, 255).ToFColor(true);
	}
};
