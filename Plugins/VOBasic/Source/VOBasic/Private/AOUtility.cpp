#include "AOUtility.h"
#include "VOManager.h"
#include "VOSettings.h"

FVector AOUtility::ComputeBestAcceleration(
	const UVOFollowingComponent* Comp,
	const FVector& CurVel,
	const FVector& TargetPos,
	const TArray<FVONeighborView>& Neis,
	const FVOParams& Params,
	FAOConesSoA& AOCones,
	TArray<FAOWorkSegment>& WorkSegments,
	TArray<TArray<FAOConeIntersection>>& SideIntersections,
	TArray<TArray<FAOSegment>>& OutsideSegments, 
	TArray<FVector2D>& Candidates,
	int32& BestCandidateIdx)
{
	const FVector ActorPos = Comp->GetOwnerLocation();

	AOCones.Reset();
	BuildAOCones(Neis, ActorPos, CurVel, Params, AOCones);

	int32 TotalSides = 0;
	PrepareAndSortWorkSegments(AOCones, WorkSegments, TotalSides);

	SideIntersections.SetNum(TotalSides);
	
	for (int32 i = 0; i < TotalSides; ++i)
		SideIntersections[i].Reset();

	CollectIntersections(WorkSegments, SideIntersections);
	SortSideIntersections(SideIntersections);
	ClassifySegments(AOCones, SideIntersections, OutsideSegments);
	
	FVector2D DesiredAcc2D = FVector2D::ZeroVector;
	FVector2D TargetPosRel = FVector2D(TargetPos - ActorPos); // For parabola
	if (!TargetPosRel.IsZero())
	{
		FVector2D VelRel = FVector2D(CurVel);  // For parabola
		FVector2D TargetAcc = FVector2D::Zero(); // For parabola
		double MaxAcc = Params.MaxAcceleration; // For circle 1
		// CurVel for circle 2

		FAOParabola Parabola;
		Parabola.A = TargetPosRel * 2.0;
		Parabola.B = VelRel * -2.0;
		Parabola.C = TargetAcc;

		// Формируем Круг 1: Ограничение модуля ускорения
		FAOCircle C1;
		C1.Center = FVector2D::ZeroVector;
		C1.R = Params.MaxAcceleration;
		C1.RSq = C1.R * C1.R;

		// Формируем Круг 2: Кинематическое ограничение
		// Условие: (Acc + V0/ta)^2 <= (Vmax/ta)^2
		// Center = -V0 / ta
		// Radius = Vmax / ta
		double Ta = /*FMath::Max(Params.TauHorizon, 0.01f)*/1.5f;
		FAOCircle C2;
		C2.Center = FVector2D(CurVel) * (-1.0 / Ta);
		C2.R = Params.MaxSpeed / Ta;
		C2.RSq = C2.R * C2.R;

		FParabolaResult Result = FindParabolaIntersection(Parabola, C1, C2, 16);
		UE_LOG(LogTemp, Display, TEXT("U: %f"), Result.U);

		if (Result.bFound)
		{
			DesiredAcc2D = Result.Point;
			UE_LOG(LogTemp, Display, TEXT("AHHAHA X: %f, Y: %f"), DesiredAcc2D.X, DesiredAcc2D.Y);
		}
	}

	FVector2D CurAcc2D = FVector2D(Comp->GetCachedAcceleration());
	UE_LOG(LogTemp, Display, TEXT("UUUSAAS X: %f, Y: %f"), CurAcc2D.X, CurAcc2D.Y);
	
	FVector2D Best = SelectBestAccelerationFromOutsideSegments(
		DesiredAcc2D,
		CurAcc2D,
		Neis,
		ActorPos,
		Params,
		AOCones,
		OutsideSegments,
		Candidates,
		BestCandidateIdx
	);
	UE_LOG(LogTemp, Display, TEXT("X: %f, Y: %f"), Best.X, Best.Y);

	return FVector(Best.X, Best.Y, 0.f);
}

