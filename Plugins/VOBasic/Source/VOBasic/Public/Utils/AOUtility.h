#pragma once

#include "CoreMinimal.h"
#include "Types/AccelerationObstacleTypes.h"
#include "Types/VelocityObstacleTypes.h"
#include "Components/VOFollowingComponent.h"

struct FVONeighborView;
struct FAOConesSoA;
struct FAOSegment;
struct FAOSide;
struct FAOCone;
struct FAOConeIntersection;
struct FAOWorkSegment;

namespace AOUtility
{
	VOBASIC_API FVector ComputeAcceleration(const FAOCalculationContext& Ctx);

	VOBASIC_API void DrawAOCones(const UVOFollowingComponent* Comp, const FAOConesSoA& Cones);

	VOBASIC_API void DrawAOOutsideSegments(const UVOFollowingComponent* Comp, const TArray<TArray<FAOSegment>>& OutsideSegments);
}