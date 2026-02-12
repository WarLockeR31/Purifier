#include "Utils/AvoidanceMath.h"

#include "Types/AccelerationObstacleTypes.h"
#include "Types/VelocityObstacleTypes.h"

bool AvoidanceMath::TryFindSubSegmentInCircle(
	const FVector2D& P1,
	const FVector2D& P2,
	const float Radius,
	FVOSegment* OutSegment)
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
	float& outT, FVector2D& outPoint)
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
	const FVector2D& Dir, const float Radius, FVOSegment* OutSegment)
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

bool AvoidanceMath::FindRayCircleIntersection(
	const FVector2D& Center,
	const FVector2D& Vel,
	float Radius,
	float Tolerance,
	float& OutTime)
{
	const double VelSq = Vel.SizeSquared();
	if (VelSq < KINDA_SMALL_NUMBER)
		return false;

	const double t_closest = FVector2D::DotProduct(Center, Vel) / VelSq;

	const FVector2D P_closest = Vel * t_closest;
	const float DistSq = FVector2D::DistSquared(P_closest, Center);
    
	if (DistSq > FMath::Square(Radius + Tolerance)) 
	{
		return false;
	}

	const float BackOffsetDist = FMath::Sqrt(FMath::Max(0.0f, FMath::Square(Radius) - DistSq));
	const float InvSpeed = FMath::InvSqrt(VelSq);
    
	OutTime = t_closest - (BackOffsetDist * InvSpeed);
    
	return true;
}

bool AvoidanceMath::FindRayCapsuleIntersection(
	const FVector2D& S1,
	const FVector2D& S2,
	const FVector2D& Vel,
	float Radius,
	float Tolerance,
	float& OutTime)
{
	const double VelSq = Vel.SizeSquared();
    if (VelSq < KINDA_SMALL_NUMBER)
    	return false;

    const FVector2D Edge = S2 - S1;
    const double EdgeLenSq = Edge.SizeSquared();
    
    float BestT = MAX_flt;
    bool bFound = false;

	float R = Radius + Tolerance;

    // Rectangle
    if (EdgeLenSq > KINDA_SMALL_NUMBER)
    {
        const FVector2D EdgeNormal(-Edge.Y, Edge.X);
        const double DistProj = -FVector2D::DotProduct(S1, EdgeNormal);
        const double VelProj = FVector2D::DotProduct(Vel, EdgeNormal);

        // Parallel check
        if (FMath::Abs(VelProj) > KINDA_SMALL_NUMBER)
        {
            // Take the closest 'wall'
            const float Sign = (DistProj > 0) ? 1.f : -1.f;
            const float TargetDist = Sign * R * FMath::Sqrt(EdgeNormal.SizeSquared());
            
            // Time of intersection
            const float t_edge = (TargetDist - DistProj) / VelProj;

            // Segment check
            const FVector2D HitPos = Vel * t_edge;
            const float t_proj = FVector2D::DotProduct(HitPos - S1, Edge) / EdgeLenSq;
            
            if (t_proj >= 0.f && t_proj <= 1.f)
            {
                BestT = t_edge;
                bFound = true;
            }
        }
    }

    // Corners (spheres)
    auto CheckVertex = [&](const FVector2D& Vert)
    {
        float B = -2.f * FVector2D::DotProduct(Vel, Vert);
        float C_Val = Vert.SizeSquared() - FMath::Square(R);
        float Discr = FMath::Square(B) - 4.f * VelSq * C_Val;

        if (Discr >= 0.f)
        {
            float t = (-B - FMath::Sqrt(Discr)) / (2.f * VelSq);
            
            if (t < BestT)
            {
                BestT = t;
                bFound = true;
            }
        }
    };

    CheckVertex(S1);
    CheckVertex(S2);

    if (bFound)
    {
        OutTime = BestT;
        return true;
    }

    return false;
}

bool AvoidanceMath::FindCircleCircleIntersections(
	const FAOCircle& C1, const FAOCircle& C2, FVector2D& OutP1, FVector2D& OutP2)
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