void AOUtility::PrepareAndSortWorkSegments(const FAOConesSoA& InCones, TArray<FAOWorkSegment>& OutWorkSegments, int32& OutTotalSides)
{
	OutWorkSegments.Reset();
	OutTotalSides = InCones.Num() * 4; // L, R, TH, MinTime

	for (int32 i = 0; i < InCones.Num(); ++i)
	{
		auto AddSideToWork = [&](const TArray<FAOSegment>& Segments, int32 SideIdx)
		{
			for (int32 s = 0; s < Segments.Num(); ++s)
			{
				// TODO: Sort when adding?
				FAOWorkSegment& WS = OutWorkSegments.Add_GetRef(FAOWorkSegment());
				WS.ConeIdx = i;
				WS.SideIdx = SideIdx; // 0=Left, 1=Right, 2=TH
				WS.SegIdx = s;
				WS.SegmentRef = &Segments[s];
			}
		};

		// Left (SideIdx = 0)
		if (InCones.isLeftSideValid[i])
		{
			AddSideToWork(InCones.LeftSide[i].Segments, 0);
		}

		// Right (SideIdx = 1)
		if (InCones.isRightSideValid[i])
		{
			AddSideToWork(InCones.RightSide[i].Segments, 1);
		}

		// Time Horizon (SideIdx = 2)
		if (InCones.isTHSegmentValid[i])
		{			
			FAOWorkSegment& WS = OutWorkSegments.Add_GetRef(FAOWorkSegment());
			WS.ConeIdx = i;
			WS.SideIdx = 2; 
			WS.SegIdx = 0;  
			WS.SegmentRef = &InCones.TimeHorizonSegment[i];
		}

		// Min Time Segment (SideIdx = 3)
		if (InCones.isMinTimeSegmentValid[i])
		{
			FAOWorkSegment& WS = OutWorkSegments.Add_GetRef(FAOWorkSegment());
			WS.ConeIdx = i;
			WS.SideIdx = 3; 
			WS.SegIdx = 0;
			WS.SegmentRef = &InCones.MinTimeSegment[i];
		}
	}

	// Sort by MinX
	OutWorkSegments.Sort([](const FAOWorkSegment& A, const FAOWorkSegment& B) {
		return A.SegmentRef->MinX < B.SegmentRef->MinX;
	});
}

void AOUtility::CollectIntersections(const TArray<FAOWorkSegment>& WorkSegments, TArray<TArray<FAOConeIntersection>>& OutSideIntersections)
{
    const int32 NumWork = WorkSegments.Num();

	for (int32 i = 0; i < NumWork; ++i)
	{
		const FAOWorkSegment& W = WorkSegments[i];
		int32 FlatSideIdx = W.ConeIdx * 4 + W.SideIdx;
		
		OutSideIntersections[FlatSideIdx].Add({ W.SegmentRef->P1, 0.f, W.SegIdx, false, false });
		OutSideIntersections[FlatSideIdx].Add({ W.SegmentRef->P2, 1.f, W.SegIdx, false, false });
	}

	for (int32 i = 0; i < NumWork; ++i)
	{
		const FAOWorkSegment& WA = WorkSegments[i];
		const FAOSegment& SegA = *WA.SegmentRef;

		for (int32 j = i + 1; j < NumWork; ++j)
		{
			const FAOWorkSegment& WB = WorkSegments[j];
			const FAOSegment& SegB = *WB.SegmentRef;

			// AABB
			if (SegB.MinX > SegA.MaxX) break; 
			if (SegA.MaxY < SegB.MinY || SegA.MinY > SegB.MaxY) continue;
			if (WA.ConeIdx == WB.ConeIdx) continue; 

			FVector2D IntP;
			if (SegmentIntersection2D(SegA.P1, SegA.P2, SegB.P1, SegB.P2, IntP))
			{
				FVector2D DirA = (SegA.P2 - SegA.P1);
				float LenSqA = DirA.SizeSquared();
				float tA = (LenSqA > KINDA_SMALL_NUMBER) ? FVector2D::DotProduct(IntP - SegA.P1, DirA) / LenSqA : 0.f;

				FVector2D DirB = (SegB.P2 - SegB.P1);
				float LenSqB = DirB.SizeSquared();
				float tB = (LenSqB > KINDA_SMALL_NUMBER) ? FVector2D::DotProduct(IntP - SegB.P1, DirB) / LenSqB : 0.f;
				
				tA = FMath::Clamp(tA, 0.f, 1.f);
				tB = FMath::Clamp(tB, 0.f, 1.f);
				
				bool bEntryA = FVector2D::DotProduct(DirA, SegB.OutsideNormal) < 0.f;
				bool bEntryB = FVector2D::DotProduct(DirB, SegA.OutsideNormal) < 0.f;

				int32 SideIdxA = WA.ConeIdx * 4 + WA.SideIdx;
				OutSideIntersections[SideIdxA].Add({ IntP, tA, WA.SegIdx, bEntryA, true });

				int32 SideIdxB = WB.ConeIdx * 4 + WB.SideIdx;
				OutSideIntersections[SideIdxB].Add({ IntP, tB, WB.SegIdx, bEntryB, true });
			}
		}
	}
}

