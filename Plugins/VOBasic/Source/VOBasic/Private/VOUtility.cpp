#include "VOUtility.h"

#include "AvoidanceMath.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "VOManager.h"
#include "VOFollowingComponent.h"
#include "VelocityObstacleTypes.h"

// Private declarations
namespace
{
    bool WillCollideWithinTau(
        const FVector2D& RelativePosition,
        const FVector2D& RelativeVelocity,
        float Radius,
        float TimeHorizon,
        float* OutTOI = nullptr
    );

    bool IsVelocityForbidden(
        const FVector2D& CandidateVA,
        const TArray<FVONeighborView>& Neis,
        const FVector& ActorPos,
        const FVOParams& Params
    );

    FVOCone ComputeVOCone(
        const float R,
        const FVector2D& C,
        const FVector2D& Vel,
        const FVOParams& Params
    );

    void BuildVOCones(
        const TArray<FVONeighborView>& Neis,
        const FVector& ActorPos,
        const FVOParams& Params,
        FVOConesSoA& OutVOCones
    );

    void CollectIntersections(
        const FVOConesSoA& VOCones,
        TArray<TArray<FVOConeIntersection>>& IntersectionsByRays
    );
	
    void SortIntersectionsByRays(
        TArray<TArray<FVOConeIntersection>>& IntersectionsByRays
    );

    int32 CountVOsForPoint(
        const FVOConesSoA& VOCones,
        int32 ConeIndexToSkip,
        const FVector2D& P
    );

    void ClassifySegments(
        const FVOConesSoA& VOCones,
        const TArray<TArray<FVOConeIntersection>>& IntersectionsByRays,
        TArray<TArray<FVOOutsideSegment>>& OutsideSegmentsByRays
    );

    float ScoreVelocityCandidate(
        const FVector2D& V,
        const FVector2D& DesiredVel2D,
        const FVector2D& CurVel2D,
        const TArray<FVONeighborView>& Neis,
        const FVector& ActorPos,
        const FVOParams& Params
    );

    FVector2D SelectBestVelocityFromOutsideSegments(
        const FVOCalculationContext& Ctx
    );
}

namespace VOUtility
{
	FVector ComputeVelocity(const FVOCalculationContext& Ctx)
	{
		if (Ctx.Cones) Ctx.Cones->Reset();
		
		const FVector2D Desired2D(Ctx.DesiredVelocity.X, Ctx.DesiredVelocity.Y);

		// 1. Check if direct velocity is possible
		const bool bDesiredForbidden = IsVelocityForbidden(Desired2D, *Ctx.Neis, Ctx.ActorPos, *Ctx.Params);
		if (!bDesiredForbidden)
		{
			// Clear debug candidates if no complexity needed
			if (Ctx.OutCandidates) Ctx.OutCandidates->Reset();
			if (Ctx.OutBestCandidateIdx) *Ctx.OutBestCandidateIdx = -1;
			
			return Ctx.DesiredVelocity.GetClampedToMaxSize2D(Ctx.Params->MaxSpeed);
		}

		// 2. Build Cones
		BuildVOCones(*Ctx.Neis, Ctx.ActorPos, *Ctx.Params, *Ctx.Cones);

		const int32 NumRays = Ctx.Cones->Num() * 3;

		// 3. Prepare Intersections
		auto& Intersections = *Ctx.Intersections;
		Intersections.SetNum(NumRays, EAllowShrinking::No);
		//for(auto& Arr : Intersections) Arr.Reset(); // TODO: Check if needed
		
		CollectIntersections(*Ctx.Cones, Intersections);
		SortIntersectionsByRays(Intersections);

		// 4. Classify Segments
		auto& OutsideSegments = *Ctx.OutsideSegments;
		OutsideSegments.SetNum(NumRays, EAllowShrinking::No);
		//for(auto& Arr : OutsideSegments) Arr.Reset();

		ClassifySegments(*Ctx.Cones, Intersections, OutsideSegments);

		// 5. Select Best
		FVector2D Best2D = SelectBestVelocityFromOutsideSegments(Ctx);

		return FVector(Best2D.X, Best2D.Y, 0.f);
	}

