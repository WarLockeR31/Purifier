#pragma once
#include "CoreMinimal.h"
#include "CrowdManagerBase.h"
#include "VOFollowingComponent.h"
#include "UVOManager.generated.h"

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

	struct FVOConeIntersection { FVector2D P; bool bIsFirst; };

	static bool TryFindIntersections(const float A1, const float B1, const float C1,
		const float A2, const float B2, const float C2,
		FVector2D* OutPoint);

	static bool TryFindIntersections(const float A1, const float B1, const float C1, FVector2D apex1, bool isLeftRay1,
		const float A2, const float B2, const float C2, FVector2D apex2, bool isLeftRay2,
		FVector2D* OutPoint);

	static FVector2D LeftDirFromNormal(const FVector2D& N) { return FVector2D(-N.Y, N.X); }
	static FVector2D RightDirFromNormal(const FVector2D& N) { return FVector2D(N.Y, -N.X); }

	bool IsVelocityForbidden(const FVector2D& CandidateVA, const TArray<FVONeighborView>& Neis, const FVector& ActorPos, const FVOParams& Params) const;
	void BuildVOCones(const TArray<FVONeighborView>& Neis, const FVector& ActorPos, const FVOParams& Params, TArray<FVOCone>& OutVOCones) const;
	void CollectIntersections(const TArray<FVOCone>& VOCones, TArray<TArray<FVOConeIntersection>>& OutIntersectionsByRays) const;
	void SortIntersectionsByRays(const TArray<FVOCone>& VOCones, TArray<TArray<FVOConeIntersection>>& IntersectionsByRays) const;
	int32 CountVOsForPoint(const TArray<FVOCone>& VOCones, int32 ConeIndexToSkip, const FVector2D& P) const;
	void ClassifySegments(const TArray<FVOCone>& VOCones, const TArray<TArray<FVOConeIntersection>>& IntersectionsByRays, const FVOParams& Params, TArray<TArray<FVOOutsideSegment>>& OutOutsideSegmentsByRays) const;
	static void GetRayApexNormalOffset(const TArray<FVOCone>& VOCones, int32 RayIndex, FVector2D& OutApex, FVector2D& OutNormal, float& OutOffset);
	FVOCone ComputeVOCone(const float R, const FVector2D& C, const FVector2D& Vel, const FVOParams& Params) const;
	bool WillCollideWithinTau(const FVector2D& RelativePosition, const FVector2D& RelativeVelocity, float Radius, float TimeHorizon, float* OutTOI) const;
	void DrawVOCones(const UVOFollowingComponent* Comp, TArray<FVOCone>& Cone) const;
	void DrawCombinedVO(const UVOFollowingComponent* Comp, const TArray<TArray<FVOOutsideSegment>>& OutsideSegmentsByRays) const;
	FVector ComputeVelocity(const UVOFollowingComponent* Comp, const FVector& CurVel, const FVector& DesiredVel, const TArray<FVONeighborView>& Neis, const FVOParams& Params) const;
};