#include "AOUtility.h"
#include "VOManager.h"
#include "VOSettings.h"

void AOUtility::BuildAOCones(const TArray<FVONeighborView>& Neis, const FVector& ActorPos, const FVector& ActorVel, const FVOParams& Params,
                             FAOConesSoA& OutVOCones)
{
	for (int32 i = 0; i < Neis.Num(); ++i)
	{
		const FVONeighborView& N = Neis[i];
		const float R = Params.AgentRadius + N.Radius;
		const FVector2D pRel(N.Pos.X - ActorPos.X, N.Pos.Y - ActorPos.Y);
		if (R * R >= pRel.SizeSquared())
		{
			continue;
		}
		const FVector2D vRel(ActorVel.X - N.Vel.X, ActorVel.Y - N.Vel.Y);
		OutVOCones.Add(ComputeAOCone(R, pRel, vRel, FVector2D(N.Acc), Params));
	}
}

FAOCone AOUtility::ComputeAOCone(const float R, const FVector2D& C, const FVector2D& Vel, const FVector2D& Acc, const FVOParams& Params)
{
	FAOCone Cone;

	const UVOSettings* Settings = UVOSettings::Get();

	TArray<FVector2D> PointsL, PointsR;
	TArray<FVector2D> NormalsL, NormalsR;
	bool isConvexL = false, isConvexR = false;
	
	float t = Settings->MinimalReactionTime;
	float t_interval = (Params.TauHorizon - Settings->MinimalReactionTime) / Settings->NDiscreteIntervals;
	float t_last = Params.TauHorizon;
	for (int i = 0; i > Settings->NDiscreteIntervals; ++i)
	{
		float InvT = 1.f / t;
		float InvSqrT = InvT * InvT;
		
		FVector2D CenterLineP = 2 * C * InvSqrT - 2 * Vel * InvT + Acc;			//S_AOc
		FVector2D GrazeSourceP = - Vel * InvT + Acc;							//S_AOGrazeSource
		FVector2D GrazeToCenterOffset = GrazeSourceP - CenterLineP;				//v_H
		float GrazeToCenterLengthSqr = GrazeToCenterOffset.SizeSquared();		//d_sqr
		float RTimed = 2 * R / (t * t);
		float DistanceSqr = GrazeToCenterLengthSqr - RTimed * RTimed;
		if (DistanceSqr < 0.f)
		{
			t_last = t;
			break;
		}
		
		float RTimedOverGrazeToCenterLenSqr = RTimed / GrazeToCenterLengthSqr;
		float k_h = RTimedOverGrazeToCenterLenSqr * FMath::Sqrt(DistanceSqr);
		FVector2D b = CenterLineP + RTimed * RTimedOverGrazeToCenterLenSqr * GrazeToCenterOffset;
		FVector2D c = k_h * FVector2D(GrazeToCenterOffset.Y, -GrazeToCenterOffset.X);
		FVector2D PointL = b + c;
		FVector2D PointR = b - c;

		PointsL.Add(PointL);
		PointsR.Add(PointR);

		// Check if convex
		if (i == 0)
		{
			if (GrazeSourceP.X * PointL.Y - GrazeSourceP.Y * PointL.X >= 0.f)
				isConvexL = true;
			if (GrazeSourceP.X * PointL.Y - GrazeSourceP.Y * PointL.X <= 0.f)
				isConvexR = true;
		}

		// Calculate normals in points if not convex (not normalized)
		FVector2D DirGraze = PointL - GrazeSourceP;
		if (!isConvexL)
			NormalsL.Add({DirGraze.Y, -DirGraze.X});
		if (!isConvexR)
			NormalsR.Add({-DirGraze.Y, DirGraze.X});
		
		t += t_interval;
	}

	
	// Calculate TimeHorizon Points
	float InvT = 1.f / t;
	float InvSqrT = InvT * InvT;
		
	FVector2D CenterLineP = 2 * C * InvSqrT - 2 * Vel * InvT + Acc;			//S_AOc
	FVector2D GrazeSourceP = - Vel * InvT + Acc;							//S_AOGrazeSource
	FVector2D GrazeToCenterOffset = GrazeSourceP - CenterLineP;				//v_H
	float GrazeToCenterLengthSqr = GrazeToCenterOffset.SizeSquared();		//d_sqr
	float RTimed = 2 * R / (t * t);
	
	FVector2D 	FirstL = PointsL.Last();
	float		RTimedOverGrazeToCenterLen = RTimed / FMath::Sqrt(GrazeToCenterLengthSqr);	//d_ratio
	FVector2D 	GrazeOffsetFromCenter = RTimedOverGrazeToCenterLen * GrazeToCenterOffset;
	FVector2D 	TimeHorizonGrazePoint = CenterLineP + GrazeOffsetFromCenter;				//P_TH
	FVector2D 	TimeHorizonGrazeNormal = -GrazeToCenterOffset.GetSafeNormal();				//n_TH
	float		TimeHorizonC = -TimeHorizonGrazeNormal.Dot(TimeHorizonGrazePoint);

	float outT = 0.f;
	FVector2D TimeHorizonL = FVector2D::ZeroVector;
	if (FindLineAndSegmentIntersection(TimeHorizonGrazeNormal, TimeHorizonC, GrazeSourceP, FirstL, outT, TimeHorizonL))
	{
		PointsL.Add(TimeHorizonL);
		NormalsL.Add(NormalsL.Last());
	}

	FVector2D TimeHorizonR = TimeHorizonL + 2 * (TimeHorizonGrazePoint - TimeHorizonL);
	PointsR.Add(TimeHorizonR);
	NormalsR.Add(NormalsR.Last());
	
	Cone.TimeHorizonNormal = TimeHorizonGrazeNormal;
	Cone.TimeHorizonOffset = TimeHorizonC;


	// Build convex points
	if (!isConvexL)
		PointsL = BuildConvexSide(PointsL, NormalsL);
	if (!isConvexR)
		PointsR = BuildConvexSide(PointsR, NormalsR);

	//TODO: Add self-intersection check
	if (isConvexL)
	{
		
	}
	if (isConvexR)
	{
		
	}
	
	// TODO: Full segments sides and circle validation
	FVector2D PrevPoint = PointsL.Last();
	for (int i = PointsL.Num() - 2; i >= 0; --i)
	{
		FVOSegment Segment;
		if (TryFindSubSegmentInCircle(PrevPoint, PointsL[i], Params.MaxAcceleration, &Segment))
		{
			Cone.LeftSide.Segments.Add({Segment.P1, Segment.P2 /* TODO: Add normal */});
		}
		PrevPoint = PointsL[i];
	}

	PrevPoint = PointsR.Last();
	for (int i = PointsR.Num() - 2; i >= 0; --i)
	{
		FVOSegment Segment;
		if (TryFindSubSegmentInCircle(PrevPoint, PointsR[i], Params.MaxAcceleration, &Segment))
		{
			Cone.RightSide.Segments.Add({Segment.P1, Segment.P2 /* TODO: Add normal */});
		}
		PrevPoint = PointsR[i];
	}

	// TODO: We need all segments for inside check

	return Cone;
}