	void DrawVOCones(const UVOFollowingComponent* Comp, const FVOConesSoA& Cones)
	{
		UWorld* W = Comp->GetWorld(); if (!W) return;
		FVector P = Comp->GetOwnerLocation();
		for (int i = 0; i < Cones.Num(); i++)
		{
			FColor Color = FColor::MakeRandomColor();
			
			FVector Start;
			FVector End;

			if (Cones.bIsLeftRaySegmentValid[i])
			{
				Start = FVector(Cones.LeftRaySegment[i].P1.X, Cones.LeftRaySegment[i].P1.Y, 0.f);
				End = FVector(Cones.LeftRaySegment[i].P2.X, Cones.LeftRaySegment[i].P2.Y, 0.f);
				DrawDebugLine(W, Start + P, End + P, Color, true, 15.f, 0, 0.6f);
			}

			if (Cones.bIsRightRaySegmentValid[i])
			{
				Start = FVector(Cones.RightRaySegment[i].P1.X, Cones.RightRaySegment[i].P1.Y, 0.f);
				End = FVector(Cones.RightRaySegment[i].P2.X, Cones.RightRaySegment[i].P2.Y, 0.f);
				DrawDebugLine(W, Start + P, End + P, Color, true, 15.f, 0, 0.6f);
			}

			if (Cones.bIsTHSegmentValid[i])
			{
				Start = FVector(Cones.TimeHorizonSegment[i].P1.X, Cones.TimeHorizonSegment[i].P1.Y, 0.f);
				End = FVector(Cones.TimeHorizonSegment[i].P2.X, Cones.TimeHorizonSegment[i].P2.Y, 0.f);
				DrawDebugLine(W, Start + P, End + P, Color, true, 15.f, 0, 0.6f);
			}
		}
	}

	void DrawCombinedVO(const UVOFollowingComponent* Comp, const TArray<TArray<FVOOutsideSegment>>& OutsideSegmentsByRays)
	{
		UWorld* W = Comp->GetWorld(); if (!W) return;
		FVector P = Comp->GetOwnerLocation();
		FColor Color;
		for (int i = 0; i < OutsideSegmentsByRays.Num(); i++)
		{
			if (i % 3 == 0)
				Color = FColor::MakeRandomColor();
			for (int j = 0; j < OutsideSegmentsByRays[i].Num(); j++)
			{
				FVector Start = { OutsideSegmentsByRays[i][j].P1.X, OutsideSegmentsByRays[i][j].P1.Y, 0.f };
				FVector End = { OutsideSegmentsByRays[i][j].P2.X, OutsideSegmentsByRays[i][j].P2.Y, 0.f };
				DrawDebugLine(W, Start + P, End + P, Color, true, 15.f, 0, 1.2f);
			}
		}
	}

	void DrawVelocityCandidates(const UVOFollowingComponent* Comp, const TArray<FVector2D>& Candidates, int32 BestCandidateIdx, float PointSize, float LifeTime)
	{
		const FVector ActorPos = Comp->GetOwnerLocation();
		UWorld* W = Comp->GetWorld();

		const FColor CandidateColor(90, 180, 255); // голубой
		const FColor BestColor(255, 220, 0);       // жёлтый

		for (int32 i = 0; i < Candidates.Num(); ++i)
		{
			const FVector2D V = Candidates[i];
			const FVector P = ActorPos + FVector(V.X, V.Y, 0.f);

			const bool bIsBest = (i == BestCandidateIdx);
			const FColor Col = bIsBest ? BestColor : CandidateColor;
			const float Sz = bIsBest ? (PointSize * 1.7f) : PointSize;

			DrawDebugPoint(W, P, Sz, Col, /*bPersistentLines*/ true, LifeTime);
		}
	}
}

