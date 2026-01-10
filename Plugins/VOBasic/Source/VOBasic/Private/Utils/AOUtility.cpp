#include "Utils/AOUtility.h"

#include "Utils/AvoidanceMath.h"
#include "Core/VOManager.h"
#include "Settings/VOSettings.h"

// Private declarations
namespace
{
	enum class ERemoveInnerPointsResultType : uint8
	{
		None,
		SegmentIntersection,
		THIntersection,
		SinglePoint
	};

	struct RemoveSelfIntersectionsResult
	{
		ERemoveInnerPointsResultType ResultType = ERemoveInnerPointsResultType::None;
		int NumRemoved = 0;
		int FirstGoodPointInd = 0;
		int LastGoodPointInd = 0;
	};

	struct FAOConeBuilder
	{
		// Input
		const float R;
		const FVector2D C;
		const FVector2D Vel;
		const FVector2D Acc;
		const FVOParams& Params;
		const UVOSettings* Settings;

		// Buffers
		TArray<FVector2D> PointsL;
		TArray<FVector2D> PointsR;
		TArray<FVector2D> NormalsL;
		TArray<FVector2D> NormalsR;

		TArray<FVector2D> ScratchPoints;

		bool bIsConvexL = false;
		bool bIsConvexR = false;
		float LastValidT = 0.f;

		// Result
		FAOCone& OutCone;

		FAOConeBuilder(
			float InR,
			const FVector2D& InC,
			const FVector2D& InVel,
			const FVector2D& InAcc,
			const FVOParams& InParams,
			FAOCone& ConeRef)
			: R(InR), C(InC), Vel(InVel), Acc(InAcc), Params(InParams), OutCone(ConeRef)
		{
			Settings = UVOSettings::Get();

			int32 ReserveSize = Settings->NDiscreteIntervals + 2;
			PointsL.Reserve(ReserveSize);
			PointsR.Reserve(ReserveSize);
			NormalsL.Reserve(ReserveSize);
			NormalsR.Reserve(ReserveSize);
			ScratchPoints.Reserve(ReserveSize);
			LastValidT = Params.TauHorizon;
		}

		void SampleBoundaries();
		void BuildMinTimeSegment();
		void BuildTimeHorizonCap();
		void ResolveConvexityAndIntersections(int32& OutFanIndL, int32& OutFanIndR);
		bool ValidateShape();
		void Triangulate(int32 FanIndL, int32 FanIndR);
		void FinalizeSegments();

	private:
		void MakeConvexSide(TArray<FVector2D>& Points, const TArray<FVector2D>& Normals);
		bool TryFindSelfIntersections(
			const TArray<FVector2D>& InPoints,
			const TArray<FVector2D>& InNormals,
			const FVOSegment& CurrentTHSegment,
			RemoveSelfIntersectionsResult& OutResult);
		void ZipSides(const TArray<FVector2D>& SideA, const TArray<FVector2D>& SideB, int32 FanIndexA);
		void ConvertSideToSegments(const TArray<FVector2D>& Points, FAOSide& OutSide, bool bIsLeft);
	};

	// Pipeline
	void BuildCones(const FAOCalculationContext& Ctx);
	void ProcessIntersections(const FAOCalculationContext& Ctx);
	FVector2D SelectBestCandidate(const FAOCalculationContext& Ctx);

	// Cone computing
	FAOCone ComputeAOCone(
		const float R,
		const FVector2D& C,
		const FVector2D& Vel,
		const FVector2D& Acc,
		const FVOParams& Params);
	TArray<FVector2D> BuildConvexSide(const TArray<FVector2D>& Points, const TArray<FVector2D>& Normals);
	bool TryFindSelfIntersections(
		const TArray<FVector2D>& Points,
		const TArray<FVector2D>& Normals,
		const FVOSegment& TimeHorizonSegment,
		RemoveSelfIntersectionsResult& OutResult);

	// Intersection processing
	void PrepareAndSortWorkSegments(
		const FAOConesSoA& InCones,
		TArray<FAOWorkSegment>& OutWorkSegments,
		int32& OutTotalSides);
	void CollectIntersections(
		const TArray<FAOWorkSegment>& WorkSegments,
		TArray<TArray<FAOConeIntersection>>& OutSideIntersections);
	void SortSideIntersections(TArray<TArray<FAOConeIntersection>>& SideIntersections);
	void ClassifySegments(
		const FAOConesSoA& Cones,
		const TArray<TArray<FAOConeIntersection>>& SideIntersections,
		TArray<TArray<FAOSegment>>& OutOutsideSegments);

	// Finding best candidate
	float ScoreAccelerationCandidate(
		const FVector2D& CandidateAcc,
		const FVector2D& DesiredAcc,
		const FVector2D& CurAcc,
		const TArray<FVONeighborView>& Neis,
		const FVector& ActorPos,
		const FVOParams& Params);

