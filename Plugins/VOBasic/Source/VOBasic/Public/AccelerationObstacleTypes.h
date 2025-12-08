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
};

struct FAOSide
{
	TArray<FAOSegment> Segments;
};

struct FAOCone
{
	FVector2D   TimeHorizonNormal;
	float		TimeHorizonOffset;
		
	FAOSide		LeftSide;
	FAOSide		RightSide;

	bool		isLeftSideValid;
	bool		isRightSideValid;
	bool		isTHSegmentValid;

	TArray<FAOTriangle> Tris;
	TArray<FAOQuad>		Quads;
};

struct FAOConesSoA
{
	TArray<FVector2D>	TimeHorizonSegment;
	TArray<FVector2D>	TimeHorizonNormal;

	TArray<FAOSide>		LeftSide;
	TArray<FAOSide>		RightSide;

	TArray<bool>		isLeftSideValid;
	TArray<bool>		isRightSideValid;
	TArray<bool>		isTHSegmentValid;

	TArray<TArray<FAOTriangle>> Tris;
	TArray<TArray<FAOQuad>>		Quads;

	void Reset()
	{
		//TimeHorizonSegment.Reset();
		TimeHorizonNormal.Reset();

		LeftSide.Reset();
		RightSide.Reset();

		isLeftSideValid.Reset();
		isRightSideValid.Reset();
		isTHSegmentValid.Reset();

		Tris.Reset();
		Quads.Reset();
	}

	void Reserve(int32 Num)
	{
		//TimeHorizonSegment.Reserve(Num);
		TimeHorizonNormal.Reserve(Num);

		LeftSide.Reserve(Num);
		RightSide.Reserve(Num);

		isLeftSideValid.Reserve(Num);
		isRightSideValid.Reserve(Num);
		isTHSegmentValid.Reserve(Num);

		Tris.Reserve(Num);
		Quads.Reserve(Num);
	}

	void Add(const FAOCone& Cone)
	{
		//TimeHorizonSegment.Add(Cone.TimeHorizonSegment);
		TimeHorizonNormal.Add(Cone.TimeHorizonNormal);

		LeftSide.Add(Cone.LeftSide);
		RightSide.Add(Cone.RightSide);

		isLeftSideValid.Add(Cone.isLeftSideValid);
		isRightSideValid.Add(Cone.isRightSideValid);
		isTHSegmentValid.Add(Cone.isTHSegmentValid);

		Tris.Add(Cone.Tris);
		Quads.Add(Cone.Quads);
	}

	int32 Num() const { return isLeftSideValid.Num(); }
};

struct FAOConeIntersection
{
	FVector2D P;
	bool bIsFirst;
	float t;
};
