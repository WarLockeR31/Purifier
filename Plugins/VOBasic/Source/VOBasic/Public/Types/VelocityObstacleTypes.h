#pragma once

class UVOFollowingComponent;
struct FVOOutsideSegment;
struct FVOConeIntersection;
struct FVOConesSoA;
struct FVONeighborView;
struct FVOParams;

struct FVOCalculationContext
{
	// Input
	const UVOFollowingComponent* Comp = nullptr;
	FVector ActorPos = FVector::ZeroVector;
	FVector CurrentVelocity = FVector::ZeroVector;
	FVector DesiredVelocity = FVector::ZeroVector;
    
	const FVOParams* Params = nullptr;
	const TArray<FVONeighborView>* Neis = nullptr;

	// Manager Buffers
	FVOConesSoA* Cones = nullptr;
	TArray<TArray<FVOConeIntersection>>* Intersections = nullptr;
	TArray<TArray<FVOOutsideSegment>>* OutsideSegments = nullptr;

	// Debug Output
	TArray<FVector2D>* OutCandidates = nullptr;
	int32* OutBestCandidateIdx = nullptr;
};

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