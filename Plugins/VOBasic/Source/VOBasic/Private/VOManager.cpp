#include "VOManager.h"
#include "NavigationSystem.h"
#include "NavigationSystemTypes.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PawnMovementComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "Stats/Stats.h"

static TAutoConsoleVariable<int32> CVarVODebugShow(
    TEXT("vo.Show"), 0,
    TEXT("Show VO cones (0/1). Per-component bDebugDraw also must be true."),
    ECVF_Default);

static TAutoConsoleVariable<int32> CVarCVODebugShow(
	TEXT("cvo.Show"), 0,
	TEXT("Show VO cones (0/1). Per-component bDebugDraw also must be true."),
	ECVF_Default);

#define DEBUG_ON

void UVOManager::OnNavDataRegistered(ANavigationData&){ }
void UVOManager::OnNavDataUnregistered(ANavigationData&){ }
void UVOManager::CleanUp(float){ }

void UVOManager::RegisterAgent(UVOFollowingComponent* Comp)
{
	Agents.AddUnique(Comp);
}

void UVOManager::UnregisterAgent(UVOFollowingComponent* Comp)
{
	Agents.Remove(Comp);
}

void UVOManager::Tick(float DeltaTime)
{
	for (int32 i = Agents.Num() - 1; i >= 0; --i)
	{
		auto* Comp = Agents[i].Get();
		if (!Comp) { Agents.RemoveAtSwap(i); continue; }

		APawn* P = nullptr;
		if (const AController* C = Cast<AController>(Comp->GetOwner()))
			P = C->GetPawn();
		if (!P) continue;

		const FVector Pos = Comp->GetOwnerLocation();

		FVector DesiredVel = FVector::ZeroVector;
		if (Comp->HasVOGoal())
		{
			const FVector To = (Comp->GetMoveGoal() - Pos);
			const FVector2D To2D(To.X, To.Y);
			const float Dist = To2D.Size();
			if (Dist > 1.f)
				DesiredVel = FVector(To2D / Dist * Comp->GetEffectiveParams().MaxSpeed, 0.f);
		}

		const FVector CurVel = Comp->GetOwnerVelocity();

		TArray<FVONeighborView> Neis;
		const float Range = Comp->GetEffectiveParams().NeighborRange;
		const float R2 = Range * Range;
		for (const TWeakObjectPtr<UVOFollowingComponent>& It : Agents)
		{
			UVOFollowingComponent* Other = It.Get();
			if (!Other || Other == Comp) continue;
			const FVector OP = Other->GetOwnerLocation();
			if (FVector::DistSquared2D(Pos, OP) > R2) continue;
			FVONeighborView V; V.Pos = OP; V.Vel = Other->GetOwnerVelocity(); V.Radius = Other->GetAgentRadius();
			Neis.Add(V);
		}

		PrepareArrays(Neis.Num());;

		const FVector OutVel = ComputeVelocity(Comp, CurVel, DesiredVel, Neis, Comp->GetEffectiveParams());

		if (auto* Move = P->FindComponentByClass<UPawnMovementComponent>())
		{
			Move->RequestDirectMove(OutVel, false);
		}
	}
}

bool UVOManager::IsVelocityForbidden(const FVector2D& CandidateVA, const TArray<FVONeighborView>& Neis, const FVector& ActorPos, const FVOParams& Params) const
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

void UVOManager::BuildVOCones(const TArray<FVONeighborView>& Neis, const FVector& ActorPos, const FVOParams& Params, FVOConesSoA& OutVOCones) const
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

void UVOManager::CollectIntersections(const FVOConesSoA& VOCones)
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
			if (TryFindIntersections(SegmentI, SegmentJ, &OutPoint, &OutT))
			{
				const bool bIsFirst = FVector2D::DotProduct(CurRayDir, NormalJ) > 0.f;
				IntersectionsByRays[i].Add(FVOConeIntersection{ OutPoint, bIsFirst, OutT });
			}
		}
	}
}

