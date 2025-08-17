#include "VOFollowingComponent.h"
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
    if (bDebugDraw && CVarVODebugShow.GetValueOnAnyThread() != 0)
    {
    	FlushPersistentDebugLines(GetWorld());
    	DrawVOConesTau(Pos, Neis);
    }
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
    if (PV >= 0.f) // Moving away or tangent; 
        return false;

    const float VV = RelativeVelocity.SizeSquared();
    const float PP = RelativePosition.SizeSquared();
    const float R2 = Radius*Radius;

    const float a = VV;
    const float b = 2.f * PV;
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

    auto IsForbidden = [&](const FVector2D& vA2D)->bool
    {
        for (const FVONeighborView& N : Neis)
        {
            const FVector2D pRel(N.Pos.X - actorPos.X, N.Pos.Y - actorPos.Y);
            const FVector2D vRel = vA2D - FVector2D(N.Vel.X, N.Vel.Y);
            const float R = Params.AgentRadius + N.Radius;
            if (WillCollideWithinTau(pRel, vRel, R, Params.TauHorizon, nullptr))
                return true;
        }
        return false;
    };

    // Try desired
    if (!IsForbidden(FVector2D(DesiredVel.X, DesiredVel.Y)))
        return DesiredVel.GetClampedToMaxSize2D(Params.MaxSpeed);

	const FVector2D vDes2(DesiredVel.X, DesiredVel.Y);
	
    /*TArray<FVector> Cands;
	Cands.Reserve(Neis.Num()*4 + 4);*/

	// Construct all velocity obstacles
    TArray<FVOCone> VOCones;
	VOCones.Reserve(Neis.Num());

	for (const FVONeighborView& N : Neis)
	{
		float R = Params.AgentRadius + N.Radius;							//Minkowski sum radius
		FVector2D pRel(N.Pos.X - actorPos.X, N.Pos.Y - actorPos.Y);	//Relative position of a neighbor

		

		// TODO: Case when we grazing N
		if (R*R < pRel.SizeSquared()) 
		{
			UE_LOG(LogTemp, Warning, TEXT("R*R < pRel.SizeSquared()"));
			continue;
		}

		
			
		
	}
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

void UVOFollowingComponent::DrawVOConesTau(const FVector& P, const TArray<FVONeighborView>& Neis) const
{
    UWorld* W = GetWorld(); if (!W) return;

    const float S = 1.f; // world cm per (cm/s)
    const int ArcSegs = 16;

    auto Rot2D = [](const FVector2D& v, float a){ const float c=FMath::Cos(a), s=FMath::Sin(a); return FVector2D(v.X*c - v.Y*s, v.X*s + v.Y*c); };

    for (const FVONeighborView& N : Neis)
    {
        const FVector2D pRel(N.Pos.X - P.X, N.Pos.Y - P.Y);
        const float d = pRel.Size(); if (d < 1.f) continue;
        const float R = Params.AgentRadius + N.Radius;
        if (R >= d) continue; // degenerate
        const float alpha = FMath::Asin(FMath::Clamp(R/d, 0.f, 0.999f));

        // Velocity-space quantities
        const FVector2D vB(N.Vel.X, N.Vel.Y);    // apex (neighbor's velocity)
        const float tau = FMath::Max(0.001f, Params.TauHorizon);
        const FVector2D Cc = vB - pRel / tau;    // circle center for TOI=tau
        const float rTau = R / tau;
        const float dTau = (Cc - vB).Size();     // == d/tau

        // Tangent directions from apex to circle (using direction to center: cdir)
        const FVector2D cdir = (Cc - vB).GetSafeNormal(); // == -normalize(pRel)
        const FVector2D uL = Rot2D(cdir, +alpha);
        const FVector2D uR = Rot2D(cdir, -alpha);

        // Tangent points along those directions
        const float lenTan = FMath::Sqrt(FMath::Max(0.f, dTau*dTau - rTau*rTau));
        const FVector2D TL = vB + uL * lenTan;
        const FVector2D TR = vB + uR * lenTan;

        // Map to world for drawing (anchor at agent position P)
        const FVector A = P + FVector(vB.X, vB.Y, 0.f) * S;         // apex point
        const FVector WL = P + FVector(TL.X, TL.Y, 0.f) * S;        // left tangent point
        const FVector WR = P + FVector(TR.X, TR.Y, 0.f) * S;        // right tangent point
        const FVector Cw = P + FVector(Cc.X, Cc.Y, 0.f) * S;        // circle center

        // Finite edges (apex -> tangent points)
        DrawDebugLine(W, A, WL, DebugDrawColor, true, 15.f, 0, 1.5f);
        DrawDebugLine(W, A, WR, DebugDrawColor, true, 15.f, 0, 1.5f);

        // Draw the VO^tau arc (minor arc between TL and TR)
        const float a0 = FMath::Atan2((TL - Cc).Y, (TL - Cc).X);
        const float a1 = FMath::Atan2((TR - Cc).Y, (TR - Cc).X);
        float dAng = FMath::FindDeltaAngleRadians(a0, a1); // shortest signed delta in [-pi,pi]
        const float step = dAng / float(ArcSegs);
        FVector prev = WL;
        for (int i=1; i<=ArcSegs; ++i)
        {
            const float a = a0 + step * i;
            const FVector pt = Cw + FVector(FMath::Cos(a), FMath::Sin(a), 0.f) * (rTau * S);
            DrawDebugLine(W, prev, pt, DebugDrawColor, true, 15.f, 0, 1.2f);
            prev = pt;
        }

        // Optional markers
        DrawDebugPoint(W, A, 4.f, FColor::Cyan, false, 0.f, 0);
        DrawDebugPoint(W, Cw, 4.f, FColor::Green, false, 0.f, 0);
    }
}
