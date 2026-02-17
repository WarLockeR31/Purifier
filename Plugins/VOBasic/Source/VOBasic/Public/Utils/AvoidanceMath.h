#pragma once

struct FAOCircle;
struct FVOSegment;

namespace AvoidanceMath
{
	VOBASIC_API bool SegmentIntersection2D(
		const FVector2D& P1,
		const FVector2D& P2,
		const FVector2D& Q1,
		const FVector2D& Q2,
		FVector2D& OutIntersection
	);

	VOBASIC_API bool SegmentIntersection2D(
		const FVector2D& P1,
		const FVector2D& P2,
		const FVector2D& Q1,
		const FVector2D& Q2,
		FVector2D& OutPoint,
		float& OutT);

	VOBASIC_API int32 SolveQuadratic(double A, double B, double C, double& OutX1, double& OutX2);

	VOBASIC_API bool IsPointInTriangle(const FVector2D& P, const FVector2D& A, const FVector2D& B, const FVector2D& C);

	VOBASIC_API bool FindLineAndSegmentIntersection(
		const FVector2D& LineNormal,
		const float LineC,
		const FVector2D& SegmentP1,
		const FVector2D& SegmentP2,
		float& OutT,
		FVector2D& OutPoint);

	VOBASIC_API bool TryFindSubSegmentInCircle(
		const FVector2D& P1,
		const FVector2D& P2,
		float Radius,
		FVOSegment* OutSegment);

	VOBASIC_API bool TryFindSegmentOfRayInCircle(
		const FVector2D& Apex,
		const FVector2D& Normal,
		const float Offset,
		const FVector2D& Dir,
		const float Radius,
		FVOSegment* OutSegment);

#pragma region RayCasting
	VOBASIC_API bool FindRayCircleIntersection(
		const FVector2D& Center,
		const FVector2D& Vel,
		float Radius,
		float Tolerance,
		float& OutTime);

	VOBASIC_API bool FindRayCapsuleIntersection(
		const FVector2D& S1,     
		const FVector2D& S2,     
		const FVector2D& Vel,    
		float Radius,
		float Tolerance,
		float& OutTime);
#pragma endregion 

	VOBASIC_API bool FindCircleCircleIntersections(
		const FAOCircle& C1,
		const FAOCircle& C2,
		FVector2D& OutP1,
		FVector2D& OutP2);

	VOBASIC_API bool TryFindCircleTangents(
		const FVector2D& Source,
		const FVector2D& C,
		float R,
		float InvSqrT,
		FVector2D& PointL,
		FVector2D& PointR,
		FVector2D& NormalL,
		FVector2D& NormalR);
	VOBASIC_API bool TryFindCapsuleTangents(
		const FVector2D& Source,
		const FVector2D& C1,
		const FVector2D& C2,
		const FVector2D& C,
		float R,
		float InvSqrT,
		FVector2D& PointL,
		FVector2D& PointR,
		FVector2D& NormalL,
		FVector2D& NormalR);
	
	VOBASIC_API inline FColor GetColorFromSeed(int32 Seed)
	{
		uint32 Hash = Seed * 2654435761;
		uint8 Hue = (Hash) & 0xFF;
		return FLinearColor::MakeFromHSV8(Hue, 255, 255).ToFColor(true);
	}
}