// Private implementations
namespace
{
	bool WillCollideWithinTau(const FVector2D& RelativePosition, const FVector2D& RelativeVelocity, float Radius, float TimeHorizon, float* OutTOI)
	{
		const float PV = FVector2D::DotProduct(RelativePosition, RelativeVelocity);

		if (PV <= 0.f)
			return false;

		const float VV = RelativeVelocity.SizeSquared();
		const float PP = RelativePosition.SizeSquared();
		const float R2 = Radius * Radius;
		const float a = VV;
		const float b = -2.f * PV;
		const float c = PP - R2;
		const float Disc = b * b - 4.f * a * c;

		if (Disc < 0.f || a < 1e-6f)
			return false;

		const float SqrtDisc = FMath::Sqrt(Disc);
		const float Inv2A = 1.f / (2.f * a);
		const float T1 = (-b - SqrtDisc) * Inv2A;
		const float T2 = (-b + SqrtDisc) * Inv2A;

		float THit = TNumericLimits<float>::Max();
		if (T1 > 0.f)
			THit = T1;
		else if (T2 > 0.f)
			THit = T2;
		else
			return false;

		if (THit <= TimeHorizon)
		{
			if (OutTOI)
				*OutTOI = THit;
			return true;
		}
		return false;
	}

	bool IsVelocityForbidden(const FVector2D& CandidateVA, const TArray<FVONeighborView>& Neis, const FVector& ActorPos, const FVOParams& Params)
	{
		for (const FVONeighborView& N : Neis)
		{
			const FVector2D pRel(N.Pos.X - ActorPos.X, N.Pos.Y - ActorPos.Y);
			const FVector2D vRel = CandidateVA - FVector2D(N.Vel.X, N.Vel.Y);
			const float R = Params.AgentRadius + N.Radius;
			if (WillCollideWithinTau(pRel, vRel, R, Params.TauHorizon, nullptr))
				return true;
		}
		return false;
	}

	FVOCone ComputeVOCone(const float R, const FVector2D& C, const FVector2D& Vel, const FVOParams& Params)
	{
		FVOCone Cone;
		Cone.Apex = Vel;

		float CSizeSquared = C.X * C.X + C.Y * C.Y;
		float RR = R * R;
		FVector2D P = RR * FVector2D(C.X, C.Y);
		FVector2D Q = R * FMath::Sqrt(CSizeSquared - RR) * FVector2D(C.Y, -C.X);

		// Line equation
		float DenominatorInverted = 1.f / (CSizeSquared * R);
		Cone.RightRayNormal = (P - Q) * DenominatorInverted;
		Cone.LeftRayNormal = (P + Q) * DenominatorInverted;
		Cone.RightRayOffset = -FVector2D::DotProduct(Cone.RightRayNormal, Vel);
		Cone.LeftRayOffset = -FVector2D::DotProduct(Cone.LeftRayNormal, Vel);

		// Time Horizon
		float CSize = FMath::Sqrt(CSizeSquared);
		FVector2D PTimeHorizon = ((CSize - R) / (CSize * Params.TauHorizon)) * C + Vel;
		Cone.TimeHorizonNormal = C / CSize;
		Cone.TimeHorizonOffset = -FVector2D::DotProduct(Cone.TimeHorizonNormal, PTimeHorizon);

		// Apexes
		float LDeterminantInverted = 1.f / (Cone.TimeHorizonNormal.X * Cone.LeftRayNormal.Y - Cone.TimeHorizonNormal.Y * Cone.LeftRayNormal.X);
		Cone.LeftRayApex = FVector2D(
			(Cone.TimeHorizonNormal.Y * Cone.LeftRayOffset - Cone.LeftRayNormal.Y * Cone.TimeHorizonOffset) * LDeterminantInverted,
			(Cone.LeftRayNormal.X * Cone.TimeHorizonOffset - Cone.TimeHorizonNormal.X * Cone.LeftRayOffset) * LDeterminantInverted
		);
		Cone.RightRayApex = PTimeHorizon + (PTimeHorizon - Cone.LeftRayApex);

		// Ray Directions
		Cone.LeftRayDir = FVector2D(-Cone.LeftRayNormal.Y, Cone.LeftRayNormal.X);
		Cone.RightRayDir = FVector2D(Cone.RightRayNormal.Y, -Cone.RightRayNormal.X);

		// Segments
		FVOSegment OutSegment;
		if (AvoidanceMath::TryFindSegmentOfRayInCircle(Cone.LeftRayApex, Cone.LeftRayNormal, Cone.LeftRayOffset, Cone.LeftRayDir,
			Params.MaxSpeed,
			&OutSegment))
		{
			Cone.bIsLeftRaySegmentValid = true;
			Cone.LeftRaySegment = OutSegment;
		}

		if (AvoidanceMath::TryFindSegmentOfRayInCircle(Cone.RightRayApex, Cone.RightRayNormal, Cone.RightRayOffset, Cone.RightRayDir,
			Params.MaxSpeed,
			&OutSegment))
		{
			Cone.bIsRightRaySegmentValid = true;
			Cone.RightRaySegment = OutSegment;
		}

		if (AvoidanceMath::TryFindSubSegmentInCircle(Cone.LeftRayApex, Cone.RightRayApex, Params.MaxSpeed, &OutSegment))
		{
			Cone.bIsTHSegmentValid = true;
			Cone.TimeHorizonSegment = OutSegment;
		}

		return Cone;
	}

