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
	for (int i = 0; i < Settings->NDiscreteIntervals; ++i)
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
			//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("Neg sqr dist"));
			t_last = t;
			break;
		}
		
		float RTimedOverGrazeToCenterLenSqr = RTimed / GrazeToCenterLengthSqr;
		float k_h = RTimedOverGrazeToCenterLenSqr * FMath::Sqrt(DistanceSqr);
		FVector2D b = CenterLineP + RTimed * RTimedOverGrazeToCenterLenSqr * GrazeToCenterOffset;
		FVector2D c = k_h * FVector2D(-GrazeToCenterOffset.Y, GrazeToCenterOffset.X);
		FVector2D PointL = b + c;
		FVector2D PointR = b - c;

		PointsL.Add(PointL);
		PointsR.Add(PointR);

		// Check if convex
		if (i == 0)
		{
			FString Msg = FString::Printf(TEXT("L: %.2f | R: %.2f"), GrazeSourceP.X * PointL.Y - GrazeSourceP.Y * PointL.X, GrazeSourceP.X * PointR.Y - GrazeSourceP.Y * PointR.X);
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, Msg);
			
			if (GrazeSourceP.X * PointL.Y - GrazeSourceP.Y * PointL.X >= 0.f)
				isConvexL = true;
			if (GrazeSourceP.X * PointR.Y - GrazeSourceP.Y * PointR.X <= 0.f)
				isConvexR = true;

			/*Msg = FString::Printf(TEXT("X: %.2f | Y: %.2f"), GrazeSourceP.X, GrazeSourceP.Y);
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Blue, Msg);*/
		}

		// Calculate normals 
		FVector2D DirGraze = PointL - GrazeSourceP;
		NormalsL.Add({-DirGraze.Y, DirGraze.X});
		DirGraze = PointR - GrazeSourceP;
		NormalsR.Add({DirGraze.Y, -DirGraze.X});
		
		t += t_interval;
	}

	t -= t_interval;
	if (PointsL.Num() == 0)
		return Cone;
	
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
	FVector2D 	TimeHorizonGrazeNormal = -GrazeOffsetFromCenter.GetSafeNormal();			//n_TH
	float		TimeHorizonC = -TimeHorizonGrazeNormal.Dot(TimeHorizonGrazePoint);

	float outT = 0.f;
	FVector2D TimeHorizonL = FVector2D::ZeroVector;
	FVector2D TimeHorizonR  = FVector2D::ZeroVector;
	if (FindLineAndSegmentIntersection(TimeHorizonGrazeNormal, TimeHorizonC, GrazeSourceP, FirstL, outT, TimeHorizonL))
	{
		//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("THL"));
		PointsL.Add(TimeHorizonL);
		const FVector2D LastNormL = NormalsL.Last();
		NormalsL.Add(LastNormL);

		TimeHorizonR = TimeHorizonL + 2 * (TimeHorizonGrazePoint - TimeHorizonL);
		PointsR.Add(TimeHorizonR);
		const FVector2D LastNormR = NormalsR.Last();
		NormalsR.Add(LastNormR);
	}

	
	
	Cone.TimeHorizonNormal = TimeHorizonGrazeNormal;
	Cone.TimeHorizonOffset = TimeHorizonC;


	// Build convex points
	if (isConvexL)
		PointsL = BuildConvexSide(PointsL, NormalsL);
	if (isConvexR)
		PointsR = BuildConvexSide(PointsR, NormalsR);

	/*FString Msg = FString::Printf(TEXT("PL: %d | NL: %d | LR: %d | NR: %d"), PointsL.Num(), NormalsL.Num(), PointsR.Num(), NormalsR.Num());
	GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Blue, Msg);*/

	FVOSegment TimeHorizonSegment = {TimeHorizonL, TimeHorizonR};
	int centerFanIndL = -1;
	int centerFanIndR = -1;
	
	if (FVector2D::DotProduct(Vel, C) > 0.f)
	{
		auto ProcessSideIntersections = [&](TArray<FVector2D>& Points, const TArray<FVector2D>& Normals, bool bIsLeft)
		{
			RemoveSelfIntersectionsResult Result;
			if (TryFindSelfIntersections(Points, Normals, TimeHorizonSegment, Result))
			{
				switch (Result.ResultType)
				{
				case ERemoveInnerPointsResultType::THIntersection:
					{
						//GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Red, TEXT("THI"));
						const FVector2D& NewPoint = Points[Result.LastGoodPointInd];
						if (bIsLeft)
							TimeHorizonSegment.P1 = NewPoint;
						else
							TimeHorizonSegment.P2 = NewPoint;

						Points.SetNum(Result.LastGoodPointInd + 1);
						if (bIsLeft)
							centerFanIndL = Result.LastGoodPointInd;
						else
							centerFanIndR = Result.LastGoodPointInd;
						break;
					}
				case ERemoveInnerPointsResultType::SegmentIntersection:
					//GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Red, TEXT("SI"));
					Points.RemoveAt(Result.LastGoodPointInd + 1, Result.NumRemoved);
					if (bIsLeft)
						centerFanIndL = Result.LastGoodPointInd + 1;
					else
						centerFanIndR = Result.LastGoodPointInd + 1;
					break;
			
				case ERemoveInnerPointsResultType::SinglePoint:
					//GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Red, TEXT("SP"));
					Points.RemoveAt(Result.FirstGoodPointInd - 1);
					if (bIsLeft)
						centerFanIndL = Result.FirstGoodPointInd;
					else
						centerFanIndR = Result.FirstGoodPointInd;
					break;

				default:
					break;
				}
			}
		};

		if (!isConvexL)
		{
			ProcessSideIntersections(PointsL, NormalsL, true);
		}
		
		if (!isConvexR)
		{
			ProcessSideIntersections(PointsR, NormalsR, false);
		}
	}
	
	// Dumb validation
	// TODO: Replace with VO-like validation
	bool isValid = false;
	FVector2D PrevPoint = PointsL.Last();
	for (int i = PointsL.Num() - 2; i >= 0; --i)
	{
		FVOSegment Segment;
		if (TryFindSubSegmentInCircle(PrevPoint, PointsL[i], Params.MaxAcceleration, &Segment))
		{
			isValid = true;
			break;
		}
		PrevPoint = PointsL[i];
	}

	if (!isValid)
	{
		PrevPoint = PointsR.Last();
		for (int i = PointsR.Num() - 2; i >= 0; --i)
		{
			FVOSegment Segment;
			if (TryFindSubSegmentInCircle(PrevPoint, PointsR[i], Params.MaxAcceleration, &Segment))
			{
				isValid = true;
				break;
			}
			PrevPoint = PointsR[i];
		}
	}
	
	if (isValid)
	{
		Cone.isLeftSideValid = true;
		Cone.isRightSideValid = true;
		Cone.isTHSegmentValid = true;
	}
	
	auto ZipSides = [&](const TArray<FVector2D>& SideA, const TArray<FVector2D>& SideB, int32 FanIndexA)
	{
		int32 IdxB = SideB.Num() - 1;

		for (int32 IdxA = SideA.Num() - 2; IdxA >= 0; --IdxA)
		{
			const int32 TopA = IdxA + 1;
			const int32 BotA = IdxA;

			if (TopA == FanIndexA)
			{
				const int32 Diff = SideB.Num() - SideA.Num();
				if (Diff > 0)
				{
					for (int32 k = 0; k < Diff; ++k)
					{
						Cone.Tris.Add({ SideA[TopA], SideB[IdxB], SideB[IdxB - 1] });
						IdxB--; 
					}
				}
			}

			if (IdxB > 0) 
			{
				Cone.Quads.Add({ SideA[TopA], SideA[BotA], SideB[IdxB - 1], SideB[IdxB] });
				IdxB--;
			}
		}
	};

	if (!isConvexL && isConvexR)
	{
		ZipSides(PointsL, PointsR, centerFanIndL);
	}
	else
	
	if (!isConvexR && isConvexL)
	{
		ZipSides(PointsR, PointsL, centerFanIndR);
	}
	else
	{
		ZipSides(PointsR, PointsL, -1);
	}

	return Cone;
}

