#pragma once

struct FAOSegment;
struct FAOConeIntersection;
struct FAOWorkSegment;
struct FAOConesSoA;
struct FVONeighborView;
struct FVOParams;
class UVOFollowingComponent;

struct FAOCalculationContext
{
	// Input
	const UVOFollowingComponent* Comp = nullptr;
	FVector ActorPos = FVector::ZeroVector;
	FVector CurrentVelocity = FVector::ZeroVector;
	FVector TargetPos = FVector::ZeroVector;
    
	const FVOParams* Params = nullptr;
	const TArray<FVONeighborView>* Neis = nullptr;

	// Manager Buffers
	FAOConesSoA* Cones = nullptr;
	TArray<FAOWorkSegment>* WorkSegments = nullptr;
	TArray<TArray<FAOConeIntersection>>* SideIntersections = nullptr;
	TArray<TArray<FAOSegment>>* OutsideSegments = nullptr;
    
	// Debug Output
	TArray<FVector2D>* OutCandidates = nullptr;
	int32* OutBestCandidateIdx = nullptr;
};

struct FAOTriangle
{
	FVector2D P1;
	FVector2D P2;
	FVector2D P3;
};

struct FAOQuad
{
	FVector2D P1;
	FVector2D P2;
	FVector2D P3;
	FVector2D P4;
};

struct FAOSegment
{
	FVector2D P1;
	FVector2D P2;
	FVector2D OutsideNormal;
	
	float MinX, MaxX;
	float MinY, MaxY;

	// TODO: Constructor
	void Init(const FVector2D& InP1, const FVector2D& InP2, const FVector2D& InNormal)
	{
		P1 = InP1;
		P2 = InP2;
		OutsideNormal = InNormal;
		
		if (P1.X < P2.X) { MinX = P1.X; MaxX = P2.X; } else { MinX = P2.X; MaxX = P1.X; }
		if (P1.Y < P2.Y) { MinY = P1.Y; MaxY = P2.Y; } else { MinY = P2.Y; MaxY = P1.Y; }
	}
};

struct FAOWorkSegment
{
	int32 ConeIdx;
	int32 SideIdx; 
	int32 SegIdx;  
	
	const FAOSegment* SegmentRef; // TODO: Reference instead of pointer?
};

struct FAOSide
{
	TArray<FAOSegment> Segments;
};

struct FAOCone
{		
	FAOSide		LeftSide;
	FAOSide		RightSide;

	FAOSegment  TimeHorizonSegment;
	FAOSegment  MinTimeSegment;

	TArray<FAOTriangle> Tris;
	TArray<FAOQuad>		Quads;
};
struct FAOConesSoA
{
	TArray<FAOSegment> TimeHorizonSegment;
	TArray<FAOSegment> MinTimeSegment;

	TArray<FAOSide>		LeftSide;
	TArray<FAOSide>		RightSide;

	TArray<TArray<FAOTriangle>> Tris;
	TArray<TArray<FAOQuad>>		Quads;

	void Reset()
	{
		TimeHorizonSegment.Reset();
		MinTimeSegment.Reset();
		LeftSide.Reset();
		RightSide.Reset();
		Tris.Reset();
		Quads.Reset();
	}

	void Add(const FAOCone& Cone)
	{
		TimeHorizonSegment.Add(Cone.TimeHorizonSegment);
		MinTimeSegment.Add(Cone.MinTimeSegment);
		LeftSide.Add(Cone.LeftSide);
		RightSide.Add(Cone.RightSide);
		Tris.Add(Cone.Tris);
		Quads.Add(Cone.Quads);
	}

	void Reserve(int32 Num)
	{
		TimeHorizonSegment.Reserve(Num);
		MinTimeSegment.Reserve(Num);

		LeftSide.Reserve(Num);
		RightSide.Reserve(Num);

		Tris.Reserve(Num);
		Quads.Reserve(Num);
	}

	int32 Num() const { return TimeHorizonSegment.Num(); }
};

struct FAOConeIntersection
{
	FVector2D P;
	float t;          
	int32 SegmentIndex; 
	bool bIsEntry;
	bool bIsIntersection;
};

struct FAOParabola
{
	FVector2D A, B, C;
	// P(u) = A*u^2 + B*u + C
	FVector2D Eval(double u) const { return (A * u + B) * u + C; }
};

struct FParabolaResult
{
	bool bFound = false;
	double U = -1.0;
	FVector2D Point = FVector2D::ZeroVector;
};

struct FAOCircle
{
	FVector2D Center;
	double R;
	double RSq;
};
