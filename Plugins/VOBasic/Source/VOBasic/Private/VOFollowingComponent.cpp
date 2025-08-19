#include "VOFollowingComponent.h"

#include <ThirdParty/ShaderConductor/ShaderConductor/External/DirectXShaderCompiler/include/dxc/DXIL/DxilConstants.h>

#include "VOWorldSubsystem.h"
#include "AIController.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "DrawDebugHelpers.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Engine/World.h"

// Global debug cvar
static TAutoConsoleVariable<int32> CVarVODebugShow(
    TEXT("vo.Show"), 0,
    TEXT("Show VO cones (0/1). Per-component bDebugDraw also must be true."),
    ECVF_Default);

UVOFollowingComponent::UVOFollowingComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    bWantsInitializeComponent = true;
}

void UVOFollowingComponent::OnRegister()
{
    Super::OnRegister();
    if (UWorld* W = GetWorld())
        if (auto* S = W->GetSubsystem<UVOWorldSubsystem>())
            S->Register(this);
}

void UVOFollowingComponent::OnUnregister()
{
    if (UWorld* W = GetWorld())
        if (auto* S = W->GetSubsystem<UVOWorldSubsystem>())
            S->Unregister(this);
    Super::OnUnregister();
}

APawn* UVOFollowingComponent::GetControlledPawn() const   //TODO: Cached pawn
{
	if (const AController* C = Cast<AController>(GetOwner())) // UActorComponent::GetOwner()
	{
		return C->GetPawn(); // AController::GetPawn()
	}
	return nullptr;
}

FVector UVOFollowingComponent::GetOwnerLocation() const
{
    const APawn* P = GetControlledPawn();
    return P ? P->GetActorLocation() : FVector::ZeroVector;
}

FVector UVOFollowingComponent::GetOwnerVelocity() const
{
    const APawn* P = GetControlledPawn();
    if (!P) return FVector::ZeroVector;
    if (auto* Move = P->FindComponentByClass<UMovementComponent>())
        return Move->Velocity;
    return FVector::ZeroVector;
}

void UVOFollowingComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	
    APawn* P = GetControlledPawn();
    if (!P) return;

    const FVector Pos = GetOwnerLocation();

    // Desired velocity: direct toward goal (no path/corridor yet)
    FVector DesiredVel = FVector::ZeroVector;
    if (bHasGoal)
    {
        const FVector To = (Goal - Pos);
        const FVector2D To2D(To.X, To.Y);
        const float Dist = To2D.Size();
        if (Dist > 1.f)
            DesiredVel = FVector(To2D / Dist * Params.MaxSpeed, 0.f);
    }

    const FVector CurVel = GetOwnerVelocity();

    // Collect neighbors
    TArray<FVONeighborView> Neis;
    if (UWorld* W = GetWorld())
        if (auto* S = W->GetSubsystem<UVOWorldSubsystem>())
            S->QueryNeighbors(this, Pos, Params.NeighborRange, Neis);

    // Compute VO velocity
    const FVector OutVel = ComputeVelocity(CurVel, DesiredVel, Neis);
	
    // Move
    if (auto* Move = P->FindComponentByClass<UPawnMovementComponent>())
    {
        Move->RequestDirectMove(OutVel, false);
    }

    // Debug draw
    /*if (bDebugDraw && CVarVODebugShow.GetValueOnAnyThread() != 0)
    {
    	FlushPersistentDebugLines(GetWorld());
    	//DrawVOConesTau(Pos, Neis);
    }*/
}

static bool AnyCollisionWithinTau(const FVector& vCand3D, const TArray<FVONeighborView>& Neis, float SelfRadius, float Tau)
{
    const FVector2D vCand(vCand3D.X, vCand3D.Y);
    for (const FVONeighborView& N : Neis)
    {
        // Relatives are computed later in component method (need self pos); here we only wrap API.
    }
    return false;
}