void AOUtility::SortSideIntersections(TArray<TArray<FAOConeIntersection>>& SideIntersections)
{
	for (int32 i = 0; i < SideIntersections.Num(); ++i)
	{
		TArray<FAOConeIntersection>& List = SideIntersections[i];
		if (List.Num() < 2) continue;

		List.Sort([](const FAOConeIntersection& A, const FAOConeIntersection& B)
		{
			if (A.SegmentIndex != B.SegmentIndex)
				return A.SegmentIndex < B.SegmentIndex;
			
			return A.t < B.t;
		});
	}
}

void AOUtility::ClassifySegments(
	const FAOConesSoA& Cones,
	const TArray<TArray<FAOConeIntersection>>& SideIntersections,
	TArray<TArray<FAOSegment>>& OutOutsideSegments)
{
	OutOutsideSegments.SetNum(SideIntersections.Num());

	for (int32 RayIdx = 0; RayIdx < SideIntersections.Num(); ++RayIdx)
	{
		OutOutsideSegments[RayIdx].Reset();
		
		const TArray<FAOConeIntersection>& List = SideIntersections[RayIdx];
		if (List.Num() < 2) continue;

		int32 ConeIdx = RayIdx / 4;
		int32 SideType = RayIdx % 4;
		
		FVector2D FirstPoint = List[0].P;
		int32 CountOfVOs = CountAOsForPoint(Cones, ConeIdx, FirstPoint);
		
		for (int32 j = 1; j < List.Num(); ++j)
		{
			const FAOConeIntersection& Prev = List[j - 1];
			const FAOConeIntersection& Curr = List[j];

			if (CountOfVOs == 0)
			{
				// TODO: Remove if?
				if (Prev.SegmentIndex == Curr.SegmentIndex && 
					FVector2D::DistSquared(Prev.P, Curr.P) > KINDA_SMALL_NUMBER)
				{
					FVector2D SegNormal = FVector2D::ZeroVector;
					if		(SideType == 0)	SegNormal = Cones.LeftSide[ConeIdx].Segments[Prev.SegmentIndex].OutsideNormal;
					else if (SideType == 1) SegNormal = Cones.RightSide[ConeIdx].Segments[Prev.SegmentIndex].OutsideNormal;
					else if (SideType == 2) SegNormal = Cones.TimeHorizonSegment[ConeIdx].OutsideNormal;
					else if (SideType == 3) SegNormal = Cones.MinTimeSegment[ConeIdx].OutsideNormal;

					FAOSegment& NewSeg = OutOutsideSegments[RayIdx].Add_GetRef(FAOSegment());
					NewSeg.Init(Prev.P, Curr.P, SegNormal);
				}
			}
			
			if (Curr.bIsIntersection)
			{
				if (Curr.bIsEntry)
					CountOfVOs++;
				else
					CountOfVOs = FMath::Max(0, CountOfVOs - 1);
			}
		}
	}
}

int32 AOUtility::SolveQuadratic(double A, double B, double C, double& OutX1, double& OutX2)
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

bool AOUtility::FindCircleCircleIntersections(const FAOCircle& C1, const FAOCircle& C2, FVector2D& OutP1, FVector2D& OutP2)
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