	bool IsPointInsideAO(const FAOConesSoA& Cones, int32 ConeIdx, const FVector2D& P);
	bool IsPointInsideAnyAO(const FAOConesSoA& Cones, const FVector2D& P, int32 IgnoreConeIdx);
	int32 CountAOsForPoint(const FAOConesSoA& Cones, int32 ConeIndexToSkip, const FVector2D& P);
	FParabolaResult FindParabolaIntersection(
		const FAOParabola& Curve,
		const FAOCircle& C1,
		const FAOCircle& C2,
		int32 DiscSegments);
}

namespace AOUtility
{
	FVector ComputeAcceleration(const FAOCalculationContext& Ctx)
	{
		if (Ctx.Cones) Ctx.Cones->Reset();

		BuildCones(Ctx);
		ProcessIntersections(Ctx);
		FVector2D Best = SelectBestCandidate(Ctx);
		return FVector(Best.X, Best.Y, 0.f);
	}

	void DrawAOCones(const UVOFollowingComponent* Comp, const FAOConesSoA& Cones)
	{
		UWorld* W = Comp->GetWorld();
		if (!W) return;

		FVector P = Comp->GetOwnerLocation();

		for (int32 i = 0; i < Cones.Num(); ++i)
		{
			FColor Color = AvoidanceMath::GetColorFromSeed(i);
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

	void DrawAOOutsideSegments(
		const UVOFollowingComponent* Comp,
		const TArray<TArray<FAOSegment>>& OutsideSegments)
	{
		UWorld* W = Comp->GetWorld();
		if (!W) return;

		const FVector P = Comp->GetOwnerLocation();

		for (int32 i = 0; i < OutsideSegments.Num(); ++i)
		{
			const TArray<FAOSegment>& SegList = OutsideSegments[i];
			FColor Color = AvoidanceMath::GetColorFromSeed(i / 3);
			for (const FAOSegment& Seg : SegList)
			{
				FVector Start(Seg.P1.X, Seg.P1.Y, 0.f);
				FVector End(Seg.P2.X, Seg.P2.Y, 0.f);

				DrawDebugLine(W, P + Start, P + End, Color, true, -1.f, 0, 3.0f);
			}
		}
	}

	void DrawAccelConstraints(
		const UVOFollowingComponent* Comp,
		const FVector& CurrentVelocity,
		float MaxSpeed,
		float MaxAccel,
		float Tau)
	{
		UWorld* W = Comp->GetWorld();
		if (!W) return;

		const FVector P = Comp->GetOwnerLocation();
		const FVector AxisX(1.f, 0.f, 0.f);
		const FVector AxisY(0.f, 1.f, 0.f);

		// 1. Max Acceleration Circle (Always Full)
		// Radius = MaxAccel
		DrawDebugCircle(W, P, MaxAccel, 48, FColor::Orange, true, -1.f, 0, 1.5f, AxisY, AxisX);

		// 2. Collision Free Circle (Clipped)
		// Constraint: |V_new| <= V_max
		// |V_curr + A * t| <= V_max
		// |A - (-V_curr/t)| <= V_max/t
		
		float Ta = FMath::Max(Tau, 0.01f);
		FVector2D C2 = FVector2D(CurrentVelocity) * (-1.f / Ta);
		float R2 = MaxSpeed / Ta;
		float R2Sq = R2 * R2;
		float R1Sq = MaxAccel * MaxAccel; // Max Accel constraint (Circle 1)

		const int32 Segments = 64;
		const float AngleStep = 2.f * PI / Segments;
		const FColor SafeColor = FColor::Green;

		FVector2D PrevPt;
		bool bPrevValid = false;

		{
			float SinA, CosA;
			FMath::SinCos(&SinA, &CosA, 0.f);
			PrevPt = C2 + FVector2D(CosA * R2, SinA * R2);
			bPrevValid = (PrevPt.SizeSquared() <= R1Sq + 1.f); // +1.f tolerance
		}

		for (int32 i = 1; i <= Segments; ++i)
		{
			float Angle = i * AngleStep;
			float SinA, CosA;
			FMath::SinCos(&SinA, &CosA, Angle);

			FVector2D CurrLoc = C2 + FVector2D(CosA * R2, SinA * R2);
			
			// Check if inside MaxAccel circle (Radius = MaxAccel, Center = 0,0)
			bool bValid = (CurrLoc.SizeSquared() <= R1Sq + 1.f);

			if (bValid && bPrevValid)
			{
				FVector WorldP1 = P + FVector(PrevPt.X, PrevPt.Y, 0.f);
				FVector WorldP2 = P + FVector(CurrLoc.X, CurrLoc.Y, 0.f);
				DrawDebugLine(W, WorldP1, WorldP2, SafeColor, true, -1.f, 0, 2.0f);
			}

			PrevPt = CurrLoc;
			bPrevValid = bValid;
		}
	}
}

// Private implementations
namespace
{
	void BuildCones(const FAOCalculationContext& Ctx)
	{
		const auto& Neis = *Ctx.Neis;
		const auto& Params = *Ctx.Params;
		const FVector& ActorPos = Ctx.ActorPos;
		const FVector& ActorVel = Ctx.CurrentVelocity;
		FAOConesSoA& OutCones = *Ctx.Cones;

		for (const FVONeighborView& N : Neis)
		{
			const float R = Params.AgentRadius + N.Radius;
			const FVector2D pRel(N.Pos.X - ActorPos.X, N.Pos.Y - ActorPos.Y);

			if (R * R >= pRel.SizeSquared())
			{
				continue;
			}

			const FVector2D vRel(ActorVel.X - N.Vel.X, ActorVel.Y - N.Vel.Y);
			OutCones.Add(ComputeAOCone(R, pRel, vRel, FVector2D(N.Acc), Params));
		}
	}

	void ProcessIntersections(const FAOCalculationContext& Ctx)
	{
		auto& Cones = *Ctx.Cones;
		auto& WorkSegments = *Ctx.WorkSegments;
		auto& SideIntersections = *Ctx.SideIntersections;
		auto& OutsideSegments = *Ctx.OutsideSegments;

		int32 TotalSides = 0;

		PrepareAndSortWorkSegments(Cones, WorkSegments, TotalSides);

		SideIntersections.SetNum(TotalSides);
		for (int32 i = 0; i < TotalSides; ++i)
			SideIntersections[i].Reset();

		CollectIntersections(WorkSegments, SideIntersections);
		SortSideIntersections(SideIntersections);
		ClassifySegments(Cones, SideIntersections, OutsideSegments);
	}

	FVector2D SelectBestCandidate(const FAOCalculationContext& Ctx)
	{
		const FVector2D CurAcc = (Ctx.Comp) ? FVector2D(Ctx.Comp->GetCachedAcceleration()) : FVector2D::ZeroVector;
		const auto& Neis = *Ctx.Neis;
		const auto& Params = *Ctx.Params;
		const auto& Cones = *Ctx.Cones;
		const auto& OutsideSegmentsByRays = *Ctx.OutsideSegments;
		const FVector& ActorPos = Ctx.ActorPos;
		const FVector& TargetPos = Ctx.TargetPos;
		const FVector& CurVel = Ctx.CurrentVelocity;

		// TODO: Move to function
		FVector2D DesiredAcc = FVector2D::ZeroVector;
		FVector2D TargetPosRel = FVector2D(TargetPos - ActorPos); // For parabola
		if (!TargetPosRel.IsZero())
		{
			FVector2D VelRel = FVector2D(CurVel); // For parabola
			FVector2D TargetAcc = FVector2D::Zero(); // For parabola
			double MaxAcc = Params.MaxAcceleration; // For circle 1
			// CurVel for circle 2

			FAOParabola Parabola;
			Parabola.A = TargetPosRel * 2.0;
			Parabola.B = VelRel * -2.0;
			Parabola.C = TargetAcc;

			FAOCircle C1;
			C1.Center = FVector2D::ZeroVector;
			C1.R = Params.MaxAcceleration;
			C1.RSq = C1.R * C1.R;

			// (Acc + V0/ta)^2 <= (Vmax/ta)^2
			// Center = -V0 / ta
			// Radius = Vmax / ta
			double Ta = /*FMath::Max(Params.TauHorizon, 0.01f)*/1.5f;
			FAOCircle C2;
			C2.Center = FVector2D(CurVel) * (-1.0 / Ta);
			C2.R = Params.MaxSpeed / Ta;
			C2.RSq = C2.R * C2.R;

			FParabolaResult Result = FindParabolaIntersection(Parabola, C1, C2, 16);

			if (Result.bFound)
			{
				DesiredAcc = Result.Point;
			}
		}

		auto& OutCandidates = *Ctx.OutCandidates;
		int32& OutBestIdx = *Ctx.OutBestCandidateIdx;

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

		if (CountAOsForPoint(Cones, -1, FVector2D::ZeroVector) == 0)
			EvaluatePoint(FVector2D::ZeroVector);

		if (CountAOsForPoint(Cones, -1, DesiredAcc) == 0)
			EvaluatePoint(DesiredAcc);

		if (CountAOsForPoint(Cones, -1, CurAcc) == 0)
			EvaluatePoint(CurAcc);

		for (const TArray<FAOSegment>& SegList : OutsideSegmentsByRays)
		{
			for (const FAOSegment& Seg : SegList)
			{
				EvaluatePoint(Seg.P1);
				EvaluatePoint(Seg.P2);

				FVector2D SegDir = Seg.P2 - Seg.P1;
				float SegLenSq = SegDir.SizeSquared();
				if (SegLenSq > KINDA_SMALL_NUMBER)
				{
					float t = FVector2D::DotProduct(DesiredAcc - Seg.P1, SegDir) / SegLenSq;
					if (t > 0.01f && t < 0.99f)
					{
						EvaluatePoint(Seg.P1 + SegDir * t);
					}
				}
			}
		}

		// TODO: Add points of segments in candidates

		if (!bFoundAny)
		{
			EvaluatePoint(FVector2D::ZeroVector);
			return FVector2D::ZeroVector;
		}

		return BestV;
	}


	FAOCone ComputeAOCone(
		const float R,
		const FVector2D& C,
		const FVector2D& Vel,
		const FVector2D& Acc,
		const FVOParams& Params)
	{
		FAOCone Cone;

		FAOConeBuilder Builder(R, C, Vel, Acc, Params, Cone);

		Builder.SampleBoundaries();
		Builder.BuildMinTimeSegment();

		if (Builder.PointsL.Num() == 0)
			return Cone;

		Builder.BuildTimeHorizonCap();

		int32 FanIndL = -1, FanIndR = -1;
		Builder.ResolveConvexityAndIntersections(FanIndL, FanIndR);

		if (Builder.ValidateShape())
		{
			Builder.Triangulate(FanIndL, FanIndR);
			Builder.FinalizeSegments();
		}

		return Cone;
	}

	void FAOConeBuilder::SampleBoundaries()
	{
		float t = Settings->MinimalReactionTime;
		const float t_interval = (Params.TauHorizon - Settings->MinimalReactionTime) / Settings->NDiscreteIntervals;

		for (int i = 0; i < Settings->NDiscreteIntervals; ++i)
		{
			const float InvT = 1.f / t;
			const float InvSqrT = InvT * InvT;

			const FVector2D CenterLineP = 2 * C * InvSqrT - 2 * Vel * InvT + Acc;
			const FVector2D GrazeSourceP = -Vel * InvT + Acc;
			const FVector2D GrazeToCenterOffset = GrazeSourceP - CenterLineP;

			const float GrazeToCenterLengthSqr = GrazeToCenterOffset.SizeSquared();
			const float RTimed = 2 * R * InvSqrT;
			const float DistanceSqr = GrazeToCenterLengthSqr - RTimed * RTimed;

			if (DistanceSqr < 0.f)
			{
				LastValidT = t;
				break;
			}

			const float InvGrazeLenSqr = 1.f / GrazeToCenterLengthSqr;
			const float RTimedOverGrazeToCenterLenSqr = RTimed * InvGrazeLenSqr;

			const FVector2D b = CenterLineP + (RTimed * RTimedOverGrazeToCenterLenSqr) * GrazeToCenterOffset;
			const float k_h = RTimedOverGrazeToCenterLenSqr * FMath::Sqrt(DistanceSqr);
			const FVector2D c(-GrazeToCenterOffset.Y * k_h, GrazeToCenterOffset.X * k_h);

			const FVector2D PointL = b + c;
			const FVector2D PointR = b - c;

			PointsL.Add(PointL);
			PointsR.Add(PointR);


			if (i == 0)
			{
				if (GrazeSourceP.X * PointL.Y - GrazeSourceP.Y * PointL.X > 0.f) bIsConvexL = true;
				if (GrazeSourceP.X * PointR.Y - GrazeSourceP.Y * PointR.X < 0.f) bIsConvexR = true;
			}

			const FVector2D DirGrazeL = PointL - GrazeSourceP;
			NormalsL.Add({-DirGrazeL.Y, DirGrazeL.X});

			const FVector2D DirGrazeR = PointR - GrazeSourceP;
			NormalsR.Add({DirGrazeR.Y, -DirGrazeR.X});

			t += t_interval;
		}

		if (PointsL.Num() == Settings->NDiscreteIntervals)
		{
			LastValidT -= t_interval;
		}
	}

	void FAOConeBuilder::BuildMinTimeSegment()
	{
		if (PointsL.Num() > 0 && PointsR.Num() > 0)
		{
			const FVector2D P_L_Start = PointsL[0];
			const FVector2D P_R_Start = PointsR[0];
			const FVector2D Dir = P_R_Start - P_L_Start;
			const float DirSq = Dir.SizeSquared();

			FVector2D SegNormal;
			if (DirSq > KINDA_SMALL_NUMBER)
			{
				const float InvLen = FMath::InvSqrt(DirSq);
				const FVector2D DirNorm = Dir * InvLen;
				const FVector2D TempNormal(DirNorm.Y, -DirNorm.X);

				const FVector2D ExpansionDir = (PointsL.Num() > 1) ? (PointsL[1] - PointsL[0]) : Vel;
				SegNormal = (FVector2D::DotProduct(TempNormal, ExpansionDir) > 0) ? -TempNormal : TempNormal;
			}
			else
			{
				SegNormal = -Vel.GetSafeNormal();
			}

			OutCone.MinTimeSegment.Init(P_L_Start, P_R_Start, SegNormal);
			OutCone.isMinTimeSegmentValid = (DirSq > KINDA_SMALL_NUMBER);
		}
		else
		{
			OutCone.isMinTimeSegmentValid = false;
		}
	}

	void FAOConeBuilder::BuildTimeHorizonCap()
	{
		float t = LastValidT;

		const float InvT = 1.f / t;
		const float InvSqrT = InvT * InvT;

		const FVector2D CenterLineP = 2 * C * InvSqrT - 2 * Vel * InvT + Acc;
		const FVector2D GrazeSourceP = -Vel * InvT + Acc;
		const FVector2D GrazeToCenterOffset = GrazeSourceP - CenterLineP;

		const float GrazeToCenterLengthSqr = GrazeToCenterOffset.SizeSquared();
		const float RTimed = 2 * R * InvSqrT;

		const float RTimedOverGrazeToCenterLen = RTimed * FMath::InvSqrt(GrazeToCenterLengthSqr);
		const FVector2D GrazeOffsetFromCenter = RTimedOverGrazeToCenterLen * GrazeToCenterOffset;

		const FVector2D TimeHorizonGrazePoint = CenterLineP + GrazeOffsetFromCenter;

		// Normalize negate optimization
		FVector2D TimeHorizonGrazeNormal = GrazeOffsetFromCenter;
		TimeHorizonGrazeNormal.Normalize();
		TimeHorizonGrazeNormal *= -1.f;

		const float TimeHorizonC = -TimeHorizonGrazeNormal.Dot(TimeHorizonGrazePoint);

		float outT = 0.f;
		FVector2D TimeHorizonL = FVector2D::ZeroVector;

		if (AvoidanceMath::FindLineAndSegmentIntersection(
			TimeHorizonGrazeNormal,
			TimeHorizonC,
			GrazeSourceP,
			PointsL.Last(),
			outT,
			TimeHorizonL))
		{
			PointsL.Add(TimeHorizonL);
			const FVector2D LastNormL = NormalsL.Last();
			NormalsL.Add(LastNormL);

			const FVector2D TimeHorizonR = TimeHorizonL + 2 * (TimeHorizonGrazePoint - TimeHorizonL);
			PointsR.Add(TimeHorizonR);
			const FVector2D LastNormR = NormalsR.Last();
			NormalsR.Add(LastNormR);
		}

		OutCone.TimeHorizonSegment.Init(PointsL.Last(), PointsR.Last(), TimeHorizonGrazeNormal);
		OutCone.isTHSegmentValid = FVector2D::DistSquared(OutCone.TimeHorizonSegment.P1, OutCone.TimeHorizonSegment.P2)
			> KINDA_SMALL_NUMBER;
	}

	void FAOConeBuilder::ResolveConvexityAndIntersections(int32& OutFanIndL, int32& OutFanIndR)
	{
		// 1. Build Convex Hull if needed
		if (bIsConvexL) MakeConvexSide(PointsL, NormalsL);
		if (bIsConvexR) MakeConvexSide(PointsR, NormalsR);

		// 2. Remove Self Intersections (Butterfly effect)
		if (FVector2D::DotProduct(Vel, C) > 0.f)
		{
			auto ProcessSide = [&](
				TArray<FVector2D>& Pts,
				const TArray<FVector2D>& Nrms,
				bool bIsLeft,
				int32& OutFanIdx)
			{
				if (bIsLeft && bIsConvexL) return;
				if (!bIsLeft && bIsConvexR) return;

				RemoveSelfIntersectionsResult Res;
				FVOSegment THSeg = FVOSegment(OutCone.TimeHorizonSegment.P1, OutCone.TimeHorizonSegment.P2);
				if (TryFindSelfIntersections(Pts, Nrms, THSeg, Res))
				{
					switch (Res.ResultType)
					{
					case ERemoveInnerPointsResultType::THIntersection:
						{
							const FVector2D& NewPoint = Pts[Res.LastGoodPointInd];
							if (bIsLeft) OutCone.TimeHorizonSegment.P1 = NewPoint;
							else OutCone.TimeHorizonSegment.P2 = NewPoint;

							Pts.SetNum(Res.LastGoodPointInd + 1);
							OutFanIdx = Res.LastGoodPointInd;
						}
						break;
					case ERemoveInnerPointsResultType::SegmentIntersection:
						Pts.RemoveAt(Res.LastGoodPointInd + 1, Res.NumRemoved);
						OutFanIdx = Res.LastGoodPointInd + 1;
						break;
					case ERemoveInnerPointsResultType::SinglePoint:
						Pts.RemoveAt(Res.FirstGoodPointInd - 1);
						OutFanIdx = Res.FirstGoodPointInd;
						break;
					default: break;
					}
				}
			};

			ProcessSide(PointsL, NormalsL, true, OutFanIndL);
			ProcessSide(PointsR, NormalsR, false, OutFanIndR);
		}
	}

	void FAOConeBuilder::MakeConvexSide(TArray<FVector2D>& Points, const TArray<FVector2D>& Normals)
	{
		if (Points.Num() < 3) return;

		ScratchPoints.Reset();
		ScratchPoints.Add(Points[0]);
		float LinePrevC = -FVector2D::DotProduct(Normals[0], Points[0]);
		const int32 Count = Points.Num();

		for (int i = 1; i < Count - 1; ++i)
		{
			const FVector2D& NormCurrent = Normals[i];
			const FVector2D& NormPrev = Normals[i - 1];

			float LineC = -FVector2D::DotProduct(NormCurrent, Points[i]);
			float DenomGraze = NormPrev.X * NormCurrent.Y - NormPrev.Y * NormCurrent.X;

			if (FMath::Abs(DenomGraze) < KINDA_SMALL_NUMBER)
			{
				ScratchPoints.Add((Points[i] + Points[i - 1]) * 0.5f);
				LinePrevC = LineC;
				continue;
			}

			float DenomGrazeInv = 1.f / DenomGraze;

			float X = (NormPrev.Y * LineC - NormCurrent.Y * LinePrevC) * DenomGrazeInv;
			float Y = (NormCurrent.X * LinePrevC - NormPrev.X * LineC) * DenomGrazeInv;

			ScratchPoints.Add(FVector2D(X, Y));
			LinePrevC = LineC;
		}

		ScratchPoints.Add(Points.Last());

		Swap(Points, ScratchPoints);
	}

	bool FAOConeBuilder::TryFindSelfIntersections(
		const TArray<FVector2D>& InPoints,
		const TArray<FVector2D>& InNormals,
		const FVOSegment& CurrentTHSegment,
		RemoveSelfIntersectionsResult& OutResult)
	{
		OutResult = RemoveSelfIntersectionsResult();
		bool bFoundPossibleResult = false;

		const int N = InPoints.Num();
		for (int i = N - 3; i >= 0; --i)
		{
			FVector2D IntersectionPoint;
			const int CurIdx = i;
			const int PrevIdx = i + 1;

			// 1. Intersection with TH Segment
			if (AvoidanceMath::SegmentIntersection2D(
				CurrentTHSegment.P1,
				CurrentTHSegment.P2,
				InPoints[PrevIdx],
				InPoints[CurIdx],
				IntersectionPoint))
			{
				OutResult = {ERemoveInnerPointsResultType::THIntersection, N - PrevIdx, -1, CurIdx};
				return true;
			}

			// 2. Intersection with previous segments of the same side
			for (int j = N - 2; j > PrevIdx; --j)
			{
				if (AvoidanceMath::SegmentIntersection2D(
					InPoints[j + 1],
					InPoints[j],
					InPoints[PrevIdx],
					InPoints[CurIdx],
					IntersectionPoint))
				{
					OutResult = {ERemoveInnerPointsResultType::SegmentIntersection, j - i, j + 1, i};
					return true;
				}
			}

			// 3. "Kink" check
			if (!bFoundPossibleResult)
			{
				if (CurIdx > 0)
				{
					FVector2D SegDir = InPoints[i - 1] - InPoints[i];
					if (FVector2D::DotProduct(InNormals[i], SegDir) > 0.01f)
					{
						OutResult = {ERemoveInnerPointsResultType::SinglePoint, 1, i, CurIdx > 1 ? i - 2 : i};
						bFoundPossibleResult = true;
					}
				}

				if (!bFoundPossibleResult)
				{
					FVector2D SegDirNext = InPoints[i + 1] - InPoints[i];
					if (FVector2D::DotProduct(InNormals[i], SegDirNext) > 0.01f)
					{
						OutResult = {ERemoveInnerPointsResultType::SinglePoint, 1, i + 2, i};
						bFoundPossibleResult = true;
					}
				}
			}
		}

		return bFoundPossibleResult;
	}

	// TODO: Normal validation
	bool FAOConeBuilder::ValidateShape()
	{
		bool bIsValid = false;

		auto CheckSide = [&](const TArray<FVector2D>& SidePoints) -> bool
		{
			if (SidePoints.Num() < 2) return false;
			FVector2D Prev = SidePoints.Last();
			for (int i = SidePoints.Num() - 2; i >= 0; --i)
			{
				FVOSegment Seg;
				// Params captured from context
				if (AvoidanceMath::TryFindSubSegmentInCircle(Prev, SidePoints[i], Params.MaxAcceleration, &Seg))
				{
					return true;
				}
				Prev = SidePoints[i];
			}
			return true;
		};

		if (CheckSide(PointsL)) bIsValid = true;
		else if (CheckSide(PointsR)) bIsValid = true;

		if (bIsValid)
		{
			OutCone.isLeftSideValid = true;
			OutCone.isRightSideValid = true;
			// isTHSegmentValid already set in BuildTimeHorizonCap
		}
		return bIsValid;
	}

	void FAOConeBuilder::Triangulate(int32 FanIndL, int32 FanIndR)
	{
		if (!bIsConvexL && bIsConvexR)
		{
			ZipSides(PointsL, PointsR, FanIndL);
		}
		else if (!bIsConvexR && bIsConvexL)
		{
			ZipSides(PointsR, PointsL, FanIndR);
		}
		else
		{
			ZipSides(PointsR, PointsL, -1);
		}
	}

	void FAOConeBuilder::ZipSides(const TArray<FVector2D>& SideA, const TArray<FVector2D>& SideB, int32 FanIndexA)
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
					// Pre-allocate tris if possible or just Add
					for (int32 k = 0; k < Diff; ++k)
					{
						OutCone.Tris.Add({SideA[TopA], SideB[IdxB], SideB[IdxB - 1]});
						IdxB--;
					}
				}
			}

