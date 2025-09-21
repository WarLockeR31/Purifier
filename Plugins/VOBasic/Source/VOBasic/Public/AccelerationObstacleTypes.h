#pragma once

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
	FVector2D 	Apex;

	FVector2D   TimeHorizonSegment;
	FVector2D   TimeHorizonNormal;
		
	FAOSide		LeftSide;
	FAOSide		RightSide;

	bool		isLeftSideValid;
	bool		isRightSideValid;
	bool		isTHSegmentValid;
};

struct FAOConesSoA
{
	TArray<FVector2D> Apex;

	TArray<FVector2D>	TimeHorizonSegment;
	TArray<FVector2D>	TimeHorizonNormal;

	TArray<FAOSide>		LeftSide;
	TArray<FAOSide>		RightSide;

	TArray<bool>		isLeftSideValid;
	TArray<bool>		isRightSideValid;
	TArray<bool>		isTHSegmentValid;

	void Reset()
	{
		Apex.Reset();

		TimeHorizonSegment.Reset();
		TimeHorizonNormal.Reset();

		LeftSide.Reset();
		RightSide.Reset();

		isLeftSideValid.Reset();
		isRightSideValid.Reset();
		isTHSegmentValid.Reset();
	}

	void Reserve(int32 Num)
	{
		Apex.Reserve(Num);

		TimeHorizonSegment.Reserve(Num);
		TimeHorizonNormal.Reserve(Num);

		LeftSide.Reserve(Num);
		RightSide.Reserve(Num);

		isLeftSideValid.Reserve(Num);
		isRightSideValid.Reserve(Num);
		isTHSegmentValid.Reserve(Num);
	}

	void Add(const FAOCone& Cone)
	{
		Apex.Add(Cone.Apex);
		
		TimeHorizonSegment.Add(Cone.TimeHorizonSegment);
		TimeHorizonNormal.Add(Cone.TimeHorizonNormal);

		LeftSide.Add(Cone.LeftSide);
		RightSide.Add(Cone.RightSide);

		isLeftSideValid.Add(Cone.isLeftSideValid);
		isRightSideValid.Add(Cone.isRightSideValid);
		isTHSegmentValid.Add(Cone.isTHSegmentValid);
	}

	int32 Num() const { return Apex.Num(); }
};

struct FAOConeIntersection
{
	FVector2D P;
	bool bIsFirst;
	float t;
};
