#pragma once

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

	bool		isLeftSideValid;
	bool		isRightSideValid;
	bool		isTHSegmentValid;

	TArray<FAOTriangle> Tris;
	TArray<FAOQuad>		Quads;
};
struct FAOConesSoA
{
	TArray<FAOSegment> TimeHorizonSegment;

	TArray<FAOSide>		LeftSide;
	TArray<FAOSide>		RightSide;

	TArray<bool>		isLeftSideValid;
	TArray<bool>		isRightSideValid;
	TArray<bool>		isTHSegmentValid;

	TArray<TArray<FAOTriangle>> Tris;
	TArray<TArray<FAOQuad>>		Quads;

	void Reset()
	{
		TimeHorizonSegment.Reset(); 
		LeftSide.Reset();
		RightSide.Reset();
		isLeftSideValid.Reset();
		isRightSideValid.Reset();
		isTHSegmentValid.Reset();
		Tris.Reset();
		Quads.Reset();
	}

	void Add(const FAOCone& Cone)
	{
		TimeHorizonSegment.Add(Cone.TimeHorizonSegment); 
		LeftSide.Add(Cone.LeftSide);
		RightSide.Add(Cone.RightSide);
		isLeftSideValid.Add(Cone.isLeftSideValid);
		isRightSideValid.Add(Cone.isRightSideValid);
		isTHSegmentValid.Add(Cone.isTHSegmentValid);
		Tris.Add(Cone.Tris);
		Quads.Add(Cone.Quads);
	}

	void Reserve(int32 Num)
	{
		TimeHorizonSegment.Reserve(Num);

		LeftSide.Reserve(Num);
		RightSide.Reserve(Num);

		isLeftSideValid.Reserve(Num);
		isRightSideValid.Reserve(Num);
		isTHSegmentValid.Reserve(Num);

		Tris.Reserve(Num);
		Quads.Reserve(Num);
	}

	int32 Num() const { return isLeftSideValid.Num(); }
};

struct FAOConeIntersection
{
	FVector2D P;
	float t;          
	int32 SegmentIndex; 
	bool bIsEntry;
	bool bIsIntersection;
};