void UVOManager::SortIntersectionsByRays()
{
	for (int32 RayIdx = 0; RayIdx < IntersectionsByRays.Num(); ++RayIdx)
	{
		IntersectionsByRays[RayIdx].Sort([&](const FVOConeIntersection& A, const FVOConeIntersection& B)
		{
			return A.t < B.t;
		});
	}
}

int32 UVOManager::CountVOsForPoint(const FVOConesSoA& VOCones, int32 ConeIndexToSkip, const FVector2D& P) const
{
	int32 Count = 0;
	for (int32 i = 0; i < VOCones.Num(); ++i)
	{
		if (i == ConeIndexToSkip) continue;
		const float LeftDot  = FVector2D::DotProduct(P - VOCones.Apex[i], VOCones.LeftRayNormal[i]);
		const float RightDot = FVector2D::DotProduct(P - VOCones.Apex[i], VOCones.RightRayNormal[i]);
		const float THDot    = FVector2D::DotProduct(P - VOCones.LeftRayApex[i], VOCones.TimeHorizonNormal[i]);
		if (LeftDot >= 0.f && RightDot >= 0.f && THDot >= 0.f)
			++Count;
	}
	return Count;
}

void UVOManager::ClassifySegments(const FVOConesSoA& VOCones, const FVOParams& Params)
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

FVOCone UVOManager::ComputeVOCone(const float R, const FVector2D& C, const FVector2D& Vel, const FVOParams& Params) const
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
	Cone.LeftRayNormal	= (P + Q) * DenominatorInverted;
	Cone.RightRayOffset = -FVector2D::DotProduct(Cone.RightRayNormal, Vel);
	Cone.LeftRayOffset	= -FVector2D::DotProduct(Cone.LeftRayNormal, Vel);

	// Time Horizon
	float CSize = FMath::Sqrt(CSizeSquared);
	FVector2D PTimeHorizon = ((CSize - R) / (CSize * Params.TauHorizon)) * C + Vel;
	Cone.TimeHorizonNormal = C / CSize;
	Cone.TimeHorizonOffset = -FVector2D::DotProduct(Cone.TimeHorizonNormal, PTimeHorizon);

	// Apexes
	float LDeterminantInverted = 1.f / (Cone.TimeHorizonNormal.X * Cone.LeftRayNormal.Y  - Cone.TimeHorizonNormal.Y * Cone.LeftRayNormal.X);
	Cone.LeftRayApex = FVector2D(
		(Cone.TimeHorizonNormal.Y * Cone.LeftRayOffset - Cone.LeftRayNormal.Y * Cone.TimeHorizonOffset) * LDeterminantInverted,
		(Cone.LeftRayNormal.X * Cone.TimeHorizonOffset - Cone.TimeHorizonNormal.X * Cone.LeftRayOffset) * LDeterminantInverted	
	);
	Cone.RightRayApex = PTimeHorizon + (PTimeHorizon - Cone.LeftRayApex);

	// Ray Directions
	Cone.LeftRayDir		= FVector2D(-Cone.LeftRayNormal.Y, Cone.LeftRayNormal.X);
	Cone.RightRayDir	= FVector2D(Cone.RightRayNormal.Y, -Cone.RightRayNormal.X);

	// Segments
	FVOSegment OutSegment;
	if (TryFindSegmentOfRayInCircle(Cone.LeftRayApex, Cone.LeftRayNormal, Cone.LeftRayOffset, Cone.LeftRayDir,
									Params.MaxSpeed,
									&OutSegment))
	{
		Cone.bIsLeftRaySegmentValid = true;
		Cone.LeftRaySegment = OutSegment;
	}

	if (TryFindSegmentOfRayInCircle(Cone.RightRayApex, Cone.RightRayNormal, Cone.RightRayOffset, Cone.RightRayDir,
									Params.MaxSpeed,
									&OutSegment))
	{
		Cone.bIsRightRaySegmentValid = true;
		Cone.RightRaySegment = OutSegment;
	}

	if (TryFindSubSegmentInCircle(Cone.LeftRayApex, Cone.RightRayApex, Params.MaxSpeed, &OutSegment))
	{
		Cone.bIsTHSegmentValid = true;
		Cone.TimeHorizonSegment = OutSegment;
	}
	
	return Cone;
}