bool AOUtility::FindLineAndSegmentIntersection(const FVector2D& LineNormal, const float LineC,
	const FVector2D& SegmentP1, const FVector2D& SegmentP2,
	float& outT, FVector2D& outPoint)
{
	const float signedDistStart = FVector2D::DotProduct(LineNormal, SegmentP1);	// s_THL1
	const float signedDistEnd   = FVector2D::DotProduct(LineNormal, SegmentP2);   // s_THL2

	const float denom = signedDistStart - signedDistEnd;
	if (FMath::IsNearlyZero(denom))
		return false;

	const float segmentT = signedDistStart / denom;										// t_THL

	outT = segmentT;
	outPoint = FMath::Lerp(SegmentP1, SegmentP2, segmentT);
	return segmentT >= 0.0f && segmentT <= 1.0f;		
}

TArray<FVector2D> AOUtility::BuildConvexSide(const TArray<FVector2D>& Points, const TArray<FVector2D>& Normals)
{
	TArray<FVector2D> ConvexSide;
	ConvexSide.Add(Points[0]);
	
	float LinePrevC = -FVector2D::DotProduct(Normals[0], Points[0]);
	for (int i = 1; i < Points.Num() - 1; ++i)
	{
		float LineC = -FVector2D::DotProduct(Normals[i], Points[i]);
		float DenomGraze = Normals[i - 1].X * Normals[i].Y - Normals[i - 1].Y * Normals[i].X;
		if (FMath::Abs(DenomGraze) < KINDA_SMALL_NUMBER)
		{
			ConvexSide.Add((Points[i] + Points[i - 1]) * 0.5f);
			LinePrevC = LineC;
			continue;
		}
		
		float DenomGrazeInv = 1.f / DenomGraze;
		float X = (Normals[i - 1].Y * LineC		- Normals[i].Y		* LinePrevC) * DenomGrazeInv;
		float Y = (Normals[i].X		* LinePrevC - Normals[i - 1].X	* LineC)	 * DenomGrazeInv;

		ConvexSide.Add(FVector2D(X, Y));
		LinePrevC = LineC;
	}

	ConvexSide.Add(Points.Last());
	return ConvexSide;
}