	void BuildVOCones(const TArray<FVONeighborView>& Neis, const FVector& ActorPos, const FVOParams& Params, FVOConesSoA& OutVOCones)
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
			OutVOCones.Add(ComputeVOCone(R, pRel, FVector2D(N.Vel), Params));
		}
	}

	void CollectIntersections(const FVOConesSoA& VOCones, TArray<TArray<FVOConeIntersection>>& IntersectionsByRays)
	{
		const int32 NumRays = IntersectionsByRays.Num();

		for (int32 i = 0; i < NumRays; ++i)
		{
			const int32 ConeIdxI = i / 3;

			bool bIsSegmentIValid;

			switch (i % 3)
			{
			case 0:	 bIsSegmentIValid = VOCones.bIsLeftRaySegmentValid[ConeIdxI];	break;
			case 1:	 bIsSegmentIValid = VOCones.bIsRightRaySegmentValid[ConeIdxI];	break;
			default: bIsSegmentIValid = VOCones.bIsTHSegmentValid[ConeIdxI];		break;
			}

			if (!bIsSegmentIValid)
				continue;

			FVOSegment SegmentI;
			switch (i % 3)
			{
			case 0:  SegmentI = VOCones.LeftRaySegment[ConeIdxI];		break;
			case 1:  SegmentI = VOCones.RightRaySegment[ConeIdxI];		break;
			default: SegmentI = VOCones.TimeHorizonSegment[ConeIdxI];	break;
			}

			FVector2D CurRayDir;
			switch (i % 3)
			{
			case 0:  CurRayDir = VOCones.LeftRayDir[ConeIdxI];			break;
			case 1:  CurRayDir = VOCones.RightRayDir[ConeIdxI];			break;
			default: CurRayDir = FVector2D(VOCones.TimeHorizonNormal[ConeIdxI].Y, -VOCones.TimeHorizonNormal[ConeIdxI].X);	break;
			}

			// Add start and end points
			IntersectionsByRays[i].Add(FVOConeIntersection{ SegmentI.P1, true /* doesn't matter */, 0.f });
			IntersectionsByRays[i].Add(FVOConeIntersection{ SegmentI.P2, true /* doesn't matter */, 1.f });

			for (int32 j = 0; j < NumRays; ++j)
			{
				if (i / 3 == j / 3)
					continue;

				const int32 ConeIdxJ = j / 3;

				bool bIsSegmentJValid;
				switch (j % 3)
				{
				case 0:	 bIsSegmentJValid = VOCones.bIsLeftRaySegmentValid[ConeIdxJ];	break;
				case 1:	 bIsSegmentJValid = VOCones.bIsRightRaySegmentValid[ConeIdxJ];	break;
				default: bIsSegmentJValid = VOCones.bIsTHSegmentValid[ConeIdxJ];		break;
				}

				if (!bIsSegmentJValid)
					continue;

				FVOSegment SegmentJ;
				switch (j % 3)
				{
				case 0:  SegmentJ = VOCones.LeftRaySegment[ConeIdxJ];		break;
				case 1:  SegmentJ = VOCones.RightRaySegment[ConeIdxJ];		break;
				default: SegmentJ = VOCones.TimeHorizonSegment[ConeIdxJ];	break;
				}

				FVector2D NormalJ;
				switch (j % 3)
				{
				case 0:  NormalJ = VOCones.LeftRayNormal[ConeIdxJ];		break;
				case 1:  NormalJ = VOCones.RightRayNormal[ConeIdxJ];	break;
				default: NormalJ = VOCones.TimeHorizonNormal[ConeIdxJ];	break;
				}

				FVector2D	OutPoint;
				float		OutT;
				// Find intersections
				if (AvoidanceMath::SegmentIntersection2D(SegmentI.P1, SegmentI.P2, SegmentJ.P1, SegmentJ.P2, OutPoint, OutT))
				{
					const bool bIsFirst = FVector2D::DotProduct(CurRayDir, NormalJ) > 0.f;
					IntersectionsByRays[i].Add(FVOConeIntersection{ OutPoint, bIsFirst, OutT });
				}
			}
		}
	}

	void SortIntersectionsByRays(TArray<TArray<FVOConeIntersection>>& IntersectionsByRays)
	{
		for (int32 RayIdx = 0; RayIdx < IntersectionsByRays.Num(); ++RayIdx)
		{
			IntersectionsByRays[RayIdx].Sort([&](const FVOConeIntersection& A, const FVOConeIntersection& B)
			{
				return A.t < B.t;
			});
		}
	}

	int32 CountVOsForPoint(const FVOConesSoA& VOCones, int32 ConeIndexToSkip, const FVector2D& P)
	{
		int32 Count = 0;
		for (int32 i = 0; i < VOCones.Num(); ++i)
		{
			if (i == ConeIndexToSkip) continue;
			const float LeftDot = FVector2D::DotProduct(P - VOCones.Apex[i], VOCones.LeftRayNormal[i]);
			const float RightDot = FVector2D::DotProduct(P - VOCones.Apex[i], VOCones.RightRayNormal[i]);
			const float THDot = FVector2D::DotProduct(P - VOCones.LeftRayApex[i], VOCones.TimeHorizonNormal[i]);
			if (LeftDot >= 0.f && RightDot >= 0.f && THDot >= 0.f)
				++Count;
		}
		return Count;
	}

	void ClassifySegments(
		const FVOConesSoA& VOCones,
		const TArray<TArray<FVOConeIntersection>>& IntersectionsByRays,
		TArray<TArray<FVOOutsideSegment>>& OutsideSegmentsByRays)
	{
		for (int32 RayIdx = 0; RayIdx < IntersectionsByRays.Num(); ++RayIdx)
		{
			bool bIsSegmentValid;
			switch (RayIdx % 3)
			{
			case 0:	 bIsSegmentValid = VOCones.bIsLeftRaySegmentValid[RayIdx / 3];	break;
			case 1:	 bIsSegmentValid = VOCones.bIsRightRaySegmentValid[RayIdx / 3];	break;
			default: bIsSegmentValid = VOCones.bIsTHSegmentValid[RayIdx / 3];		break;
			}

			if (!bIsSegmentValid)
				continue;

			FVector2D FirstPoint = IntersectionsByRays[RayIdx][0].P;

			FVector2D Normal;
			switch (RayIdx % 3)
			{
			case 0:  Normal = VOCones.LeftRayNormal[RayIdx / 3];		break;
			case 1:  Normal = VOCones.RightRayNormal[RayIdx / 3];		break;
			default: Normal = VOCones.TimeHorizonNormal[RayIdx / 3];	break;
			}

			int32 CountOfVOs = CountVOsForPoint(VOCones, RayIdx / 3, FirstPoint);

			for (int32 j = 1; j < IntersectionsByRays[RayIdx].Num(); ++j)
			{
				if (CountOfVOs == 0)
				{
					const FVOOutsideSegment OutsideSegment = {
						IntersectionsByRays[RayIdx][j - 1].P,
						IntersectionsByRays[RayIdx][j].P,
						Normal,
					};
					OutsideSegmentsByRays[RayIdx].Add(OutsideSegment);
				}

				if (IntersectionsByRays[RayIdx][j].bIsFirst)
					++CountOfVOs;
				else
					CountOfVOs = FMath::Max(0, CountOfVOs - 1);
			}
		}
	}

	float ScoreVelocityCandidate(
		const FVector2D& V,
		const FVector2D& DesiredVel2D,
		const FVector2D& CurVel2D,
		const TArray<FVONeighborView>& Neis,
		const FVector& ActorPos,
		const FVOParams& Params)
	{
		const float MaxSpeed = FMath::Max(Params.MaxSpeed, 1e-2f);
		const float DesiredSize = DesiredVel2D.Size();
		const float VSize = V.Size();

		// Distance to DesiredVel2D (0..1)
		const float distToDesired = (DesiredVel2D - V).Size();
		const float proximity = 1.f - FMath::Clamp(distToDesired / MaxSpeed, 0.f, 1.f);

		// Angle between V and DesiredVel2D (0..1)
		float align = 0.5f;
		if (DesiredSize > KINDA_SMALL_NUMBER && VSize > KINDA_SMALL_NUMBER)
		{
			const float cosang = FVector2D::DotProduct(DesiredVel2D / DesiredSize, V / VSize); // -1..1
			align = 0.5f * (cosang + 1.f); // -> 0..1
		}

		// Speed Preference (0..1)
		const float speedPref = FMath::Clamp(VSize / MaxSpeed, 0.f, 1.f);

		// Acceleration Penalty (0..1)
		const float accelNorm = FMath::Clamp((V - CurVel2D).Size() / MaxSpeed, 0.f, 1.f);

		// Weights // TODO: Move to config
		const float WProximity = 1.0f;
		const float WAlign = 0.1f;
		const float WSpeed = 0.5f;
		const float WAccelPen = 0.3f;

		// Score
		const float score =
			WProximity * proximity
			+ WAlign * align
			+ WSpeed * speedPref
			- WAccelPen * accelNorm;

		return score;
	}

	FVector2D SelectBestVelocityFromOutsideSegments(
		const FVOCalculationContext& Ctx)
	{
		const FVector2D DesiredVel2D(Ctx.DesiredVelocity.X, Ctx.DesiredVelocity.Y);
		const FVector2D CurVel2D(Ctx.CurrentVelocity.X, Ctx.CurrentVelocity.Y);
		const auto& Neis = *Ctx.Neis;
		const FVector& ActorPos = Ctx.ActorPos;
		const FVOParams& Params = *Ctx.Params;
		const auto& OutsideSegmentsByRays = *Ctx.OutsideSegments;
		
		auto& OutCandidates = *Ctx.OutCandidates;
		int32& OutBestIdx = *Ctx.OutBestCandidateIdx;

		OutCandidates.Reset();
		OutBestIdx = -1;

		FVector2D bestV = FVector2D::ZeroVector;
		float bestScore = TNumericLimits<float>::Lowest();
		bool bFoundAny = false;

		const float MaxSpeed = FMath::Max(Params.MaxSpeed, 1e-2f);

		int32 candidateIdx = -1;

		for (int32 rayIdx = 0; rayIdx < OutsideSegmentsByRays.Num(); ++rayIdx)
		{
			const TArray<FVOOutsideSegment>& segs = OutsideSegmentsByRays[rayIdx];
			for (const FVOOutsideSegment& seg : segs)
			{
				const FVector2D candidates[2] = { seg.P1, seg.P2 };
				for (const FVector2D& cand : candidates)
				{
					OutCandidates.Add(cand);
					++candidateIdx;

					const float score = ScoreVelocityCandidate(cand, DesiredVel2D, CurVel2D, Neis, ActorPos, Params);
					if (score > bestScore)
					{
						bestScore = score;
						bestV = cand;
						bFoundAny = true;
						OutBestIdx = candidateIdx;
					}
				}
			}
		}

		if (!bFoundAny)
		{
			const FVector2D curClamped = CurVel2D.GetClampedToMaxSize(MaxSpeed);
			const FVector2D softDesired = DesiredVel2D.GetClampedToMaxSize(MaxSpeed * 0.75f);

			const int32 baseIdx = OutCandidates.Num();
			OutCandidates.Add(curClamped);
			OutCandidates.Add(softDesired);

			const float s1 = ScoreVelocityCandidate(curClamped, DesiredVel2D, CurVel2D, Neis, ActorPos, Params);
			const float s2 = ScoreVelocityCandidate(softDesired, DesiredVel2D, CurVel2D, Neis, ActorPos, Params);

			if (s2 > s1)
			{
				OutBestIdx = baseIdx + 1;
				return softDesired;
			}
			else
			{
				OutBestIdx = baseIdx;
				return curClamped;
			}
		}

		return bestV;
	}
}