bool UVOManager::TryFindIntersections(FVOSegment S1, FVOSegment S2, FVector2D* OutPoint, float* OutT)
{
	FVector2D Delta1 = S1.P2 - S1.P1;
	FVector2D Delta2 = S2.P2 - S2.P1;

	float Denominator = Delta1.X * Delta2.Y - Delta1.Y * Delta2.X;

	// Ignore grazing cases
	if (FMath::IsNearlyZero(Denominator, KINDA_SMALL_NUMBER))
	{
		return false;
	}

	float InvDenominator = 1.f / Denominator;
	FVector2D S1ToS2 = S2.P1 - S1.P1;
	float t = (S1ToS2.X * Delta2.Y - S1ToS2.Y * Delta2.X) * InvDenominator;
	float u = (S1ToS2.X * Delta1.Y - S1ToS2.Y * Delta1.X) * InvDenominator;

	// TODO: Think about edge case:
	// If S1 (or S2) is degenerate (A == B) and the point lies on the other segment,
	//    the function returns true and sets OutT = 0 (i.e., at S1.A).
	
	// Intersection outside segments
	if (t < 0.f || t > 1.f || u < 0.f || u > 1.f)
		return false;

	// Find OutPoint and OutT
	if (OutPoint)
	{
		*OutPoint = S1.P1 + t * Delta1;
	}

	if (OutT)
	{
		*OutT = t;
	}

	return true;
}

bool UVOManager::TryFindSegmentOfRayInCircle(const FVector2D& Apex, const FVector2D& Normal, const float Offset,
                                             const FVector2D& Dir, const float Radius, FVOSegment* OutSegment) const
{
	const float S       = Normal.X * Normal.X + Normal.Y * Normal.Y;
	const float SInv	= 1.f / S;
	const float SInvSqrt = FMath::InvSqrt(S);
	const float RR      = Radius * Radius;
	const float D       = FMath::Abs(Offset) * SInvSqrt;    // Distance from center of circle to line

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

bool UVOManager::TryFindSubSegmentInCircle(const FVector2D& P1, const FVector2D& P2, const float Radius,
	FVOSegment* OutSegment) const
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

bool UVOManager::WillCollideWithinTau(const FVector2D& RelativePosition, const FVector2D& RelativeVelocity, float Radius, float TimeHorizon, float* OutTOI) const
{
	const float PV = FVector2D::DotProduct(RelativePosition, RelativeVelocity);

	if (PV <= 0.f)
		return false;

	const float VV = RelativeVelocity.SizeSquared();
	const float PP = RelativePosition.SizeSquared();
	const float R2 = Radius*Radius;
	const float a = VV;
	const float b = -2.f * PV;
	const float c = PP - R2;
	const float Disc = b*b - 4.f*a*c;

	if (Disc < 0.f || a < 1e-6f)
		return false;

	const float SqrtDisc = FMath::Sqrt(Disc);
	const float T1 = (-b - SqrtDisc) / (2.f*a);
	const float T2 = (-b + SqrtDisc) / (2.f*a);

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

float UVOManager::ScoreVelocityCandidate(
	const FVector2D& V,
	const FVector2D& DesiredVel2D,
	const FVector2D& CurVel2D,
	const TArray<FVONeighborView>& Neis,
	const FVector& ActorPos,
	const FVOParams& Params
) const
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
	const float WProximity  = 1.0f;
	const float WAlign      = 0.1f;
	const float WSpeed      = 0.5f;
	const float WAccelPen   = 0.3f;

	// Score
	const float score =
		  WProximity * proximity
		+ WAlign     * align
		+ WSpeed     * speedPref
		- WAccelPen  * accelNorm;

	return score;
}

FVector2D UVOManager::SelectBestVelocityFromOutsideSegments(
	const FVector2D& DesiredVel2D,
	const FVector2D& CurVel2D,
	const TArray<FVONeighborView>& Neis,
	const FVector& ActorPos,
	const FVOParams& Params
) const
{
	Debug_LastCandidates.Reset();
	Debug_BestCandidateIdx = -1;
	
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
				Debug_LastCandidates.Add(cand);
				++candidateIdx;
				
				const float score = ScoreVelocityCandidate(cand, DesiredVel2D, CurVel2D, Neis, ActorPos, Params);
				if (score > bestScore)
				{
					bestScore = score;
					bestV = cand;
					bFoundAny = true;
					Debug_BestCandidateIdx = candidateIdx;
				}
			}
		}
	}

	if (!bFoundAny) //TODO: Implement more complex selection
	{
		const FVector2D curClamped = CurVel2D.GetClampedToMaxSize(MaxSpeed);
		const FVector2D softDesired = DesiredVel2D.GetClampedToMaxSize(MaxSpeed * 0.75f);

		const int32 baseIdx = Debug_LastCandidates.Num();
		Debug_LastCandidates.Add(curClamped);
		Debug_LastCandidates.Add(softDesired);
		
		const float s1 = ScoreVelocityCandidate(curClamped, DesiredVel2D, CurVel2D, Neis, ActorPos, Params);
		const float s2 = ScoreVelocityCandidate(softDesired, DesiredVel2D, CurVel2D, Neis, ActorPos, Params);

		if (s2 > s1)
		{
			Debug_BestCandidateIdx = baseIdx + 1;
			return softDesired;
		}
		else
		{
			Debug_BestCandidateIdx = baseIdx;
			return curClamped;
		}
	}

	return bestV;
}

