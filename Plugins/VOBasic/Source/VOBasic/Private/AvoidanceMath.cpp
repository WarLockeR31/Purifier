#include "AvoidanceMath.h"

#include "AccelerationObstacleTypes.h"
#include "VelocityObstacleTypes.h"

bool AvoidanceMath::TryFindSubSegmentInCircle(
	const FVector2D& P1,
	const FVector2D& P2,
	const float Radius,
	FVOSegment* OutSegment
)
{
	// Early return when both points inside circle 
	float P1Squared = P1.SizeSquared();
	float P2Squared = P2.SizeSquared();
	float RR = Radius * Radius;

	if (P1Squared <= RR && P2Squared <= RR)
	{
		if (OutSegment)
		{
			OutSegment->P1 = P1;
			OutSegment->P2 = P2;
		}
		return true;
	}

	// Find t values for intersection points
	FVector2D Delta = P2 - P1;
	float a = Delta.SizeSquared();
	float b = 2.f * FVector2D::DotProduct(P1, Delta);
	float c = P1Squared - RR;

	float Discriminant = b * b - 4.f * a * c;

	// Graze case
	if (FMath::IsNearlyZero(Discriminant, KINDA_SMALL_NUMBER) || Discriminant < 0.f)
	{
		return false;
	}

	float DiscSqrt = FMath::Sqrt(Discriminant);
	float InvDenominator = 1.f / (2.f * a);
	float t1 = (-b - DiscSqrt) * InvDenominator;
	float t2 = (-b + DiscSqrt) * InvDenominator;

	if (t1 > 1.f || t2 < 0.f)
		return false;

	// Find intersection points
	FVector2D OutPoint1 = (t1 < 0) ? P1 : P1 + Delta * t1;
	FVector2D OutPoint2 = (t2 > 1) ? P2 : P1 + Delta * t2;
	if (OutSegment)
	{
		OutSegment->P1 = OutPoint1;
		OutSegment->P2 = OutPoint2;
	}
	return true;
}

bool AvoidanceMath::SegmentIntersection2D(
	const FVector2D& P1,
	const FVector2D& P2,
	const FVector2D& Q1,
	const FVector2D& Q2,
	FVector2D& OutIntersection)
{
	const FVector2D r = P2 - P1;
	const FVector2D s = Q2 - Q1;

	auto Cross = [](const FVector2D& a, const FVector2D& b)
	{
		return a.X * b.Y - a.Y * b.X;
	};

	const float rxs = Cross(r, s);
	const FVector2D q_p = Q1 - P1;
	const float qpxr = Cross(q_p, r);

	if (FMath::IsNearlyZero(rxs))
	{
		if (FMath::IsNearlyZero(qpxr))
		{
			return false; 
		}
		return false; 
	}

	const float t = Cross(q_p, s) / rxs;
	const float u = Cross(q_p, r) / rxs;

	if (t < 0.f || t > 1.f || u < 0.f || u > 1.f)
		return false;

	OutIntersection = P1 + t * r;
	return true;
}

// TODO: DRY
bool AvoidanceMath::SegmentIntersection2D(
	const FVector2D& P1,
	const FVector2D& P2,
	const FVector2D& Q1,
	const FVector2D& Q2,
	FVector2D& OutPoint,
	float& OutT)
{   
	FVector2D Delta1 = P2 - P1;
	FVector2D Delta2 = Q2 - Q1;
	
	float Denominator = Delta1.X * Delta2.Y - Delta1.Y * Delta2.X;

	// Ignore grazing cases
	if (FMath::IsNearlyZero(Denominator, KINDA_SMALL_NUMBER))
	{
		return false;
	}

	float InvDenominator = 1.f / Denominator;
	FVector2D S1ToS2 = Q1 - P1;
	float t = (S1ToS2.X * Delta2.Y - S1ToS2.Y * Delta2.X) * InvDenominator;
	float u = (S1ToS2.X * Delta1.Y - S1ToS2.Y * Delta1.X) * InvDenominator;

	// TODO: Think about edge case:
	// If S1 (or S2) is degenerate (A == B) and the point lies on the other segment,
	//    the function returns true and sets OutT = 0 (i.e., at S1.A).
	
	// Intersection outside segments
	if (t < 0.f || t > 1.f || u < 0.f || u > 1.f)
		return false;

	// Find OutPoint and OutT
	OutPoint = P1 + t * Delta1;
	
	OutT = t;

	return true;
}

int32 AvoidanceMath::SolveQuadratic(double A, double B, double C, double& OutX1, double& OutX2)
{
	if (FMath::IsNearlyZero(A, 1e-9))
	{
		if (FMath::IsNearlyZero(B, 1e-9)) return 0;
		OutX1 = -C / B;
		return 1;
	}

	double D = B * B - 4.0 * A * C;
	if (D < 0.0) return 0;

	double SqrtD = FMath::Sqrt(D);
	double Inv2A = 0.5 / A;
	OutX1 = (-B - SqrtD) * Inv2A;
	OutX2 = (-B + SqrtD) * Inv2A;
	return 2;
}