bool UVOFollowingComponent::WillCollideWithinTau(const FVector2D& RelativePosition, const FVector2D& RelativeVelocity, float Radius, float TimeHorizon, float* OutTOI) const
{
	// (v_x^2 + v_y^2) * t^2 - 2(v_x * p_x + v_y * p_y) * t + (p_x^2 + p_y^2) = R^2
	// v.SizeSqr * t^2 - 2 * Dot(p, v) * t + (p.SizeSqr - R^2) = 0 ; t in (0, Tau]
	
    const float PV = FVector2D::DotProduct(RelativePosition, RelativeVelocity);
    if (PV <= 0.f) // Moving away or tangent; 
        return false;

    const float VV = RelativeVelocity.SizeSquared();
    const float PP = RelativePosition.SizeSquared();
    const float R2 = Radius*Radius;

    const float a = VV;
    const float b = -2.f * PV;
    const float c = PP - R2;

    const float Disc = b*b - 4.f*a*c;
    if (Disc < 0.f || a < 1e-6f)   // No Collision
    	return false;

    const float SqrtDisc = FMath::Sqrt(Disc);
    const float T1 = (-b - SqrtDisc) / (2.f*a);
    const float T2 = (-b + SqrtDisc) / (2.f*a);

	// Take the smallest collision time
    float THit = TNumericLimits<float>::Max();   
    if (T1 > 0.f)
    	THit = T1;
	else if (T2 > 0.f)
		THit = T2;
	else
		return false;

	// Check if collision happens within time horizon
    if (THit <= TimeHorizon)
    {
        if (OutTOI)
        	*OutTOI = THit;
        return true;
    }
    return false;
}