void UVOManager::DrawVOCones(const UVOFollowingComponent* Comp, const FVOConesSoA& Cones) const
{
	UWorld* W = Comp->GetWorld(); if (!W) return;
	FVector P = Comp->GetOwnerLocation();
	for (int i = 0; i < Cones.Num(); i++)
	{
		FColor Color = FColor::MakeRandomColor();
		FVector2D curRayDir = FVector2D(-Cones.LeftRayNormal[0].Y, Cones.LeftRayNormal[0].X);

		/*FVector Start = FVector(curVO.LeftRayApex.X, curVO.LeftRayApex.Y, 0.f);
		FVector End = Start + FVector(curRayDir.X, curRayDir.Y, 0.f) * 1000.f;
		DrawDebugLine(W, Start + P, End + P, Color, true, 15.f, 0, 0.6f);
		curRayDir = FVector2D(curVO.RightRayNormal.Y, -curVO.RightRayNormal.X);
		Start = FVector(curVO.RightRayApex.X, curVO.RightRayApex.Y, 0.f);
		End = Start + FVector(curRayDir.X, curRayDir.Y, 0.f) * 1000.f;
		DrawDebugLine(W, Start + P, End + P, Color, true, 15.f, 0, 0.6f);
		Start = FVector(curVO.LeftRayApex.X, curVO.LeftRayApex.Y, 0.f);
		End = FVector(curVO.RightRayApex.X, curVO.RightRayApex.Y, 0.f);
		DrawDebugLine(W, Start + P, End + P, Color, true, 15.f, 0, 0.6f);*/

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

void UVOManager::DrawCombinedVO(const UVOFollowingComponent* Comp) const
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
			FVector Start	= { OutsideSegmentsByRays[i][j].P1.X, OutsideSegmentsByRays[i][j].P1.Y, 0.f };
			FVector End		= { OutsideSegmentsByRays[i][j].P2.X, OutsideSegmentsByRays[i][j].P2.Y, 0.f };
			DrawDebugLine(W, Start + P, End + P, Color, true, 15.f, 0, 1.2f);
		}
	}
}