FParabolaResult AOUtility::FindParabolaIntersection(const FAOParabola& Curve, const FAOCircle& C1, const FAOCircle& C2, int32 DiscSegments)
{
	FParabolaResult Result;
	Result.U = -1.0;

	// Расстояние между центрами
	double DistSq = FVector2D::DistSquared(C1.Center, C2.Center);
	double Dist = FMath::Sqrt(DistSq);

	// 1. Проверка на непересечение (Disjoint)
	// Если круги слишком далеко друг от друга, пересечения нет.
	if (Dist > C1.R + C2.R + KINDA_SMALL_NUMBER)
	{
		return Result; // bFound = false
	}

	// Лямбда для обработки дуги (или полного круга)
	// Если bFullCircle == true, Segments удваивается, и ConstraintC игнорируется
	auto ProcessArc = [&](const FAOCircle& TargetC, const FAOCircle& ConstraintC, 
		const FVector2D& PStart, const FVector2D& PEnd, 
		bool bFullCircle, int32 SegmentsCount)
	{
		double TotalAngle;
		FVector2D VStart;

		if (bFullCircle)
		{
			TotalAngle = 2.0 * PI;
			VStart = FVector2D(TargetC.R, 0.0); // Начинаем с "востока"
		}
		else
		{
			VStart = PStart - TargetC.Center;
			double Ang1 = FMath::Atan2(VStart.Y, VStart.X);
			double Ang2 = FMath::Atan2(PEnd.Y - TargetC.Center.Y, PEnd.X - TargetC.Center.X);

			TotalAngle = Ang2 - Ang1;
			if (TotalAngle <= -PI) TotalAngle += 2.0 * PI;
			else if (TotalAngle > PI) TotalAngle -= 2.0 * PI;

			// Проверка направления (берем середину дуги)
			double MidAngle = Ang1 + TotalAngle * 0.5;
			FVector2D MidPt = FVector2D(
				TargetC.Center.X + TargetC.R * FMath::Cos(MidAngle),
				TargetC.Center.Y + TargetC.R * FMath::Sin(MidAngle)
			);

			// Если середина дуги снаружи ограничивающего круга, инвертируем дугу
			if ((MidPt - ConstraintC.Center).SizeSquared() > ConstraintC.RSq)
			{
				TotalAngle = (TotalAngle > 0.0) ? TotalAngle - 2.0 * PI : TotalAngle + 2.0 * PI;
			}
		}

		double Step = TotalAngle / (double)SegmentsCount;
		double SinStep, CosStep;
		FMath::SinCos(&SinStep, &CosStep, Step);

		// Precompute squared segment length
		double SegLenSq = 2.0 * TargetC.RSq * (1.0 - CosStep);

		FVector2D CurrOffset = VStart;
		FVector2D PrevP = bFullCircle ? (TargetC.Center + VStart) : PStart;

		for (int32 i = 0; i < SegmentsCount; ++i)
		{
			// Vector Rotation
			double NextX = CurrOffset.X * CosStep - CurrOffset.Y * SinStep;
			double NextY = CurrOffset.X * SinStep + CurrOffset.Y * CosStep;

			CurrOffset = FVector2D(NextX, NextY);
			FVector2D CurrP = TargetC.Center + CurrOffset;

			FVector2D SegDir = CurrP - PrevP;
			FVector2D Normal(-SegDir.Y, SegDir.X);
			double C_Line = -(Normal | PrevP);

			double QA = Normal | Curve.A;
			double QB = Normal | Curve.B;
			double QC = (Normal | Curve.C) + C_Line;

			double U1, U2;
			int32 Roots = SolveQuadratic(QA, QB, QC, U1, U2);

			if (Roots > 0)
			{
				auto CheckPoint = [&](double U)
				{
					if (U > Result.U)
					{
						FVector2D P = Curve.Eval(U);

						// Если полный круг, проверка на Constraint не нужна (мы и так внутри)
						// Если дуга, проверяем попадание во второй круг
						bool bInConstraint = bFullCircle || ((P - ConstraintC.Center).SizeSquared() <= ConstraintC.RSq + 0.1);
						
						if (bInConstraint)
						{
							double Proj = (P - PrevP) | SegDir;
							if (Proj >= -1e-2 && Proj <= SegLenSq + 1e-2)
							{
								Result.U = U;
								Result.Point = P;
								Result.bFound = true;
							}
						}
					}
				};

				CheckPoint(U1);
				if (Roots > 1) CheckPoint(U2);
			}

			PrevP = CurrP;
		}
	};

	// 2. Проверка на вложенность (Nested)
	bool bC2inC1 = Dist + C2.R <= C1.R + KINDA_SMALL_NUMBER;
	bool bC1inC2 = Dist + C1.R <= C2.R + KINDA_SMALL_NUMBER;

	if (bC2inC1)
	{
		// C2 внутри C1 -> Зона пересечения равна C2. Ищем пересечение с C2.
		// Удваиваем сегменты для точности.
		ProcessArc(C2, C1, FVector2D::ZeroVector, FVector2D::ZeroVector, true, DiscSegments * 2);
		return Result;
	}
	
	if (bC1inC2)
	{
		// C1 внутри C2 -> Зона пересечения равна C1. Ищем пересечение с C1.
		ProcessArc(C1, C2, FVector2D::ZeroVector, FVector2D::ZeroVector, true, DiscSegments * 2);
		return Result;
	}

	// 3. Обычное пересечение (Lens)
	FVector2D Int1, Int2;
	if (FindCircleCircleIntersections(C1, C2, Int1, Int2))
	{
		ProcessArc(C1, C2, Int1, Int2, false, DiscSegments);
		ProcessArc(C2, C1, Int1, Int2, false, DiscSegments);
	}

	return Result;
}