FVector UVOFollowingComponent::ComputeVelocity(const FVector& CurVel, const FVector& DesiredVel, const TArray<FVONeighborView>& Neis) const
{
    const FVector actorPos = GetOwnerLocation();
	UWorld* W = GetWorld(); // UTU
	
    auto IsForbidden = [&](const FVector2D& vA2D)->bool
    {
        for (const FVONeighborView& N : Neis)
        {
            const FVector2D pRel(N.Pos.X - actorPos.X, N.Pos.Y - actorPos.Y);
            const FVector2D vRel = vA2D - FVector2D(N.Vel.X, N.Vel.Y);
            const float R = Params.AgentRadius + N.Radius;
			
        	if (bDebugDraw && CVarVODebugShow.GetValueOnAnyThread() != 0)
        	{
        		//FlushPersistentDebugLines(GetWorld());
        		//DrawDebugLine(W, actorPos, actorPos + FVector(vA2D.X, vA2D.Y, 0.f), DebugDrawColor, true, 15.f, 0, 1.2f);
        		//DrawDebugLine(W, N.Pos, N.Pos + N.Vel, DebugDrawColor, true, 15.f, 0, 1.2f);
        	}
        	
            if (WillCollideWithinTau(pRel, vRel, R, Params.TauHorizon, nullptr))
                return true;
        }
        return false;
    };
	UE_LOG(LogTemp, Warning, TEXT("Is Desired Good = %s"), !IsForbidden(FVector2D(DesiredVel.X, DesiredVel.Y)) ? TEXT("true") : TEXT("false"));
    // Try desired
	
    if (!IsForbidden(FVector2D(DesiredVel.X, DesiredVel.Y)))
        return DesiredVel.GetClampedToMaxSize2D(Params.MaxSpeed);
	
	const FVector2D vDes2(DesiredVel.X, DesiredVel.Y);

	// Construct all velocity obstacles
    TArray<FVOCone> VOCones;
	VOCones.Reserve(Neis.Num());

	for (int i = 0; i < Neis.Num(); i++)
	{
	    const FVONeighborView& N = Neis[i];
	
		float R = Params.AgentRadius + N.Radius;							//Minkowski sum radius
		FVector2D pRel(N.Pos.X - actorPos.X, N.Pos.Y - actorPos.Y);	//Relative position of a neighbor
		
		// TODO: Case when we grazing N
		if (R*R >= pRel.SizeSquared()) 
		{
			UE_LOG(LogTemp, Warning, TEXT("R*R < pRel.SizeSquared()"));
			continue;
		}

		VOCones.Add(ComputeVOCone(R, pRel, FVector2D(N.Vel)));
	}

	struct FVOConeIntersection
	{
		FVector2D P;
		bool bIsFirst; // is this intersection first of 2 VO intersections?
	};

	TArray<TArray<FVOConeIntersection>> intersectionPointsByRays; //rows - different lines
	//intersectionPointsByRays.Reserve(VOCones.Num()*2);
	intersectionPointsByRays.SetNum(VOCones.Num()*2);
	for (int i = 0; i < intersectionPointsByRays.Num(); i++)
	{
		intersectionPointsByRays[i].Reserve(2 * VOCones.Num() - 1);
	}

	int CurRayIndex = 0;
	// Find and classify all intersection points
	for (int i = 0; i < VOCones.Num(); i++)
	{
		FVOCone curVO = VOCones[i];

		// Add apexes???
		
		//for (int j = 0; j < i; j++)          // TODO:
		for (int j = 0; j < VOCones.Num(); j++)
		{
			UE_LOG(LogTemp, Warning, TEXT("I = %d, J = %d"), i, j);
			if (i == j)
				continue; 
			
			FVector2D IntersectionPoint;

#pragma region Left ray
			// Left ray of cur VO
			if (TryFindIntersections(
				curVO.LeftRayNormal.X, curVO.LeftRayNormal.Y, curVO.LeftRayOffset,
				VOCones[j].LeftRayNormal.X, VOCones[j].LeftRayNormal.Y, VOCones[j].LeftRayOffset,
				&IntersectionPoint))
			{
				FVector2D curRayDir = FVector2D(-curVO.LeftRayNormal.Y, curVO.LeftRayNormal.X);
				bool bIsFirst = FVector2D::DotProduct(curRayDir, VOCones[j].LeftRayNormal) > 0.f;
				intersectionPointsByRays[CurRayIndex].Add(FVOConeIntersection { IntersectionPoint, bIsFirst });

				// TODO: Optimization for case when we have first intersection
				// Other will be opposite if we have intersection
			}

			if (TryFindIntersections(
				curVO.LeftRayNormal.X, curVO.LeftRayNormal.Y, curVO.LeftRayOffset,
				VOCones[j].RightRayNormal.X, VOCones[j].RightRayNormal.Y, VOCones[j].RightRayOffset,
				&IntersectionPoint))
			{
				FVector2D curRayDir = FVector2D(-curVO.LeftRayNormal.Y, curVO.LeftRayNormal.X);
				bool bIsFirst = FVector2D::DotProduct(curRayDir, VOCones[j].RightRayNormal) > 0.f;
				intersectionPointsByRays[CurRayIndex].Add(FVOConeIntersection { IntersectionPoint, bIsFirst });
			}
#pragma endregion

#pragma region Right ray
			// Right ray of cur VO
			if (TryFindIntersections(
				curVO.RightRayNormal.X, curVO.RightRayNormal.Y, curVO.RightRayOffset,
				VOCones[j].LeftRayNormal.X, VOCones[j].LeftRayNormal.Y, VOCones[j].LeftRayOffset,
				&IntersectionPoint))
			{
				FVector2D curRayDir = FVector2D(curVO.RightRayNormal.Y, -curVO.RightRayNormal.X);
				bool bIsFirst = FVector2D::DotProduct(curRayDir, VOCones[j].LeftRayNormal) > 0.f;
				intersectionPointsByRays[CurRayIndex+1].Add(FVOConeIntersection { IntersectionPoint, bIsFirst });

				// TODO: Optimization for case when we have first intersection
				// Other will be opposite if we have intersection
			}

			if (TryFindIntersections(
				curVO.RightRayNormal.X, curVO.RightRayNormal.Y, curVO.RightRayOffset,
				VOCones[j].RightRayNormal.X, VOCones[j].RightRayNormal.Y, VOCones[j].RightRayOffset,
				&IntersectionPoint))
			{
				FVector2D curRayDir = FVector2D(curVO.RightRayNormal.Y, -curVO.RightRayNormal.X);
				bool bIsFirst = FVector2D::DotProduct(curRayDir, VOCones[j].RightRayNormal) > 0.f;
				intersectionPointsByRays[CurRayIndex+1].Add(FVOConeIntersection { IntersectionPoint, bIsFirst });
			}
#pragma endregion

			// TODO: TH Constraint
		}

		CurRayIndex += 2;

		/*for (int j = i+1; j < VOCones.Num(); j++) // TODO:
		{
			
		}*/
	}

	//UE_LOG(LogTemp, Log, TEXT("Rays1: %d"), intersectionPointsByRays.Num());
	
	// Sort points on each ray
	for (int i = 0; i < intersectionPointsByRays.Num(); i++)
	{
		FVector2D CurApex;
		FVector2D CurRayDir;
		if (i % 2 == 0)
		{
			CurRayDir = FVector2D(-VOCones[i/2].LeftRayNormal.Y, VOCones[i/2].LeftRayNormal.X);
			CurApex = VOCones[i/2].LeftRayApex;
		}
		else
		{
			CurRayDir = FVector2D(VOCones[i/2].RightRayNormal.Y, -VOCones[i/2].RightRayNormal.X);
			CurApex = VOCones[i/2].RightRayApex;
		}

		intersectionPointsByRays[i].Sort([&](const FVOConeIntersection& A, const FVOConeIntersection& B)
		{
			const float tA = FVector2D::DotProduct(A.P - CurApex, CurRayDir);
			const float tB = FVector2D::DotProduct(B.P - CurApex, CurRayDir);
			return tA < tB;
		});
	}

	//UE_LOG(LogTemp, Log, TEXT("Rays2: %d"), intersectionPointsByRays.Num());

	auto CountVOForPoint = [&](int I, FVector2D P)->int
	{
		int Count = 0;
		for (int i = 0; i < VOCones.Num(); i++)
		{
			FVector2D V_B = VOCones[i].Apex;
			float LeftDot = FVector2D::DotProduct(P  - V_B, VOCones[i].LeftRayNormal);
			float RightDot = FVector2D::DotProduct(P  - V_B, VOCones[i].RightRayNormal);
			float THDot = FVector2D::DotProduct(P  - VOCones[i].LeftRayApex /* or right */, VOCones[i].TimeHorizonNormal);
			if (LeftDot >= 0.f && RightDot >= 0.f && THDot >= 0.f)
				Count++;
		}
		return Count;
	};

	UE_LOG(LogTemp, Log, TEXT("Rays3"));
	TArray<TArray<FVOOutsideSegment>> OutsideSegmentsByRays;
	OutsideSegmentsByRays.SetNum(intersectionPointsByRays.Num());
	for (int i = 0; i < OutsideSegmentsByRays.Num(); i++)
	{
		UE_LOG(LogTemp, Log, TEXT("%d"), intersectionPointsByRays[i].Num());
		OutsideSegmentsByRays[i].Reserve(FMath::Max(0, intersectionPointsByRays[i].Num() - 1));
		//OutsideSegmentsByRays[i].Reserve(intersectionPointsByRays[i].Num() - 1);
	}

	// Classify segments
	for (int i = 0; i < intersectionPointsByRays.Num(); i++)
	{
		FVector2D	CurApex = (i % 2 == 0) ? VOCones[i/2].LeftRayApex : VOCones[i/2].RightRayApex;
		FVector2D	CurNormal = (i % 2 == 0) ? VOCones[i/2].LeftRayNormal : VOCones[i/2].RightRayNormal;
		float		CurOffset = (i % 2 == 0) ? VOCones[i/2].LeftRayOffset : VOCones[i/2].RightRayOffset;

		int CountOfVOs = CountVOForPoint(i, CurApex);

		for (int j = 1; j < intersectionPointsByRays[i].Num(); j++)
		{
			if (CountOfVOs == 0)
			{
				FVOOutsideSegment OutsideSegment = {
					intersectionPointsByRays[i][j-1].P,
					intersectionPointsByRays[i][j].P,
					CurNormal,
					CurOffset,
				};
				OutsideSegmentsByRays[i].Add(OutsideSegment);
			}
		}
	}

	if (bDebugDraw && CVarVODebugShow.GetValueOnAnyThread() != 0)
	{
		FlushPersistentDebugLines(GetWorld());
		DrawCombinedVO(OutsideSegmentsByRays);
		DrawVOCones(VOCones);
	}

	return DesiredVel;
}