			if (IdxB > 0)
			{
				OutCone.Quads.Add({SideA[TopA], SideA[BotA], SideB[IdxB - 1], SideB[IdxB]});
				IdxB--;
			}
		}
	}

	void FAOConeBuilder::FinalizeSegments()
	{
		if (OutCone.isLeftSideValid && PointsL.Num() > 1)
		{
			ConvertSideToSegments(PointsL, OutCone.LeftSide, true);
		}

		if (OutCone.isRightSideValid && PointsR.Num() > 1)
		{
			ConvertSideToSegments(PointsR, OutCone.RightSide, false);
		}
	}

	void FAOConeBuilder::ConvertSideToSegments(const TArray<FVector2D>& Points, FAOSide& OutSide, bool bIsLeft)
	{
		OutSide.Segments.Reset(Points.Num() - 1);

		for (int32 i = 0; i < Points.Num() - 1; ++i)
		{
			const FVector2D& P1 = Points[i];
			const FVector2D& P2 = Points[i + 1];

			// Fast dist check
			float DSq = FVector2D::DistSquared(P1, P2);
			if (DSq < KINDA_SMALL_NUMBER) continue;

			// Normalize manually to reuse DSq
			float InvLen = FMath::InvSqrt(DSq);
			FVector2D Dir = (P2 - P1) * InvLen;

			FVector2D Normal = bIsLeft ? FVector2D(-Dir.Y, Dir.X) : FVector2D(Dir.Y, -Dir.X);

			FAOSegment NewSeg;
			NewSeg.Init(P1, P2, Normal);
			OutSide.Segments.Add(NewSeg);
		}
	}

	void PrepareAndSortWorkSegments(
		const FAOConesSoA& InCones,
		TArray<FAOWorkSegment>& OutWorkSegments,
		int32& OutTotalSides)
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
		OutWorkSegments.Sort([](const FAOWorkSegment& A, const FAOWorkSegment& B)
		{
			return A.SegmentRef->MinX < B.SegmentRef->MinX;
		});
	}

	void CollectIntersections(
		const TArray<FAOWorkSegment>& WorkSegments,
		TArray<TArray<FAOConeIntersection>>& OutSideIntersections)
	{
		const int32 NumWork = WorkSegments.Num();

		for (int32 i = 0; i < NumWork; ++i)
		{
			const FAOWorkSegment& W = WorkSegments[i];
			int32 FlatSideIdx = W.ConeIdx * 4 + W.SideIdx;

			OutSideIntersections[FlatSideIdx].Add({W.SegmentRef->P1, 0.f, W.SegIdx, false, false});
			OutSideIntersections[FlatSideIdx].Add({W.SegmentRef->P2, 1.f, W.SegIdx, false, false});
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
				if (AvoidanceMath::SegmentIntersection2D(SegA.P1, SegA.P2, SegB.P1, SegB.P2, IntP))
				{
					FVector2D DirA = (SegA.P2 - SegA.P1);
					float LenSqA = DirA.SizeSquared();
					float tA = (LenSqA > KINDA_SMALL_NUMBER)
						           ? FVector2D::DotProduct(IntP - SegA.P1, DirA) / LenSqA
						           : 0.f;

					FVector2D DirB = (SegB.P2 - SegB.P1);
					float LenSqB = DirB.SizeSquared();
					float tB = (LenSqB > KINDA_SMALL_NUMBER)
						           ? FVector2D::DotProduct(IntP - SegB.P1, DirB) / LenSqB
						           : 0.f;

					tA = FMath::Clamp(tA, 0.f, 1.f);
					tB = FMath::Clamp(tB, 0.f, 1.f);

					bool bEntryA = FVector2D::DotProduct(DirA, SegB.OutsideNormal) < 0.f;
					bool bEntryB = FVector2D::DotProduct(DirB, SegA.OutsideNormal) < 0.f;

					int32 SideIdxA = WA.ConeIdx * 4 + WA.SideIdx;
					OutSideIntersections[SideIdxA].Add({IntP, tA, WA.SegIdx, bEntryA, true});

					int32 SideIdxB = WB.ConeIdx * 4 + WB.SideIdx;
					OutSideIntersections[SideIdxB].Add({IntP, tB, WB.SegIdx, bEntryB, true});
				}
			}
		}
	}

	void SortSideIntersections(TArray<TArray<FAOConeIntersection>>& SideIntersections)
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

	void ClassifySegments(
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
						if (SideType == 0)
							SegNormal = Cones.LeftSide[ConeIdx].Segments[Prev.SegmentIndex].
								OutsideNormal;
						else if (SideType == 1)
							SegNormal = Cones.RightSide[ConeIdx].Segments[Prev.SegmentIndex].
								OutsideNormal;
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

	FParabolaResult FindParabolaIntersection(
		const FAOParabola& Curve,
		const FAOCircle& C1,
		const FAOCircle& C2,
		int32 DiscSegments)
	{
		FParabolaResult Result;
		Result.U = -1.0;

		// Centers distance
		double DistSq = FVector2D::DistSquared(C1.Center, C2.Center);
		double Dist = FMath::Sqrt(DistSq);

		// 1. INtersection checks
		if (Dist > C1.R + C2.R + KINDA_SMALL_NUMBER)
		{
			return Result; // bFound = false
		}

		auto ProcessArc = [&](
			const FAOCircle& TargetC,
			const FAOCircle& ConstraintC,
			const FVector2D& PStart,
			const FVector2D& PEnd,
			bool bFullCircle,
			int32 SegmentsCount)
		{
			double TotalAngle;
			FVector2D VStart;

			if (bFullCircle)
			{
				TotalAngle = 2.0 * PI;
				VStart = FVector2D(TargetC.R, 0.0);
			}
			else
			{
				VStart = PStart - TargetC.Center;
				double Ang1 = FMath::Atan2(VStart.Y, VStart.X);
				double Ang2 = FMath::Atan2(PEnd.Y - TargetC.Center.Y, PEnd.X - TargetC.Center.X);

				TotalAngle = Ang2 - Ang1;
				if (TotalAngle <= -PI) TotalAngle += 2.0 * PI;
				else if (TotalAngle > PI) TotalAngle -= 2.0 * PI;

				// Check direction
				double MidAngle = Ang1 + TotalAngle * 0.5;
				FVector2D MidPt = FVector2D(
					TargetC.Center.X + TargetC.R * FMath::Cos(MidAngle),
					TargetC.Center.Y + TargetC.R * FMath::Sin(MidAngle));

				// If wrong - reverse
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
				int32 Roots = AvoidanceMath::SolveQuadratic(QA, QB, QC, U1, U2);

				if (Roots > 0)
				{
					auto CheckPoint = [&](double U)
					{
						if (U > Result.U)
						{
							FVector2D P = Curve.Eval(U);
							
							bool bInConstraint = bFullCircle || ((P - ConstraintC.Center).SizeSquared() <= ConstraintC.
								RSq + 0.1);

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

		// 2. Nesting check
		bool bC2inC1 = Dist + C2.R <= C1.R + KINDA_SMALL_NUMBER;
		bool bC1inC2 = Dist + C1.R <= C2.R + KINDA_SMALL_NUMBER;

		if (bC2inC1)
		{
			// C2 inside C1
			ProcessArc(C2, C1, FVector2D::ZeroVector, FVector2D::ZeroVector, true, DiscSegments * 2);
			return Result;
		}

		if (bC1inC2)
		{
			// C1 inside C2
			ProcessArc(C1, C2, FVector2D::ZeroVector, FVector2D::ZeroVector, true, DiscSegments * 2);
			return Result;
		}

		// 3. Intersection
		FVector2D Int1, Int2;
		if (AvoidanceMath::FindCircleCircleIntersections(C1, C2, Int1, Int2))
		{
			ProcessArc(C1, C2, Int1, Int2, false, DiscSegments);
			ProcessArc(C2, C1, Int1, Int2, false, DiscSegments);
		}

		return Result;
	}

	bool IsPointInsideAO(const FAOConesSoA& Cones, int32 ConeIdx, const FVector2D& P)
	{
		const TArray<FAOTriangle>& Tris = Cones.Tris[ConeIdx];
		for (const FAOTriangle& Tri : Tris)
		{
			if (AvoidanceMath::IsPointInTriangle(P, Tri.P1, Tri.P2, Tri.P3))
			{
				return true;
			}
		}

		const TArray<FAOQuad>& Quads = Cones.Quads[ConeIdx];
		for (const FAOQuad& Quad : Quads)
		{
			// TODO: Check without triangulation

			// Tri 1: P1-P2-P3
			if (AvoidanceMath::IsPointInTriangle(P, Quad.P1, Quad.P2, Quad.P3))
			{
				return true;
			}
			// Tri 2: P1-P3-P4
			if (AvoidanceMath::IsPointInTriangle(P, Quad.P1, Quad.P3, Quad.P4))
			{
				return true;
			}
		}

		return false;
	}

	bool IsPointInsideAnyAO(const FAOConesSoA& Cones, const FVector2D& P, int32 IgnoreConeIdx)
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

	int32 CountAOsForPoint(const FAOConesSoA& Cones, int32 ConeIndexToSkip, const FVector2D& P)
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

	float ScoreAccelerationCandidate(
		const FVector2D& CandidateAcc,
		const FVector2D& DesiredAcc,
		const FVector2D& CurAcc,
		const TArray<FVONeighborView>& Neis,
		const FVector& ActorPos,
		const FVOParams& Params)
	{
		const float W_Proximity = 1.0f;
		const float W_Effort = 0.05f;
		const float W_Smooth = 0.9f;

		float DistDesSq = FVector2D::DistSquared(CandidateAcc, DesiredAcc);
		float MagSq = CandidateAcc.SizeSquared();
		float JerkSq = FVector2D::DistSquared(CandidateAcc, CurAcc);

		return -(W_Proximity * DistDesSq) - (W_Effort * MagSq) - (W_Smooth * JerkSq);
	}
}