bool AvoidanceMath::IsPointInTriangle(const FVector2D& P, const FVector2D& A, const FVector2D& B, const FVector2D& C)
{
	auto Sign = [](const FVector2D& p1, const FVector2D& p2, const FVector2D& p3)
	{
		return (p1.X - p3.X) * (p2.Y - p3.Y) - (p2.X - p3.X) * (p1.Y - p3.Y);
	};

	float d1 = Sign(P, A, B);
	float d2 = Sign(P, B, C);
	float d3 = Sign(P, C, A);

	bool has_neg = (d1 < 0) || (d2 < 0) || (d3 < 0);
	bool has_pos = (d1 > 0) || (d2 > 0) || (d3 > 0);

	return !(has_neg && has_pos);
}

bool AvoidanceMath::FindLineAndSegmentIntersection(
	const FVector2D& LineNormal, const float LineC,
	const FVector2D& SegmentP1, const FVector2D& SegmentP2,
	float& outT, FVector2D& outPoint
)
{
	const float signedDistStart = FVector2D::DotProduct(LineNormal, SegmentP1) + LineC; // s_THL1
	const float signedDistEnd = FVector2D::DotProduct(LineNormal, SegmentP2) + LineC; // s_THL2

	const float denom = signedDistStart - signedDistEnd;
	if (FMath::IsNearlyZero(denom))
		return false;

	const float segmentT = signedDistStart / denom; // t_THL

	outT = segmentT;
	outPoint = FMath::Lerp(SegmentP1, SegmentP2, segmentT);
	return segmentT >= 0.0f && segmentT <= 1.0f;
}

bool AvoidanceMath::TryFindSegmentOfRayInCircle(
	const FVector2D& Apex, const FVector2D& Normal, const float Offset,
	const FVector2D& Dir, const float Radius, FVOSegment* OutSegment
)
{
	const float S = Normal.X * Normal.X + Normal.Y * Normal.Y;
	const float SInv = 1.f / S;
	const float SInvSqrt = FMath::InvSqrt(S);
	const float RR = Radius * Radius;
	const float D = FMath::Abs(Offset) * SInvSqrt; // Distance from center of circle to line

	// Edge case?
	if (FMath::IsNearlyEqual(D, Radius, KINDA_SMALL_NUMBER) || D > Radius)
	{
		return false;
	}

	const FVector2D Q = FVector2D(Normal.Y, -Normal.X) * FMath::Sqrt(RR * S - Offset * Offset);
	const FVector2D P1 = (-Normal * Offset + Q) * SInv;
	const FVector2D P2 = (-Normal * Offset - Q) * SInv;

	const float t1 = FVector2D::DotProduct(P1 - Apex, Dir);
	const float t2 = FVector2D::DotProduct(P2 - Apex, Dir);

	// No intersections with ray
	if (t1 < 0.f && t2 < 0.f)
		return false;

	if (!OutSegment)
		return true;

	// 2 Intersections
	if (t1 > 0.f && t2 > 0.f)
	{
		if (t2 > t1)
		{
			OutSegment->P1 = P1;
			OutSegment->P2 = P2;
		}
		else
		{
			OutSegment->P1 = P2;
			OutSegment->P2 = P1;
		}

		return true;
	}

	// 1 Intersection
	{
		OutSegment->P1 = Apex;
		OutSegment->P2 = t1 >= 0.f ? P1 : P2; // TODO: CHECK
		return true;
	}
}

bool AvoidanceMath::FindCircleCircleIntersections(
	const FAOCircle& C1, const FAOCircle& C2, FVector2D& OutP1, FVector2D& OutP2
)
{
	FVector2D DVec = C2.Center - C1.Center;
	double DistSq = DVec.SizeSquared();
	double Dist = FMath::Sqrt(DistSq);

	// TODO: Optimize
	if (Dist > C1.R + C2.R || Dist < FMath::Abs(C1.R - C2.R) || Dist < 1e-9)
		return false;

	double A = (C1.RSq - C2.RSq + DistSq) / (2.0 * Dist);
	double H = FMath::Sqrt(FMath::Max(0.0, C1.RSq - A * A));

	double X2 = C1.Center.X + A * (C2.Center.X - C1.Center.X) / Dist;
	double Y2 = C1.Center.Y + A * (C2.Center.Y - C1.Center.Y) / Dist;

	double DX = H * (C2.Center.Y - C1.Center.Y) / Dist;
	double DY = H * (C2.Center.X - C1.Center.X) / Dist;

	OutP1 = FVector2D(X2 + DX, Y2 - DY);
	OutP2 = FVector2D(X2 - DX, Y2 + DY);
	return true;
}