FVOCone UVOFollowingComponent::ComputeVOCone(const float R, const FVector2D& C, const FVector2D& Vel) const
{
	FVOCone Cone;

	Cone.Apex = Vel;
	
	float CSizeSquared = C.X * C.X + C.Y * C.Y;
	float RR = R * R;
	FVector2D P = RR * FVector2D(C.X, C.Y);
	FVector2D Q = R * FMath::Sqrt(CSizeSquared - RR) * FVector2D(C.Y, -C.X);

	// Rays
	// Right -> S=-1 ; Left -> S=1
	float DenominatorInverted = 1.f / CSizeSquared * R;
	Cone.RightRayNormal = (P - Q) * DenominatorInverted;
	Cone.LeftRayNormal	= (P + Q) * DenominatorInverted;
	Cone.RightRayOffset = -FVector2D::DotProduct(Cone.RightRayNormal, Vel);
	Cone.LeftRayOffset	= -FVector2D::DotProduct(Cone.LeftRayNormal, Vel);

	// Time Horizon
	float CSize = FMath::Sqrt(CSizeSquared);
	FVector2D PTimeHorizon = ((CSize - R) / (CSize * Params.TauHorizon)) * C + Vel;
	Cone.TimeHorizonNormal = C / CSize;
	Cone.TimeHorizonOffset = -FVector2D::DotProduct(Cone.TimeHorizonNormal, PTimeHorizon);
	
	// Rays apexes
	float LDeterminantInverted = 1.f / (Cone.TimeHorizonNormal.X * Cone.LeftRayNormal.Y  - Cone.TimeHorizonNormal.Y * Cone.LeftRayNormal.X);
	Cone.LeftRayApex = FVector2D(
		(Cone.TimeHorizonNormal.Y * Cone.LeftRayOffset - Cone.LeftRayNormal.Y * Cone.TimeHorizonOffset) * LDeterminantInverted,
		(Cone.LeftRayNormal.X * Cone.TimeHorizonOffset - Cone.TimeHorizonNormal.X * Cone.LeftRayOffset) * LDeterminantInverted	
	);
	Cone.RightRayApex = PTimeHorizon + (PTimeHorizon - Cone.LeftRayApex);
	
	return Cone;
}