FVector2D AOUtility::SelectBestAccelerationFromOutsideSegments(
    const FVector2D& DesiredAcc,
    const FVector2D& CurAcc,
    const TArray<FVONeighborView>& Neis,
    const FVector& ActorPos,
    const FVOParams& Params,
    const FAOConesSoA& Cones,
    const TArray<TArray<FAOSegment>>& OutsideSegmentsByRays,
    TArray<FVector2D>& OutCandidates,
    int32& OutBestIdx)
{
    OutCandidates.Reset();
    OutBestIdx = -1;

    float BestScore = -FLT_MAX;
    FVector2D BestV = FVector2D::ZeroVector;
    bool bFoundAny = false;

    auto EvaluatePoint = [&](const FVector2D& P)
    {
        OutCandidates.Add(P); 
        int32 CurrentIdx = OutCandidates.Num() - 1;

        float Score = ScoreAccelerationCandidate(P, DesiredAcc, CurAcc, Neis, ActorPos, Params);

        if (Score > BestScore)
        {
            BestScore = Score;
            BestV = P;
            OutBestIdx = CurrentIdx;
            bFoundAny = true;
        }
    };

    // Zero 
    if (CountAOsForPoint(Cones, -1, FVector2D::ZeroVector) == 0)
    {
        EvaluatePoint(FVector2D::ZeroVector);
    }

    // Desired
    if (CountAOsForPoint(Cones, -1, DesiredAcc) == 0)
    {
        EvaluatePoint(DesiredAcc);
    }

	// Desired
	if (CountAOsForPoint(Cones, -1, CurAcc) == 0)
	{
		EvaluatePoint(CurAcc);
	}

    for (const TArray<FAOSegment>& SegList : OutsideSegmentsByRays)
    {
        for (const FAOSegment& Seg : SegList)
        {
            // Ends
            EvaluatePoint(Seg.P1);
            EvaluatePoint(Seg.P2);

            // Projection of desired acc
            FVector2D SegDir = Seg.P2 - Seg.P1;
            float SegLenSq = SegDir.SizeSquared();

            if (SegLenSq > KINDA_SMALL_NUMBER)
            {
                float t = FVector2D::DotProduct(DesiredAcc - Seg.P1, SegDir) / SegLenSq;

                if (t > 0.01f && t < 0.99f)
                {
                    FVector2D Projection = Seg.P1 + SegDir * t;
                    EvaluatePoint(Projection);
                }
            }
        }
    }
	
    if (!bFoundAny)
    {
    	// TODO: Change fallback
        FVector2D Fallback = FVector2D::ZeroVector;
    	UE_LOG(LogTemp, Warning, TEXT("No acceleration candidates found! Using fallback: %s"), *Fallback.ToString());
        
        EvaluatePoint(Fallback);
        
        return Fallback;
    }

    return BestV;
}

bool AOUtility::IsPointInsideAO(const FAOConesSoA& Cones, int32 ConeIdx, const FVector2D& P)
{
	const TArray<FAOTriangle>& Tris = Cones.Tris[ConeIdx];
	for (const FAOTriangle& Tri : Tris)
	{
		if (IsPointInTriangle(P, Tri.P1, Tri.P2, Tri.P3))
		{
			return true;
		}
	}

	const TArray<FAOQuad>& Quads = Cones.Quads[ConeIdx];
	for (const FAOQuad& Quad : Quads)
	{
		// TODO: Check without triangulation
		
		// Tri 1: P1-P2-P3
		if (IsPointInTriangle(P, Quad.P1, Quad.P2, Quad.P3))
		{
			return true;
		}
		// Tri 2: P1-P3-P4
		if (IsPointInTriangle(P, Quad.P1, Quad.P3, Quad.P4))
		{
			return true;
		}
	}

	return false;
}

