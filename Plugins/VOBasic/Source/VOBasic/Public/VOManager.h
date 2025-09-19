#pragma once
#include "CoreMinimal.h"
#include "CrowdManagerBase.h"
#include "VOFollowingComponent.h"
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

#pragma region VO
struct FVOSegment
{
	FVector2D P1;
	FVector2D P2;
};

struct FVOCone
{
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
	bool		bIsLeftRaySegmentValid  = false;
	bool		bIsRightRaySegmentValid = false;
	bool		bIsTHSegmentValid       = false;
};

// Truncated VO
struct FVOConesSoA
{
	TArray<FVector2D> Apex;
	
	TArray<FVector2D> LeftRayApex;
	TArray<FVector2D> LeftRayDir;
	TArray<FVector2D> LeftRayNormal;
	TArray<float> LeftRayOffset;
	
	TArray<FVector2D> RightRayApex;
	TArray<FVector2D> RightRayDir;
	TArray<FVector2D> RightRayNormal;
	TArray<float> RightRayOffset;
	
	TArray<FVector2D> TimeHorizonNormal;
	TArray<float> TimeHorizonOffset;

	TArray<FVOSegment> LeftRaySegment;
	TArray<FVOSegment> RightRaySegment;
	TArray<FVOSegment> TimeHorizonSegment;
	TArray<bool> bIsLeftRaySegmentValid;
	TArray<bool> bIsRightRaySegmentValid;
	TArray<bool> bIsTHSegmentValid;

	void Reset()
	{
		Apex.Reset();
		LeftRayApex.Reset();
		LeftRayDir.Reset();
		LeftRayNormal.Reset();
		LeftRayOffset.Reset();
		RightRayApex.Reset();
		RightRayDir.Reset();
		RightRayNormal.Reset();
		RightRayOffset.Reset();
		TimeHorizonNormal.Reset();
		TimeHorizonOffset.Reset();
		LeftRaySegment.Reset();
		RightRaySegment.Reset();
		TimeHorizonSegment.Reset();
		bIsLeftRaySegmentValid.Reset();
		bIsRightRaySegmentValid.Reset();
		bIsTHSegmentValid.Reset();
	}

	void Reserve(int32 Num)
	{
		Apex.Reserve(Num);
		LeftRayApex.Reserve(Num);
		LeftRayDir.Reserve(Num);
		LeftRayNormal.Reserve(Num);
		LeftRayOffset.Reserve(Num);
		RightRayApex.Reserve(Num);
		RightRayDir.Reserve(Num);
		RightRayNormal.Reserve(Num);
		RightRayOffset.Reserve(Num);
		TimeHorizonNormal.Reserve(Num);
		TimeHorizonOffset.Reserve(Num);
		LeftRaySegment.Reserve(Num);
		RightRaySegment.Reserve(Num);
		TimeHorizonSegment.Reserve(Num);
		bIsLeftRaySegmentValid.Reserve(Num);
		bIsRightRaySegmentValid.Reserve(Num);
		bIsTHSegmentValid.Reserve(Num);
	}

	void Add(const FVOCone& Cone)
	{
		Apex.Add(Cone.Apex);
		LeftRayApex.Add(Cone.LeftRayApex);
		LeftRayDir.Add(Cone.LeftRayDir);
		LeftRayNormal.Add(Cone.LeftRayNormal);
		LeftRayOffset.Add(Cone.LeftRayOffset);
		RightRayApex.Add(Cone.RightRayApex);
		RightRayDir.Add(Cone.RightRayDir);
		RightRayNormal.Add(Cone.RightRayNormal);
		RightRayOffset.Add(Cone.RightRayOffset);
		TimeHorizonNormal.Add(Cone.TimeHorizonNormal);
		TimeHorizonOffset.Add(Cone.TimeHorizonOffset);
		LeftRaySegment.Add(Cone.LeftRaySegment);
		RightRaySegment.Add(Cone.RightRaySegment);
		TimeHorizonSegment.Add(Cone.TimeHorizonSegment);
		bIsLeftRaySegmentValid.Add(Cone.bIsLeftRaySegmentValid);
		bIsRightRaySegmentValid.Add(Cone.bIsRightRaySegmentValid);
		bIsTHSegmentValid.Add(Cone.bIsTHSegmentValid);
	}

	int32 Num() const { return Apex.Num(); }
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
#pragma endregion

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
	void BuildVOCones(const TArray<FVONeighborView>& Neis, const FVector& ActorPos, const FVOParams& Params, FVOConesSoA& OutVOCones) const;
	void CollectIntersections(const FVOConesSoA& VOCones);
	void SortIntersectionsByRays();
	int32 CountVOsForPoint(const FVOConesSoA& VOCones, int32 ConeIndexToSkip, const FVector2D& P) const;
	void ClassifySegments(const FVOConesSoA& VOCones, const FVOParams& Params);
	FVOCone ComputeVOCone(const float R, const FVector2D& C, const FVector2D& Vel, const FVOParams& Params) const;
	bool WillCollideWithinTau(const FVector2D& RelativePosition, const FVector2D& RelativeVelocity, float Radius, float TimeHorizon, float* OutTOI) const;

	FVector2D SelectBestVelocityFromOutsideSegments(
		const FVector2D& DesiredVel2D,
		const FVector2D& CurVel2D,
		const TArray<FVONeighborView>& Neis,
		const FVector& ActorPos,
		const FVOParams& Params
	) const;

	float ScoreVelocityCandidate(
		const FVector2D& CandidateV,
		const FVector2D& DesiredVel2D,
		const FVector2D& CurVel2D,
		const TArray<FVONeighborView>& Neis,
		const FVector& ActorPos,
		const FVOParams& Params
	) const;

	
	FVector ComputeVelocity(const UVOFollowingComponent* Comp, const FVector& CurVel, const FVector& DesiredVel, const TArray<FVONeighborView>& Neis, const FVOParams& Params);

	void DrawVOCones(const UVOFollowingComponent* Comp, const FVOConesSoA& VOCones) const;
	void DrawCombinedVO(const UVOFollowingComponent* Comp) const;
	void DrawVelocityCandidates(
		const UVOFollowingComponent* Comp,
		float PointSize = 10.f,
		float LifeTime = 15.f
	) const;

	mutable TArray<FVector2D> Debug_LastCandidates;
	mutable int32 Debug_BestCandidateIdx = -1;

	// Helpers
	void PrepareArrays(size_t NumNeis);
};