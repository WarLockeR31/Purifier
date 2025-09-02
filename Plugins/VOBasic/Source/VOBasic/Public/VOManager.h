#pragma once
#include "CoreMinimal.h"
#include "CrowdManagerBase.h"
#include "VOFollowingComponent.h"
#include "VOManager.generated.h"

USTRUCT()
struct FVONeighborView
{
	GENERATED_BODY()
	FVector Pos = FVector::ZeroVector;   // world XY
	FVector Vel = FVector::ZeroVector;   // world XY
	float Radius = 34.f;                 // cm
};

struct FVOSegment
{
	FVector2D P1;
	FVector2D P2;
};

// Truncated VO
USTRUCT(BlueprintType)
struct FVOCone
{
	GENERATED_BODY()
	FVector2D 	Apex;
		
	FVector2D 	LeftRayApex;  // Maybe unneeded
	FVector2D	LeftRayDir;	  // Maybe unneeded
	FVector2D 	LeftRayNormal;
	float     	LeftRayOffset;
		
	FVector2D 	RightRayApex; // Maybe unneeded
	FVector2D	RightRayDir;  // Maybe unneeded
	FVector2D 	RightRayNormal;
	float     	RightRayOffset;
	
	FVector2D	TimeHorizonNormal;
	float		TimeHorizonOffset;

	FVOSegment	LeftRaySegment;		// P1 - Closest to LeftRayApex
	FVOSegment	RightRaySegment;	// P1 - Closest to RightRayApex
	FVOSegment	TimeHorizonSegment;
	bool		bIsLeftRaySegmentValid;
	bool		bIsRightRaySegmentValid;
	bool		bIsTHSegmentValid;
};

struct FVOOutsideSegment
{
	FVector2D P1;
	FVector2D P2;
	FVector2D OutsideNormal;
};

struct FVOConeIntersection
{
	FVector2D P;
	bool bIsFirst;
	float t;
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

	TArray<FVOCone>						VO_Cones;
	TArray<TArray<FVOConeIntersection>> IntersectionsByRays;
	TArray<TArray<FVOOutsideSegment>>	OutsideSegmentsByRays;

	

	/// 
	/// @param S1 First segment
	/// @param S2 Secont segment
	/// @param OutPoint Point of intersections
	/// @param OutT Normalized parameter along S1
	/// @return 
	static bool TryFindIntersections(FVOSegment S1, FVOSegment S2, FVector2D* OutPoint, float* OutT);

	bool TryFindSegmentOfRayInCircle(const FVector2D& Apex, const FVector2D& Normal, const float Offset, const FVector2D& Dir,
									 const float Radius,
									 FVOSegment* OutSegment) const;

	bool TryFindSubSegmentInCircle(const FVector2D& P1, const FVector2D& P2, const float Radius, FVOSegment* OutSegment) const;

	bool IsVelocityForbidden(const FVector2D& CandidateVA, const TArray<FVONeighborView>& Neis, const FVector& ActorPos, const FVOParams& Params) const;
	void BuildVOCones(const TArray<FVONeighborView>& Neis, const FVector& ActorPos, const FVOParams& Params, TArray<FVOCone>& OutVOCones) const;
	void CollectIntersections(const TArray<FVOCone>& VOCones);
	void SortIntersectionsByRays(const TArray<FVOCone>& VOCones);
	int32 CountVOsForPoint(const TArray<FVOCone>& VOCones, int32 ConeIndexToSkip, const FVector2D& P) const;
	void ClassifySegments(const TArray<FVOCone>& VOCones, const FVOParams& Params);
	FVOCone ComputeVOCone(const float R, const FVector2D& C, const FVector2D& Vel, const FVOParams& Params) const;
	bool WillCollideWithinTau(const FVector2D& RelativePosition, const FVector2D& RelativeVelocity, float Radius, float TimeHorizon, float* OutTOI) const;
	void DrawVOCones(const UVOFollowingComponent* Comp, TArray<FVOCone>& Cone) const;
	void DrawCombinedVO(const UVOFollowingComponent* Comp) const;
	FVector ComputeVelocity(const UVOFollowingComponent* Comp, const FVector& CurVel, const FVector& DesiredVel, const TArray<FVONeighborView>& Neis, const FVOParams& Params);


	// Helpers
	void PrepareArrays(size_t NumNeis);
};