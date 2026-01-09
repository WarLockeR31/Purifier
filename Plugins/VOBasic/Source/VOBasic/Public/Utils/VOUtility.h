#pragma once

#include "CoreMinimal.h"
#include "Types/VelocityObstacleTypes.h"

class UVOFollowingComponent;

namespace VOUtility
{
	VOBASIC_API FVector ComputeVelocity(const FVOCalculationContext& Ctx);

	VOBASIC_API void DrawVOCones(const UVOFollowingComponent* Comp, const FVOConesSoA& Cones);
	
	VOBASIC_API void DrawCombinedVO(const UVOFollowingComponent* Comp, 
		const TArray<TArray<FVOOutsideSegment>>& OutsideSegmentsByRays);
	
	VOBASIC_API void DrawVelocityCandidates(const UVOFollowingComponent* Comp, 
		const TArray<FVector2D>& Candidates, int32 BestCandidateIdx,
		float PointSize, float LifeTime);
};