bool AvoidanceMath::TryFindCircleTangents(
	const FVector2D& Source,
	const FVector2D& C,
	const float R,
	const float InvSqrT,
	FVector2D& PointL,
	FVector2D& PointR,
	FVector2D& NormalL,
	FVector2D& NormalR)
{
	const FVector2D GrazeToCenterOffset = Source - C;

	const float GrazeToCenterLengthSqr = GrazeToCenterOffset.SizeSquared();
	const float RTimed = 2 * R * InvSqrT;
	const float DistanceSqr = GrazeToCenterLengthSqr - RTimed * RTimed;

	if (DistanceSqr <= 0.f)
	{
		return false;
	}

	const float InvGrazeLenSqr = 1.f / GrazeToCenterLengthSqr;
	const float RTimedOverGrazeToCenterLenSqr = RTimed * InvGrazeLenSqr;

	const FVector2D b = C + (RTimed * RTimedOverGrazeToCenterLenSqr) * GrazeToCenterOffset;
	const float k_h = RTimedOverGrazeToCenterLenSqr * FMath::Sqrt(DistanceSqr);
	const FVector2D c(-GrazeToCenterOffset.Y * k_h, GrazeToCenterOffset.X * k_h);

	PointL = b + c;
	PointR = b - c;

	const FVector2D DirGrazeL = PointL - Source;
	NormalL = {-DirGrazeL.Y, DirGrazeL.X};
	
	const FVector2D DirGrazeR = PointR - Source;
	NormalR = {DirGrazeR.Y, -DirGrazeR.X};

	return true;
}

bool AvoidanceMath::TryFindCapsuleTangents(
	const FVector2D& Source,
	const FVector2D& C1,
	const FVector2D& C2,
	const FVector2D& C,
	const float R,
	const float InvSqrT,
	FVector2D& PointL,
	FVector2D& PointR,
	FVector2D& NormalL,
	FVector2D& NormalR)
{
	// C1 & C2 = relative to C
	FVector2D C1Timed = C1 * 2 * InvSqrT + C;
	FVector2D C2Timed = C2 * 2 * InvSqrT + C;
	const float RTimed = 2 * R * InvSqrT;
	//const float RTimed = 2 * R * InvSqrT;

	// TODO: Remove?
	float DistSq = FVector2D::DistSquared(Source, FMath::ClosestPointOnSegment2D(Source, C1Timed, C2Timed));
	if (DistSq <= RTimed * RTimed)
		return false;
	
	FVector2D C1toC2 = C2Timed - C1Timed;
	FVector2D C1toC2Normalized = C1toC2.GetSafeNormal();
	FVector2D ROffset = FVector2D(C1toC2Normalized.Y, -C1toC2Normalized.X);
	FVector2D C1toSource = Source - C1Timed;
	float t = (C1toSource | C1toC2) / (C1toC2 | C1toC2);

	if (FMath::Abs(C1toSource | ROffset) < RTimed)
	{
		if (t <= 0.0f)
		{
			UE_LOG(LogTemp, Log, TEXT("1"));
			return TryFindCircleTangents(Source, C1Timed, R, InvSqrT, PointL, PointR, NormalL, NormalR);
		}

		if (t >= 1.0f)
		{
			UE_LOG(LogTemp, Log, TEXT("2"));
			return TryFindCircleTangents(Source, C2Timed, R, InvSqrT, PointL, PointR, NormalL, NormalR);
		}
	}
	
	{
		FVector2D L1, R1, NL1, NR1;
		FVector2D L2, R2, NL2, NR2;

		bool bFoundC1 = TryFindCircleTangents(Source, C1Timed, R, InvSqrT, L1, R1, NL1, NR1);
		bool bFoundC2 = TryFindCircleTangents(Source, C2Timed, R, InvSqrT, L2, R2, NL2, NR2);

		if (bFoundC1 && bFoundC2)
		{
			UE_LOG(LogTemp, Log, TEXT("3"));
			float Cross = C1toC2 ^ C1toSource;
          
			if (Cross > 0.0f)
			{
				PointL = L1; NormalL = NL1; // Берем левую от C1
				PointR = R2; NormalR = NR2; // Берем правую от C2
			}
			else
			{
				PointL = L2; NormalL = NL2; // Берем левую от C2
				PointR = R1; NormalR = NR1; // Берем правую от C1
			}

			return true;
		}

		UE_LOG(LogTemp, Log, TEXT("4"));
	}

	return false;
}