void UVOManager::DrawVelocityCandidates(
	const UVOFollowingComponent* Comp,
	float PointSize,
	float LifeTime
) const
{
	const FVector ActorPos = Comp->GetOwnerLocation();

	UWorld* W = Comp->GetWorld();

	const FColor CandidateColor(90, 180, 255); // голубой
	const FColor BestColor(255, 220, 0);       // жёлтый

	for (int32 i = 0; i < Debug_LastCandidates.Num(); ++i)
	{
		const FVector2D V = Debug_LastCandidates[i];
		const FVector P = ActorPos + FVector(V.X, V.Y, 0.f);

		const bool bIsBest = (i == Debug_BestCandidateIdx);
		const FColor Col = bIsBest ? BestColor : CandidateColor;
		const float Sz = bIsBest ? (PointSize * 1.7f) : PointSize;

		DrawDebugPoint(W, P, Sz, Col, /*bPersistentLines*/ true, LifeTime);
	}
}

FVector UVOManager::ComputeVelocity(const UVOFollowingComponent* Comp, const FVector& CurVel, const FVector& DesiredVel, const TArray<FVONeighborView>& Neis, const FVOParams& Params)
{
	SCOPE_CYCLE_COUNTER(STAT_VOComputeVelocity);
	
	const FVector ActorPos = Comp->GetOwnerLocation();
	
	const bool bDesiredForbidden = IsVelocityForbidden(FVector2D(DesiredVel.X, DesiredVel.Y), Neis, ActorPos, Params);
	if (!bDesiredForbidden)
		return DesiredVel.GetClampedToMaxSize2D(Params.MaxSpeed);
	
	BuildVOCones(Neis, ActorPos, Params, VO_Cones);
	CollectIntersections(VO_Cones);
	SortIntersectionsByRays();
	ClassifySegments(VO_Cones, Params);

	const FVector2D best2D = SelectBestVelocityFromOutsideSegments(
		FVector2D(DesiredVel.X, DesiredVel.Y),
		FVector2D(CurVel.X, CurVel.Y),
		Neis,
		ActorPos,
		Params
	);
	FVector OutVel = FVector(best2D.X, best2D.Y, 0.f);

#ifdef DEBUG_ON
	UWorld* W = Comp->GetWorld();
	if (Comp->bDebugDraw)
	{
		FlushPersistentDebugLines(Comp->GetWorld());
		if (CVarCVODebugShow.GetValueOnAnyThread() != 0)
			DrawCombinedVO(Comp);
		if (CVarVODebugShow.GetValueOnAnyThread() != 0)
			DrawVOCones(Comp, VO_Cones);
		for (auto N : Neis)
		{
			DrawDebugLine(W, N.Pos, N.Pos + N.Vel, Comp->DebugDrawColor, true, 15.f, 0, 0.3f);
		}
		DrawDebugCircle(W, Comp->GetOwnerLocation(), Params.MaxSpeed, 20, Comp->DebugDrawColor, true, 15.f, 0, 0.6f, FVector(0.f, 1.f, 0.f), FVector(1.f, 0.f, 0.f));
			
		DrawVelocityCandidates(Comp, 10.f, 15.f);
	}
#endif
	return OutVel;
}

void UVOManager::PrepareArrays(size_t NumNeis)
{
	// TODO: Think about shrinking
	
	VO_Cones.Reset();
	VO_Cones.Reserve(NumNeis);

	size_t NumRays = 3 * NumNeis;
	IntersectionsByRays.SetNum(NumRays, EAllowShrinking::No);
	for (int32 i = 0; i < IntersectionsByRays.Num(); ++i)
	{
		IntersectionsByRays[i].Reset();
		IntersectionsByRays[i].Reserve(NumRays - 1); // 3 * (N - 1) + 2
	}

	OutsideSegmentsByRays.SetNum(NumRays, EAllowShrinking::No);
	for (int32 i = 0; i < OutsideSegmentsByRays.Num(); ++i)
	{
		OutsideSegmentsByRays[i].Reset();
		OutsideSegmentsByRays[i].Reserve(FMath::Max(0, IntersectionsByRays[i].Num()));
	}
}