bool AOUtility::TryFindSubSegmentInCircle(const FVector2D& P1, const FVector2D& P2, const float Radius,
	FVOSegment* OutSegment)
{
	// Early return when both points inside circle 
	float P1Squared = P1.SizeSquared();
	float P2Squared = P2.SizeSquared();
	float RR		= Radius * Radius;

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

bool AOUtility::TryFindSelfIntersections(
	const TArray<FVector2D>& Points, const TArray<FVector2D>& Normals,
	const FVOSegment& TimeHorizonSegment, bool bIsRight,
	RemoveSelfIntersectionsResult& OutResult)
{
	RemoveSelfIntersectionsResult PossibleResult;
	bool bFoundPossibleResult = false;

	int N = Points.Num();
	for (int i = 2; i < N; ++i)
	{
		FVector2D IntersectionPoint;
		
		// Intersection with TH
		if (SegmentIntersection2D(
			TimeHorizonSegment.P1, TimeHorizonSegment.P2,
			Points[i-1], Points[i],
			IntersectionPoint))
		{
			OutResult = { i - 1, -1, i, true, true };
			return true;
		}

		// Intersection with previous segments
		for (int j = 1; j < i - 1; ++j)
		{
			if (SegmentIntersection2D(
				Points[j-1], Points[j],
				Points[i-1], Points[i],
				IntersectionPoint))
			{
				OutResult = { i - j - 1, i - 1, j, false, true };
				return true;
			}
		}

		if (!bFoundPossibleResult && i < N - 1 && FVector2D::DotProduct(Normals[i], Points[i+1] - Points[i]) > 0.f)
		{
			PossibleResult = { 1, i, i < N - 2 ? i + 2 : i, false, false };
			bFoundPossibleResult = true;
		}

		if (!bFoundPossibleResult && FVector2D::DotProduct(Normals[i], Points[i-1] - Points[i]) > 0.f)
		{
			PossibleResult = { 1, i - 2, i, false, false };
			bFoundPossibleResult = true;
		}
	}

	if (bFoundPossibleResult)
	{
		OutResult = PossibleResult;
		return true;
	}
	
	return false;
}

bool SegmentIntersection2D(
	const FVector2D& P1, const FVector2D& P2,
	const FVector2D& Q1, const FVector2D& Q2,
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


