#include "UVOManager.h"
#include "NavigationSystem.h"
#include "NavigationSystemTypes.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PawnMovementComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"

static TAutoConsoleVariable<int32> CVarVODebugShow(
    TEXT("vo.Show"), 0,
    TEXT("Show VO cones (0/1). Per-component bDebugDraw also must be true."),
    ECVF_Default);

static TAutoConsoleVariable<int32> CVarCVODebugShow(
	TEXT("cvo.Show"), 0,
	TEXT("Show VO cones (0/1). Per-component bDebugDraw also must be true."),
	ECVF_Default);

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
				DesiredVel = FVector(To2D / Dist * Comp->Params.MaxSpeed, 0.f);
		}

		const FVector CurVel = Comp->GetOwnerVelocity();

		TArray<FVONeighborView> Neis;
		const float Range = Comp->Params.NeighborRange;
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

		const FVector OutVel = ComputeVelocity(Comp, CurVel, DesiredVel, Neis, Comp->Params);

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

void UVOManager::BuildVOCones(const TArray<FVONeighborView>& Neis, const FVector& ActorPos, const FVOParams& Params, TArray<FVOCone>& OutVOCones) const
{
	OutVOCones.Reset();
	OutVOCones.Reserve(Neis.Num());

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

static inline FVector2D RayDirFromNormal_Local(const FVector2D& N, bool bIsLeft)
{
	return bIsLeft ? FVector2D(-N.Y, N.X) : FVector2D(N.Y, -N.X);
}

void UVOManager::CollectIntersections(const TArray<FVOCone>& VOCones, TArray<TArray<FVOConeIntersection>>& OutIntersectionsByRays) const
{
	const int32 NumRays = VOCones.Num() * 2;
	OutIntersectionsByRays.Reset();
	OutIntersectionsByRays.SetNum(NumRays);
	for (int32 i = 0; i < OutIntersectionsByRays.Num(); ++i)
	{
		OutIntersectionsByRays[i].Reserve(2 * VOCones.Num() - 1);
	}

	int32 CurRayIndex = 0;
	for (int32 i = 0; i < VOCones.Num(); ++i)
	{
		const FVOCone& CurVO = VOCones[i];
		OutIntersectionsByRays[CurRayIndex    ].Add({ CurVO.LeftRayApex,  true });
		OutIntersectionsByRays[CurRayIndex + 1].Add({ CurVO.RightRayApex, true });

		for (int32 j = 0; j < VOCones.Num(); ++j)
		{
			if (i == j) continue;
			for (int sideI = 0; sideI < 2; ++sideI)
			{
				const int32 rayIndexI = CurRayIndex + sideI;
				FVector2D apex1, n1; float c1;
				GetRayApexNormalOffset(VOCones, rayIndexI, apex1, n1, c1);
				const bool isLeft1 = (sideI == 0);
				const FVector2D curRayDir = isLeft1 ? LeftDirFromNormal(n1) : RightDirFromNormal(n1);
				for (int sideJ = 0; sideJ < 2; ++sideJ)
				{
					const int32 rayIndexJ = j * 2 + sideJ;
					FVector2D apex2, n2; float c2;
					GetRayApexNormalOffset(VOCones, rayIndexJ, apex2, n2, c2);
					const bool isLeft2 = (sideJ == 0);
					FVector2D P;
					if (TryFindIntersections(
						n1.X, n1.Y, c1, apex1, isLeft1,
						n2.X, n2.Y, c2, apex2, isLeft2,
						&P))
					{
						const bool bIsFirst = FVector2D::DotProduct(curRayDir, n2) > 0.f;
						OutIntersectionsByRays[rayIndexI].Add({ P, bIsFirst });
					}
				}
			}
		}

		CurRayIndex += 2;
	}
}

void UVOManager::SortIntersectionsByRays(const TArray<FVOCone>& VOCones, TArray<TArray<FVOConeIntersection>>& IntersectionsByRays) const
{
	for (int32 RayIdx = 0; RayIdx < IntersectionsByRays.Num(); ++RayIdx)
	{
		FVector2D CurApex, CurNormal; float CurOffset;
		GetRayApexNormalOffset(VOCones, RayIdx, CurApex, CurNormal, CurOffset);
		const FVector2D CurRayDir = (RayIdx % 2 == 0) ? LeftDirFromNormal(CurNormal) : RightDirFromNormal(CurNormal);

		IntersectionsByRays[RayIdx].Sort([&](const FVOConeIntersection& A, const FVOConeIntersection& B)
		{
			const float tA = FVector2D::DotProduct(A.P - CurApex, CurRayDir);
			const float tB = FVector2D::DotProduct(B.P - CurApex, CurRayDir);
			return tA < tB;
		});
	}
}

int32 UVOManager::CountVOsForPoint(const TArray<FVOCone>& VOCones, int32 ConeIndexToSkip, const FVector2D& P) const
{
	int32 Count = 0;
	for (int32 i = 0; i < VOCones.Num(); ++i)
	{
		if (i == ConeIndexToSkip) continue;
		const float LeftDot  = FVector2D::DotProduct(P - VOCones[i].Apex, VOCones[i].LeftRayNormal);
		const float RightDot = FVector2D::DotProduct(P - VOCones[i].Apex, VOCones[i].RightRayNormal);
		const float THDot    = FVector2D::DotProduct(P - VOCones[i].LeftRayApex, VOCones[i].TimeHorizonNormal);
		if (LeftDot >= 0.f && RightDot >= 0.f && THDot >= 0.f)
			++Count;
	}
	return Count;
}

void UVOManager::ClassifySegments(const TArray<FVOCone>& VOCones, const TArray<TArray<FVOConeIntersection>>& IntersectionsByRays, const FVOParams& Params, TArray<TArray<FVOOutsideSegment>>& OutOutsideSegmentsByRays) const
{
	OutOutsideSegmentsByRays.Reset();
	OutOutsideSegmentsByRays.SetNum(IntersectionsByRays.Num());
	for (int32 i = 0; i < OutOutsideSegmentsByRays.Num(); ++i)
	{
		OutOutsideSegmentsByRays[i].Reserve(FMath::Max(0, IntersectionsByRays[i].Num()));
	}

	for (int32 RayIdx = 0; RayIdx < IntersectionsByRays.Num(); ++RayIdx)
	{
		FVector2D CurApex, CurNormal; float CurOffset;
		GetRayApexNormalOffset(VOCones, RayIdx, CurApex, CurNormal, CurOffset);

		int32 CountOfVOs = CountVOsForPoint(VOCones, RayIdx / 2, CurApex);

		for (int32 j = 1; j < IntersectionsByRays[RayIdx].Num(); ++j)
		{
			if (CountOfVOs == 0)
			{
				const FVOOutsideSegment OutsideSegment = {
					IntersectionsByRays[RayIdx][j - 1].P,
					IntersectionsByRays[RayIdx][j].P,
					CurNormal,
					CurOffset,
				};
				OutOutsideSegmentsByRays[RayIdx].Add(OutsideSegment);
			}

			if (IntersectionsByRays[RayIdx][j].bIsFirst)
				++CountOfVOs;
			else
				CountOfVOs = FMath::Max(0, CountOfVOs - 1);
		}

		if (CountOfVOs == 0)
		{
			const float S       = CurNormal.X * CurNormal.X + CurNormal.Y * CurNormal.Y;
			const float InvSqrt = FMath::InvSqrt(S);
			const float RR      = Params.MaxSpeed * Params.MaxSpeed;
			const float D       = FMath::Abs(CurOffset) * InvSqrt;

			const FVector2D CurRayDir = (RayIdx % 2 == 0) ? LeftDirFromNormal(CurNormal) : RightDirFromNormal(CurNormal);
			const FVector2D FromPoint = IntersectionsByRays[RayIdx].Num() > 0 ? IntersectionsByRays[RayIdx].Last().P : CurApex;

			if (FMath::IsNearlyEqual(D, Params.MaxSpeed, KINDA_SMALL_NUMBER))
			{
				const FVector2D LastPoint = -CurNormal * CurOffset / S;
				if (FVector2D::DotProduct(LastPoint - CurApex, CurRayDir) > 0)
				{
					OutOutsideSegmentsByRays[RayIdx].Add({ FromPoint, LastPoint, CurNormal, CurOffset });
				}
			}
			else if (D < Params.MaxSpeed)
			{
				const FVector2D Q = FVector2D(CurNormal.Y, -CurNormal.X) * FMath::Sqrt(RR * S - CurOffset * CurOffset);
				const FVector2D LastPoint1 = (-CurNormal * CurOffset + Q) / S;
				const FVector2D LastPoint2 = (-CurNormal * CurOffset - Q) / S;

				const float t1 = FVector2D::DotProduct(LastPoint1 - IntersectionsByRays[RayIdx].Last().P, CurRayDir);
				const float t2 = FVector2D::DotProduct(LastPoint2 - IntersectionsByRays[RayIdx].Last().P, CurRayDir);

				if (t1 > 0 || t2 > 0)
				{
					const FVector2D Chosen = (t1 > t2) ? LastPoint1 : LastPoint2;
					OutOutsideSegmentsByRays[RayIdx].Add({ FromPoint, Chosen, CurNormal, CurOffset });
				}
			}
		}
	}
}

void UVOManager::GetRayApexNormalOffset(const TArray<FVOCone>& VOCones, int32 RayIndex, FVector2D& OutApex, FVector2D& OutNormal, float& OutOffset)
{
	const int32 ConeIdx = RayIndex / 2;
	const bool bLeft = (RayIndex % 2 == 0);
	if (bLeft)
	{
		OutApex   = VOCones[ConeIdx].LeftRayApex;
		OutNormal = VOCones[ConeIdx].LeftRayNormal;
		OutOffset = VOCones[ConeIdx].LeftRayOffset;
	}
	else
	{
		OutApex   = VOCones[ConeIdx].RightRayApex;
		OutNormal = VOCones[ConeIdx].RightRayNormal;
		OutOffset = VOCones[ConeIdx].RightRayOffset;
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

	float DenominatorInverted = 1.f / (CSizeSquared * R);
	Cone.RightRayNormal = (P - Q) * DenominatorInverted;
	Cone.LeftRayNormal	= (P + Q) * DenominatorInverted;
	Cone.RightRayOffset = -FVector2D::DotProduct(Cone.RightRayNormal, Vel);
	Cone.LeftRayOffset	= -FVector2D::DotProduct(Cone.LeftRayNormal, Vel);

	float CSize = FMath::Sqrt(CSizeSquared);
	FVector2D PTimeHorizon = ((CSize - R) / (CSize * Params.TauHorizon)) * C + Vel;
	Cone.TimeHorizonNormal = C / CSize;
	Cone.TimeHorizonOffset = -FVector2D::DotProduct(Cone.TimeHorizonNormal, PTimeHorizon);

	float LDeterminantInverted = 1.f / (Cone.TimeHorizonNormal.X * Cone.LeftRayNormal.Y  - Cone.TimeHorizonNormal.Y * Cone.LeftRayNormal.X);
	Cone.LeftRayApex = FVector2D(
		(Cone.TimeHorizonNormal.Y * Cone.LeftRayOffset - Cone.LeftRayNormal.Y * Cone.TimeHorizonOffset) * LDeterminantInverted,
		(Cone.LeftRayNormal.X * Cone.TimeHorizonOffset - Cone.TimeHorizonNormal.X * Cone.LeftRayOffset) * LDeterminantInverted	
	);
	Cone.RightRayApex = PTimeHorizon + (PTimeHorizon - Cone.LeftRayApex);
	return Cone;
}

bool UVOManager::TryFindIntersections(const float A1, const float B1, const float C1,
		const float A2, const float B2, const float C2,
		FVector2D* OutPoint)
{
	float D = A1 * B2 - A2 * B1;
	if (FMath::IsNearlyZero(D, KINDA_SMALL_NUMBER))
		return false;
	float DInv = 1.f / D;
	if (OutPoint)
	{
		*OutPoint = FVector2D((B1 * C2 - B2 * C1) * DInv, (C1 * A2 - C2 * A1) * DInv);
	}
	return true;
}

bool UVOManager::TryFindIntersections(const float A1, const float B1, const float C1, FVector2D apex1, bool isLeftRay1,
		const float A2, const float B2, const float C2, FVector2D apex2, bool isLeftRay2,
		FVector2D* OutPoint)
{
	FVector2D IntersectionPoint;
	if (!TryFindIntersections(A1, B1, C1, A2, B2, C2, &IntersectionPoint))
		return false;
	FVector2D RayDir1 = isLeftRay1 ? FVector2D(-B1, A1) : FVector2D(B1, -A1);
	FVector2D RayDir2 = isLeftRay2 ? FVector2D(-B2, A2) : FVector2D(B2, -A2);
	if (FVector2D::DotProduct(RayDir1, IntersectionPoint - apex1) <= 0.f
		|| FVector2D::DotProduct(RayDir2, IntersectionPoint - apex2) <= 0.f)
		return false;
	if (OutPoint)
	{
		*OutPoint = IntersectionPoint;
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

void UVOManager::DrawVOCones(const UVOFollowingComponent* Comp, TArray<FVOCone>& Cone) const
{
	UWorld* W = Comp->GetWorld(); if (!W) return;
	FVector P = Comp->GetOwnerLocation();
	for (int i = 0; i < Cone.Num(); i++)
	{
		FVOCone curVO = Cone[i];
		FColor Color = FColor::MakeRandomColor();
		FVector2D curRayDir = FVector2D(-curVO.LeftRayNormal.Y, curVO.LeftRayNormal.X);
		FVector Start = FVector(curVO.LeftRayApex.X, curVO.LeftRayApex.Y, 0.f);
		FVector End = Start + FVector(curRayDir.X, curRayDir.Y, 0.f) * 1000.f;
		DrawDebugLine(W, Start + P, End + P, Color, true, 15.f, 0, 0.6f);
		curRayDir = FVector2D(curVO.RightRayNormal.Y, -curVO.RightRayNormal.X);
		Start = FVector(curVO.RightRayApex.X, curVO.RightRayApex.Y, 0.f);
		End = Start + FVector(curRayDir.X, curRayDir.Y, 0.f) * 1000.f;
		DrawDebugLine(W, Start + P, End + P, Color, true, 15.f, 0, 0.6f);
		Start = FVector(curVO.LeftRayApex.X, curVO.LeftRayApex.Y, 0.f);
		End = FVector(curVO.RightRayApex.X, curVO.RightRayApex.Y, 0.f);
		DrawDebugLine(W, Start + P, End + P, Color, true, 15.f, 0, 0.6f);
	}
}

void UVOManager::DrawCombinedVO(const UVOFollowingComponent* Comp, const TArray<TArray<FVOOutsideSegment>>& OutsideSegmentsByRays) const
{
	UWorld* W = Comp->GetWorld(); if (!W) return;
	FVector P = Comp->GetOwnerLocation();
	FColor Color;
	for (int i = 0; i < OutsideSegmentsByRays.Num(); i++)
	{
		if (i % 2 == 0)
			Color = FColor::MakeRandomColor();
		for (int j = 0; j < OutsideSegmentsByRays[i].Num(); j++)
		{
			FVector Start	= { OutsideSegmentsByRays[i][j].P1.X, OutsideSegmentsByRays[i][j].P1.Y, 0.f };
			FVector End		= { OutsideSegmentsByRays[i][j].P2.X, OutsideSegmentsByRays[i][j].P2.Y, 0.f };
			DrawDebugLine(W, Start + P, End + P, Color, true, 15.f, 0, 1.2f);
		}
	}
}

FVector UVOManager::ComputeVelocity(const UVOFollowingComponent* Comp, const FVector& CurVel, const FVector& DesiredVel, const TArray<FVONeighborView>& Neis, const FVOParams& Params) const
{
	const FVector ActorPos = Comp->GetOwnerLocation();
	UWorld* W = Comp->GetWorld();
	const bool bDesiredForbidden = IsVelocityForbidden(FVector2D(DesiredVel.X, DesiredVel.Y), Neis, ActorPos, Params);
	if (!bDesiredForbidden)
		return DesiredVel.GetClampedToMaxSize2D(Params.MaxSpeed);
	TArray<FVOCone> VOCones;
	BuildVOCones(Neis, ActorPos, Params, VOCones);
	TArray<TArray<FVOConeIntersection>> IntersectionsByRays;
	CollectIntersections(VOCones, IntersectionsByRays);
	SortIntersectionsByRays(VOCones, IntersectionsByRays);
	TArray<TArray<FVOOutsideSegment>> OutsideSegmentsByRays;
	ClassifySegments(VOCones, IntersectionsByRays, Params, OutsideSegmentsByRays);
	if (Comp->bDebugDraw)
	{
		FlushPersistentDebugLines(Comp->GetWorld());
		if (CVarCVODebugShow.GetValueOnAnyThread() != 0)
			DrawCombinedVO(Comp, OutsideSegmentsByRays);
		if (CVarVODebugShow.GetValueOnAnyThread() != 0)
			DrawVOCones(Comp, VOCones);
		for (auto N : Neis)
		{
			DrawDebugLine(W, N.Pos, N.Pos + N.Vel, Comp->DebugDrawColor, true, 15.f, 0, 0.3f);
		}
		DrawDebugCircle(W, Comp->GetOwnerLocation(), Params.MaxSpeed, 20, Comp->DebugDrawColor, true, 15.f, 0, 0.6f, FVector(0.f, 1.f, 0.f), FVector(1.f, 0.f, 0.f));
	}
	return DesiredVel;
}