bool AOUtility::IsPointInsideAnyAO(const FAOConesSoA& Cones, const FVector2D& P, int32 IgnoreConeIdx)
{
	int32 Num = Cones.Num();
	for (int32 i = 0; i < Num; ++i)
	{
		if (i == IgnoreConeIdx) continue;

		if (IsPointInsideAO(Cones, i, P))
			return true;
	}
	return false;
}

int32 AOUtility::CountAOsForPoint(const FAOConesSoA& Cones, int32 ConeIndexToSkip, const FVector2D& P)
{
	int32 Count = 0;
	int32 Num = Cones.Num();
	
	for (int32 i = 0; i < Num; ++i)
	{
		if (i == ConeIndexToSkip) continue;

		if (IsPointInsideAO(Cones, i, P))
		{
			Count++;
		}
	}
	return Count;
}

bool AOUtility::IsPointInTriangle(const FVector2D& P, const FVector2D& A, const FVector2D& B, const FVector2D& C)
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

float AOUtility::ScoreAccelerationCandidate(
	const FVector2D& CandidateAcc,
	const FVector2D& DesiredAcc,
	const FVector2D& CurAcc,
	const TArray<FVONeighborView>& Neis,
	const FVector& ActorPos,
	const FVOParams& Params)
{
	const float W_Proximity = 1.0f;  
	const float W_Effort    = 0.05f; 
	const float W_Smooth    = 0.9f;  

	float DistDesSq = FVector2D::DistSquared(CandidateAcc, DesiredAcc);
	float MagSq = CandidateAcc.SizeSquared();
	float JerkSq = FVector2D::DistSquared(CandidateAcc, CurAcc);

	return - (W_Proximity * DistDesSq) - (W_Effort * MagSq) - (W_Smooth * JerkSq);
}

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
			//FString Msg = FString::Printf(TEXT("L: %.2f | R: %.2f"), GrazeSourceP.X * PointL.Y - GrazeSourceP.Y * PointL.X, GrazeSourceP.X * PointR.Y - GrazeSourceP.Y * PointR.X);
			//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, Msg);
			
			if (GrazeSourceP.X * PointL.Y - GrazeSourceP.Y * PointL.X > 0.f)
				isConvexL = true;
			if (GrazeSourceP.X * PointR.Y - GrazeSourceP.Y * PointR.X < 0.f)
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

#pragma region MinReactionTime Segment
	if (PointsL.Num() > 0 && PointsR.Num() > 0)
    {
        FVector2D P_L_Start = PointsL[0];
        FVector2D P_R_Start = PointsR[0];

        FVector2D Dir = P_R_Start - P_L_Start;
        
        FVector2D SegNormal;
        if (Dir.SizeSquared() > KINDA_SMALL_NUMBER)
        {
            FVector2D DirNorm = Dir.GetSafeNormal();
            
            
            FVector2D TempNormal = FVector2D(DirNorm.Y, -DirNorm.X); // Вправо от вектора L->R
            
            // Проверяем направление относительно вектора "развития" конуса (P[1] - P[0])
            FVector2D ExpansionDir = (PointsL.Num() > 1) ? (PointsL[1] - PointsL[0]) : Vel;
            
            if (FVector2D::DotProduct(TempNormal, ExpansionDir) > 0)
            {
                // Если нормаль смотрит туда же, куда растет конус -> инвертируем
                SegNormal = -TempNormal;
            	UE_LOG(LogTemp, Warning, TEXT("Inverted normal!"));
            }
            else
            {
                SegNormal = TempNormal;
            }
        }
        else
        {
            SegNormal = -Vel.GetSafeNormal(); 
        }

        Cone.MinTimeSegment.Init(P_L_Start, P_R_Start, SegNormal);
        
        Cone.isMinTimeSegmentValid = (FVector2D::DistSquared(P_L_Start, P_R_Start) > KINDA_SMALL_NUMBER);
    }
    else
    {
        Cone.isMinTimeSegmentValid = false;
    }