// TODO: Add ray constraints
bool UVOFollowingComponent::TryFindIntersections(const float A1, const float B1, const float C1,
												 const float A2, const float B2, const float C2,
                                                 FVector2D* OutPoint) const
{
	// Solve system of equations with Cramer's rule
	float D = A1 * B2 - A2 * B1;
	UE_LOG(LogTemp, Log, TEXT("D = %f"), D);
	if (FMath::IsNearlyZero(D, KINDA_SMALL_NUMBER))
		return false;

	float DInv = 1.f / D;
	UE_LOG(LogTemp, Log, TEXT("Intersection"));
	if (OutPoint)
	{
		*OutPoint = FVector2D((B1 * C2 - B2 * C1) * DInv, (C1 * A2 - C2 * A1) * DInv);
		UE_LOG(LogTemp, Log, TEXT("Intersection2"));
	}
	return true;
}

void UVOFollowingComponent::DrawVOCones(TArray<FVOCone>& Cone) const
{
    UWorld* W = GetWorld(); if (!W) return;

	FVector P = GetOwner()->GetActorLocation();

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

void UVOFollowingComponent::DrawCombinedVO(const TArray<TArray<FVOOutsideSegment>>& OutsideSegmentsByRays) const
{
	UWorld* W = GetWorld(); if (!W) return;

	FVector P = GetOwner()->GetActorLocation();

	//UE_LOG(LogTemp, Log, TEXT("Rays: %d"), OutsideSegmentsByRays.Num());

	FColor Color;
	for (int i = 0; i < OutsideSegmentsByRays.Num(); i++)
	{
		if (i % 2 == 0)
			Color = FColor::MakeRandomColor();
		for (int j = 0; j < OutsideSegmentsByRays[i].Num(); j++)
		{
			FVector Start	= { OutsideSegmentsByRays[i][j].P1.X, OutsideSegmentsByRays[i][j].P1.Y, 0.f };
			FVector End		= { OutsideSegmentsByRays[i][j].P2.X, OutsideSegmentsByRays[i][j].P2.Y, 0.f };
			UE_LOG(LogTemp, Log, TEXT("Start: %f, %f"), Start.X, Start.Y);
			DrawDebugLine(W, Start + P, End + P, Color, true, 15.f, 0, 1.2f);
		}
	}
}
