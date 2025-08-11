#include "VOFollowingComponent.h"
#include "VOWorldSubsystem.h"
#include "AIController.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "DrawDebugHelpers.h"
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
    const FVector NewVel = ComputeVO(CurVel, DesiredVel, Neis);

    // Acceleration limit
    FVector OutVel = CurVel;
    const FVector Delta = NewVel - CurVel;
    const float MaxDv = Params.MaxAccel * DeltaTime;
    const float DvLen = Delta.Size2D();
    if (DvLen > MaxDv && DvLen > KINDA_SMALL_NUMBER)
        OutVel += Delta.GetSafeNormal2D() * MaxDv;
    else
        OutVel = NewVel;

    // Feed to movement
    if (auto* Move = P->FindComponentByClass<UPawnMovementComponent>())
    {
        // RequestDirectMove expects a velocity-like vector (cm/s)
        Move->RequestDirectMove(OutVel, /*bForceMaxSpeed=*/false);
    }

    // Debug draw
    if (bDebugDraw && CVarVODebugShow.GetValueOnAnyThread() != 0)
    {
    	FlushPersistentDebugLines(GetWorld());
    	DrawVOCones(Pos, Neis);
    }
}

// Minimal VO sampling:
//  - Try desired velocity; if collision within Tau -> try a few rotated directions and reduced speeds; else stop.
static void GenerateCandidates(const FVector& Desired, float MaxSpeed, int32 AngleSamples, TArray<FVector>& Out)
{
    Out.Reset();
    const FVector2D d2(Desired.X, Desired.Y);
    const float dLen = d2.Size();
    const float baseSpd = (dLen > 1.f) ? FMath::Clamp(dLen, 0.f, MaxSpeed) : MaxSpeed;

    // Primary
    Out.Add(FVector(d2.GetSafeNormal() * baseSpd, 0.f));

    const int32 half = FMath::Max(1, AngleSamples/2);
    const float step = PI / float(AngleSamples); // up to ~180 deg sweep

    for (int32 i=1; i<=half; ++i)
    {
        const float ang = step * i;
        const float cosA = FMath::Cos(ang), sinA = FMath::Sin(ang);
        const FVector2D n = d2.IsNearlyZero() ? FVector2D(1,0) : d2.GetSafeNormal();
        // rotate +/-
        const FVector2D r1(n.X*cosA - n.Y*sinA, n.X*sinA + n.Y*cosA);
        const FVector2D r2(n.X*cosA + n.Y*sinA, -n.X*sinA + n.Y*cosA);
        Out.Add(FVector(r1*baseSpd,0));
        Out.Add(FVector(r2*baseSpd,0));
    }

    // Reduced speeds
    Out.Add(FVector(d2.GetSafeNormal() * (0.75f*baseSpd), 0));
    Out.Add(FVector(d2.GetSafeNormal() * (0.5f*baseSpd), 0));
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

bool UVOFollowingComponent::WillCollideWithinTau(const FVector2D& pRel, const FVector2D& vRel, float R, float Tau, float* OutTOI) const
{
    // Solve |p + t v|^2 = R^2, t in (0, Tau]. If approaching.
    const float pv = FVector2D::DotProduct(pRel, vRel);
    if (pv >= 0.f) // moving away or tangent; no future collision
        return false;

    const float vv = vRel.SizeSquared();
    const float pp = pRel.SizeSquared();
    const float R2 = R*R;

    const float a = vv;
    const float b = 2.f * pv;
    const float c = pp - R2;

    const float disc = b*b - 4.f*a*c;
    if (disc < 0.f || a < 1e-6f) return false;

    const float sqrtDisc = FMath::Sqrt(disc);
    const float t1 = (-b - sqrtDisc) / (2.f*a);
    const float t2 = (-b + sqrtDisc) / (2.f*a);

    float tHit = TNumericLimits<float>::Max();
    if (t1 > 0.f) tHit = t1; else if (t2 > 0.f) tHit = t2; else return false;

    if (tHit <= Tau)
    {
        if (OutTOI) *OutTOI = tHit;
        return true;
    }
    return false;
}

FVector UVOFollowingComponent::ComputeVO(const FVector& CurVel, const FVector& DesiredVel, const TArray<FVONeighborView>& Neis) const
{
    // Gather own state
    const FVector P = GetOwnerLocation();

    // First try desired
    auto Violates = [&](const FVector& v)->bool
    {
        const FVector2D vA(v.X, v.Y);
        for (const FVONeighborView& N : Neis)
        {
            const FVector2D pRel(N.Pos.X - P.X, N.Pos.Y - P.Y);
            const FVector2D vRel = vA - FVector2D(N.Vel.X, N.Vel.Y);
            const float R = Params.AgentRadius + N.Radius;
            if (WillCollideWithinTau(pRel, vRel, R, Params.TauHorizon, nullptr))
                return true;
        }
        return false;
    };

    if (!Violates(DesiredVel))
        return DesiredVel.GetClampedToMaxSize2D(Params.MaxSpeed);

    // Generate simple candidates around desired
    TArray<FVector> Cands;
    GenerateCandidates(DesiredVel, Params.MaxSpeed, Params.AngleSamples, Cands);

    float BestScore = TNumericLimits<float>::Max();
    FVector Best = FVector::ZeroVector;

    for (const FVector& v : Cands)
    {
        bool Bad = false; float MinTOI = Params.TauHorizon;
        const FVector2D vA(v.X, v.Y);
        for (const FVONeighborView& N : Neis)
        {
            const FVector2D pRel(N.Pos.X - P.X, N.Pos.Y - P.Y);
            const FVector2D vRel = vA - FVector2D(N.Vel.X, N.Vel.Y);
            const float R = Params.AgentRadius + N.Radius;
            float toi = 0.f;
            if (WillCollideWithinTau(pRel, vRel, R, Params.TauHorizon, &toi))
            {
                Bad = true;
                MinTOI = FMath::Min(MinTOI, toi);
                break;
            }
        }
        // Objective: prefer non-colliding, then max TOI, then closeness to desired, then small accel change
        const float desPen = (v - DesiredVel).Size2D();
        const float accPen = (v - CurVel).Size2D();
        const float collidePen = Bad ? (10000.f - 1000.f*MinTOI) : 0.f; // any collision is heavy penalty
        const float J = collidePen + desPen + 0.25f*accPen;
        if (J < BestScore)
        {
            BestScore = J; Best = v;
        }
    }

    if (Best.IsNearlyZero())
    {
        // Last resort: brake
        return FVector::ZeroVector;
    }
    return Best.GetClampedToMaxSize2D(Params.MaxSpeed);
}

void UVOFollowingComponent::DrawVOCones(const FVector& P, const TArray<FVONeighborView>& Neis) const
{
    UWorld* W = GetWorld(); if (!W) return;

    const float L = 150.f; // ray length for visualization
    const FColor Col(255, 64, 64);

    for (const FVONeighborView& N : Neis)
    {
        const FVector2D d(N.Pos.X - P.X, N.Pos.Y - P.Y);
        const float D = d.Size();
        if (D < 1.f) continue;
        const float r = Params.AgentRadius + N.Radius;
        const float s = FMath::Clamp(r / D, 0.f, 0.99f);
        const float alpha = FMath::Asin(s);
        const FVector2D n = d / D;

        // rotate n by +/- alpha in XY plane
        const float c = FMath::Cos(alpha), sA = FMath::Sin(alpha);
        const FVector2D left ( n.X*c - n.Y*sA, n.X*sA + n.Y*c );
        const FVector2D right( n.X*c + n.Y*sA, -n.X*sA + n.Y*c );

        const FVector L0 = FVector(P.X, P.Y, P.Z + 5.f);
        const FVector L1 = L0 + FVector(left.X,  left.Y,  0.f) * L;
        const FVector R1 = L0 + FVector(right.X, right.Y, 0.f) * L;

        DrawDebugLine(W, L0, L1, Col, true, 10.f, 0, 1.5f);
        DrawDebugLine(W, L0, R1, Col, true, 10.f, 0, 1.5f);

        // optional arc between left/right (approx)
        const int Segs = 8;
        for (int i=1; i<=Segs; ++i)
        {
            const float t0 = (i-1) / float(Segs);
            const float t1 = i / float(Segs);
            const float ang0 = -alpha + (2*alpha)*t0;
            const float ang1 = -alpha + (2*alpha)*t1;
            const float c0 = FMath::Cos(ang0), s0 = FMath::Sin(ang0);
            const float c1 = FMath::Cos(ang1), s1 = FMath::Sin(ang1); 
            const FVector2D a0(n.X*c0 - n.Y*s0, n.X*s0 + n.Y*c0);
            const FVector2D a1(n.X*c1 - n.Y*s1, n.X*s1 + n.Y*c1);
            DrawDebugLine(W,
                L0 + FVector(a0.X, a0.Y, 0)*L,
                L0 + FVector(a1.X, a1.Y, 0)*L,
                Col, true, 10.f, 0, 1.f);
        }
    }
}