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