bool AOUtility::FindLineAndSegmentIntersection(const FVector2D& LineNormal, const float LineC,
	const FVector2D& SegmentP1, const FVector2D& SegmentP2,
	float& outT, FVector2D& outPoint)
{
	const float signedDistStart = FVector2D::DotProduct(LineNormal, SegmentP1) + LineC;	// s_THL1
	const float signedDistEnd   = FVector2D::DotProduct(LineNormal, SegmentP2) + LineC;   // s_THL2

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
	const FVOSegment& TimeHorizonSegment,
	RemoveSelfIntersectionsResult& OutResult)
{
	RemoveSelfIntersectionsResult PossibleResult;
	bool bFoundPossibleResult = false;

	int N = Points.Num();
	for (int i = N - 3; i >= 0; --i)
	{
		FVector2D IntersectionPoint;
		const int CurIdx = i;
		const int PrevIdx = i + 1;
		
		// Intersection with TH
		if (SegmentIntersection2D(
			TimeHorizonSegment.P1, TimeHorizonSegment.P2,
			Points[PrevIdx], Points[CurIdx],
			IntersectionPoint))
		{
			OutResult = { ERemoveInnerPointsResultType::THIntersection, N - PrevIdx, -1, CurIdx };
			return true;
		}

		// Intersection with previous segments
		for (int j = N - 2; j > PrevIdx; --j)
		{
			if (SegmentIntersection2D(
				Points[j+1], Points[j],
				Points[PrevIdx], Points[CurIdx],
				IntersectionPoint))
			{
				OutResult = { ERemoveInnerPointsResultType::SegmentIntersection, j - i, j + 1, i };
				return true;
			}
		}

		if (!bFoundPossibleResult && CurIdx > 0 && FVector2D::DotProduct(Normals[i], Points[i-1] - Points[i]) > 0.01f)
		{
			PossibleResult = { ERemoveInnerPointsResultType::SinglePoint, 1, i, CurIdx > 1 ? i - 2 : i };
			bFoundPossibleResult = true;
		}

		if (!bFoundPossibleResult && FVector2D::DotProduct(Normals[i], Points[i+1] - Points[i]) > 0.01f)
		{
			PossibleResult = { ERemoveInnerPointsResultType::SinglePoint, 1, i + 2, i };
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

bool AOUtility::SegmentIntersection2D(
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

void AOUtility::DrawAOCones(const UVOFollowingComponent* Comp, const FAOConesSoA& Cones)
{
	UWorld* W = Comp->GetWorld(); 
	if (!W) return;

	FVector P = Comp->GetOwnerLocation();

	for (int32 i = 0; i < Cones.Num(); ++i)
	{
		FColor Color = FColor::MakeRandomColor();

		const TArray<FAOTriangle>& ConeTris = Cones.Tris[i];
		for (const FAOTriangle& Tri : ConeTris)
		{
			FVector V1(Tri.P1.X, Tri.P1.Y, 0.f);
			FVector V2(Tri.P2.X, Tri.P2.Y, 0.f);
			FVector V3(Tri.P3.X, Tri.P3.Y, 0.f);

			DrawDebugLine(W, P + V1, P + V2, Color, true, 15.f, 0, 0.6f);
			DrawDebugLine(W, P + V2, P + V3, Color, true, 15.f, 0, 0.6f);
			DrawDebugLine(W, P + V3, P + V1, Color, true, 15.f, 0, 0.6f);
		}

		const TArray<FAOQuad>& ConeQuads = Cones.Quads[i];
		for (const FAOQuad& Quad : ConeQuads)
		{
			FVector V1(Quad.P1.X, Quad.P1.Y, 0.f);
			FVector V2(Quad.P2.X, Quad.P2.Y, 0.f);
			FVector V3(Quad.P3.X, Quad.P3.Y, 0.f);
			FVector V4(Quad.P4.X, Quad.P4.Y, 0.f);

			DrawDebugLine(W, P + V1, P + V2, Color, true, 15.f, 0, 0.6f);
			DrawDebugLine(W, P + V2, P + V3, Color, true, 15.f, 0, 0.6f);
			DrawDebugLine(W, P + V3, P + V4, Color, true, 15.f, 0, 0.6f);
			DrawDebugLine(W, P + V4, P + V1, Color, true, 15.f, 0, 0.6f);
		}
	}
}