#pragma endregion 

	// TODO: t_last;
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

		FString Msg = FString::Printf(TEXT("THL: X: %.2f | Y: %.2f"), TimeHorizonL.X, TimeHorizonL.Y);
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, Msg);

		TimeHorizonR = TimeHorizonL + 2 * (TimeHorizonGrazePoint - TimeHorizonL);
		PointsR.Add(TimeHorizonR);
		const FVector2D LastNormR = NormalsR.Last();
		NormalsR.Add(LastNormR);
	}

	FString Msg = FString::Printf(TEXT("BCR: %d"), PointsR.Num());
	GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, Msg);


	// Build convex points
	if (isConvexL)
		PointsL = BuildConvexSide(PointsL, NormalsL);
	if (isConvexR)
		PointsR = BuildConvexSide(PointsR, NormalsR);

	Msg = FString::Printf(TEXT("R: %d"), PointsR.Num());
	GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, Msg);

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

	Msg = FString::Printf(TEXT("RN: %d"), PointsR.Num());
	GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, Msg);
	
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

	Msg = FString::Printf(TEXT("CQ: %d"), Cone.Quads.Num());
	GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, Msg);

	Cone.TimeHorizonSegment.Init(
			TimeHorizonSegment.P1, 
			TimeHorizonSegment.P2, 
			TimeHorizonGrazeNormal
		);
        
	if (FVector2D::DistSquared(TimeHorizonSegment.P1, TimeHorizonSegment.P2) < KINDA_SMALL_NUMBER)
	{
		Cone.isTHSegmentValid = false;
	}

	auto ConvertPointsToSegments = [](const TArray<FVector2D>& Points, FAOSide& OutSide, bool bIsLeft)
	{
		OutSide.Segments.Reset();
		if (Points.Num() < 2) return;

		for (int32 i = 0; i < Points.Num() - 1; ++i)
		{
			const FVector2D& P1 = Points[i];
			const FVector2D& P2 = Points[i+1];

			if (FVector2D::DistSquared(P1, P2) < KINDA_SMALL_NUMBER) // TODO: Check if it's needed'
				continue;

			FVector2D Dir = (P2 - P1).GetSafeNormal();
            
			FVector2D Normal = bIsLeft ? FVector2D(-Dir.Y, Dir.X) : FVector2D(Dir.Y, -Dir.X); //TODO: Maybe use old arrays

			FAOSegment NewSeg;
			NewSeg.Init(P1, P2, Normal);
			OutSide.Segments.Add(NewSeg);
		}
	};

	if (Cone.isLeftSideValid && PointsL.Num() > 1)
	{
		ConvertPointsToSegments(PointsL, Cone.LeftSide, true);
	}

	if (Cone.isRightSideValid && PointsR.Num() > 1)
	{
		ConvertPointsToSegments(PointsR, Cone.RightSide, false);
	}

	return Cone;
}

bool AOUtility::FindLineAndSegmentIntersection(const FVector2D& LineNormal, const float LineC,
	const FVector2D& SegmentP1, const FVector2D& SegmentP2,
	float& outT, FVector2D& outPoint)
{
	const float signedDistStart = FVector2D::DotProduct(LineNormal, SegmentP1) + LineC;	// s_THL1
	const float signedDistEnd   = FVector2D::DotProduct(LineNormal, SegmentP2) + LineC; // s_THL2

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
		FColor Color = GetColorFromSeed(i);
		const TArray<FAOTriangle>& ConeTris = Cones.Tris[i];
		for (const FAOTriangle& Tri : ConeTris)
		{
			FVector V1(Tri.P1.X, Tri.P1.Y, 0.f);
			FVector V2(Tri.P2.X, Tri.P2.Y, 0.f);
			FVector V3(Tri.P3.X, Tri.P3.Y, 0.f);

			FString Msg = FString::Printf(TEXT("jjjjj"));
			GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, Msg);

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

void AOUtility::DrawAOOutsideSegments(const UVOFollowingComponent* Comp, const TArray<TArray<FAOSegment>>& OutsideSegments)
{
	UWorld* W = Comp->GetWorld();
	if (!W) return;
    
	const FVector P = Comp->GetOwnerLocation();

	for (int32 i = 0; i < OutsideSegments.Num(); ++i)
	{
		const TArray<FAOSegment>& SegList = OutsideSegments[i];
		FColor Color = GetColorFromSeed(i / 3);
		for (const FAOSegment& Seg : SegList)
		{
			FVector Start(Seg.P1.X, Seg.P1.Y, 0.f);
			FVector End(Seg.P2.X, Seg.P2.Y, 0.f);
            
			DrawDebugLine(W, P + Start, P + End, Color, true, -1.f, 0, 3.0f);
		}
	}
}
