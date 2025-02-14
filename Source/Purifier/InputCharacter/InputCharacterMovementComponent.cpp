// Fill out your copyright notice in the Description page of Project Settings.


#include "InputCharacterMovementComponent.h"
#include "InputCharacter.h"
#include "Components/CapsuleComponent.h"
#include <Kismet/KismetSystemLibrary.h>

UInputCharacterMovementComponent::UInputCharacterMovementComponent()
{
	WallRunCollider = CreateDefaultSubobject<UCapsuleComponent>(TEXT("WallRunCollider"));
	WallRunCollider->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WallRunCollider->SetHiddenInGame(true);
}

void UInputCharacterMovementComponent::InitializeComponent()
{
    Super::InitializeComponent();

	if (AInputCharacter* OwnerCast = Cast<AInputCharacter>(GetOwner()))
	{
		InputCharacterOwner = OwnerCast;
		WallRunCollider->SetupAttachment(GetOwner()->GetRootComponent());
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Incorrect owner of the InputCharacterMovementComponent"));
	}
}

float UInputCharacterMovementComponent::GetMaxSpeed() const
{
	if (MovementMode != MOVE_Custom) return Super::GetMaxSpeed();

	switch (CustomMovementMode)
	{
	case CMOVE_WallRun:
		return MaxWallRunSpeed;
	default:
		UE_LOG(LogTemp, Fatal, TEXT("Invalid Movement Mode"))
			return -1.f;
	}
}

float UInputCharacterMovementComponent::GetMaxBrakingDeceleration() const
{
	if (MovementMode != MOVE_Custom) return Super::GetMaxBrakingDeceleration();

	switch (CustomMovementMode)
	{
	case CMOVE_WallRun:
		return 0.f;
	default:
		UE_LOG(LogTemp, Fatal, TEXT("Invalid Movement Mode"))
			return -1.f;
	}
}

bool UInputCharacterMovementComponent::CanAttemptJump() const
{
	return Super::CanAttemptJump() || IsWallRunning();
}

bool UInputCharacterMovementComponent::DoJump(bool bReplayingMoves)
{
	bool bWasWallRunning = IsWallRunning();
	if (Super::DoJump(bReplayingMoves))
	{
		if (bWasWallRunning)
		{
			FVector Start = UpdatedComponent->GetComponentLocation();
			FVector CastDirection = Velocity.Cross(FVector(0.f, 0.f, Safe_bWallRunIsLeft ? 1.f : -1.f));
			FVector CastDelta = CastDirection * CapR() * 2;
			FVector End = Start + CastDelta;
			auto Params = InputCharacterOwner->GetIgnoreCharacterParams();
			FHitResult WallHit;
			
			if (GetWorld()->LineTraceSingleByProfile(WallHit, Start, End, "BlockAll", Params))
			{
				Velocity += WallHit.ImpactNormal * WallJumpOffForce;
			}	
		}
		return true;
	}
	return false;
}


// Movement Pipeline
void UInputCharacterMovementComponent::UpdateCharacterStateBeforeMovement(float DeltaSeconds)
{
	// Wall Run
	if (IsFalling() || IsDashing())                                         
	{
		TryWallRun();
	}

	Super::UpdateCharacterStateBeforeMovement(DeltaSeconds);
}

void UInputCharacterMovementComponent::PhysCustom(float deltaTime, int32 Iterations)
{
	Super::PhysCustom(deltaTime, Iterations);
	
	FHitResult DashHit;
	switch (CustomMovementMode)
	{
	case CMOVE_Dash:
		SafeMoveUpdatedComponent(Velocity * deltaTime, UpdatedComponent->GetComponentQuat(), true, DashHit);
		if (DashHit.IsValidBlockingHit() && ((FVector::VectorPlaneProject(Velocity, DashHit.Normal).GetSafeNormal() | Velocity.GetSafeNormal()) >= FMath::Cos(FMath::DegreesToRadians(MaxWallTurnAngle))))
		{
			SlideAlongSurface(Velocity * deltaTime, 1.f - DashHit.Time, DashHit.Normal, DashHit, true);
		}
		break;
	case CMOVE_WallRun:
		PhysWallRun(deltaTime, Iterations);
		break;
	default:
		UE_LOG(LogTemp, Fatal, TEXT("Invalid Movement Mode"));
	}
}

// Movement Event
void UInputCharacterMovementComponent::OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode)
{
	Super::OnMovementModeChanged(PreviousMovementMode, PreviousCustomMode);

	if (PreviousMovementMode == MOVE_Custom && PreviousCustomMode == CMOVE_WallRun)
	{
		bCanWallRunSameSide = false;
		InputCharacterOwner->GetWorldTimerManager().SetTimer(WallRunSameSideCooldownHandle, this, &UInputCharacterMovementComponent::ResetWallRunSameSideCooldown, WallRunSameSideCooldown, false);
	}

	if (IsWallRunning())
	{
		InputCharacterOwner->CancelDash();
		TimeFromStart = 0.f;
	}
}

#pragma region Wall Run
bool UInputCharacterMovementComponent::TryWallRun()
{
	if (Velocity.SizeSquared2D() < pow(MinWallRunSpeed, 2)) return false;
	if (Velocity.Z < -MaxVerticalWallRunSpeed) return false;
	FVector Start = UpdatedComponent->GetComponentLocation();

	FVector SideAcceleration = Acceleration.ProjectOnTo(InputCharacterOwner->GetActorRightVector());
	FVector End = Start + (Velocity.GetSafeNormal2D() + SideAcceleration.GetSafeNormal2D()).GetSafeNormal() * TryWallRunTraceDisstance;
	auto Params = InputCharacterOwner->GetIgnoreCharacterParams();

	FHitResult FloorHit;
	// Check Player Height
	if (GetWorld()->LineTraceSingleByProfile(FloorHit, Start, Start + FVector::DownVector * (CapHH() + MinWallRunHeight), "BlockAll", Params))
	{
		return false;
	}

	FHitResult WallHitLine;
	//Line Trace In Direction of the Velocity + Acceleration
	GetWorld()->LineTraceSingleByProfile(WallHitLine, Start, End, "BlockAll", Params);
	if (!WallHitLine.IsValidBlockingHit())
	{
		return false;
	}

	//Capsule Trace To Wall
	TArray<FHitResult> OutHits;
	TArray<AActor*> ActorsToIgnore;
	End = Start - WallHitLine.ImpactNormal;
	InputCharacterOwner->GetAllChildActors(ActorsToIgnore);
	ActorsToIgnore.Add(InputCharacterOwner);

	bool bIsHitted = UKismetSystemLibrary::CapsuleTraceMultiByProfile(
		GetWorld(),
		Start,
		End,
		WallRunCollider->GetScaledCapsuleRadius(),
		WallRunCollider->GetScaledCapsuleHalfHeight(),
		"BlockAll",
		false,
		ActorsToIgnore,
		EDrawDebugTrace::ForDuration,
		OutHits,
		true
	);

	FHitResult* WallHitCapsule = OutHits.FindByPredicate([WallHitLine](const FHitResult& Hit)
	{
		return (Hit.Normal | WallHitLine.Normal) > 0;
	});

	if (!WallHitCapsule)
		return false;

	float ApproachSpeed = (Velocity + SideAcceleration)/2 | -WallHitCapsule->ImpactNormal;
	if (!SurfaceIsWallRunnable(WallHitCapsule->ImpactNormal) || ApproachSpeed < MinApproachSpeedForWallRun)
		return false;

	FVector ProjectedVelocity = FVector::VectorPlaneProject(Velocity, WallHitCapsule->ImpactNormal);
	if (ProjectedVelocity.SizeSquared2D() < pow(MinWallRunSpeed, 2)) 
		return false;

	
	bool bNewWallRunIsLeft = (ProjectedVelocity.GetSafeNormal2D().Cross(WallHitCapsule->ImpactNormal) | FVector::UpVector) > 0;
	if (bNewWallRunIsLeft == Safe_bWallRunIsLeft && !bCanWallRunSameSide)
		return false;

	// Passed all conditions
	Safe_bWallRunIsLeft = bNewWallRunIsLeft;
	Velocity = ProjectedVelocity;
	Velocity.Z = FMath::Clamp(Velocity.Z, 0.f, MaxVerticalWallRunSpeed);
	SetMovementMode(MOVE_Custom, CMOVE_WallRun);
	GEngine->AddOnScreenDebugMessage(-1, 2, FColor::Yellow, "Starting WallRun"); 
	return true;
}

bool UInputCharacterMovementComponent::SurfaceIsWallRunnable(const FVector SurfaceNormal) const
{
	if (SurfaceNormal.Z < -0.05f)
	{
		return false;
	}

	FVector SurfaceNormalProjection = FVector(SurfaceNormal.X, SurfaceNormal.Y, 0).GetSafeNormal();
	float angle = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(SurfaceNormalProjection, SurfaceNormal)));
	return angle < GetWalkableFloorAngle();
}

void UInputCharacterMovementComponent::PhysWallRun(float deltaTime, int32 Iterations)
{
	if (deltaTime < MIN_TICK_TIME)
	{
		return;
	}
	if (!CharacterOwner || (!CharacterOwner->Controller && !bRunPhysicsWithNoController && !HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity() && (CharacterOwner->GetLocalRole() != ROLE_SimulatedProxy)))
	{
		Acceleration = FVector::ZeroVector;
		Velocity = FVector::ZeroVector;
		return;
	}

	bJustTeleported = false;
	float remainingTime = deltaTime;
	// Perform the move
	while ((remainingTime >= MIN_TICK_TIME) && (Iterations < MaxSimulationIterations) && CharacterOwner && (CharacterOwner->Controller || bRunPhysicsWithNoController || (CharacterOwner->GetLocalRole() == ROLE_SimulatedProxy)))
	{
		Iterations++;
		bJustTeleported = false;
		const float timeTick = GetSimulationTimeStep(remainingTime, Iterations);
		remainingTime -= timeTick;
		const FVector OldLocation = UpdatedComponent->GetComponentLocation();

		//Pull away check
		float SinPullAwayAngle = FMath::Sin(FMath::DegreesToRadians(WallRunPullAwayAngle));
		FHitResult WallHit;
		TraceToWall(WallHit);
		bool bWantsToPullAway = WallHit.IsValidBlockingHit() && !Acceleration.IsNearlyZero() && (Acceleration.GetSafeNormal() | WallHit.Normal) > SinPullAwayAngle;
		if (!WallHit.IsValidBlockingHit() || bWantsToPullAway)
		{
			Velocity += WallHit.ImpactNormal * WallJumpOffForce;
			GEngine->AddOnScreenDebugMessage(-1, 2, FColor::Yellow, "Pull Away");
			SetMovementMode(MOVE_Falling);
			StartNewPhysics(remainingTime, Iterations);

			return;
		}

		// Clamp Acceleration
		Acceleration = FVector::VectorPlaneProject(Acceleration, WallHit.Normal);
		// Apply acceleration
		float Friction = (Velocity.SizeSquared2D() > pow(MaxWallRunSpeed, 2)) ? WallRunFriction : 0.f;
		float WallRunVelocityZ = Velocity.Z; Velocity.Z = 0.f;
		CalcVelocity(timeTick, Friction, false, GetMaxBrakingDeceleration());
		
		if (TimeFromStart <= AccelerationUpTime)
			Velocity.Z = AccelerationUp * WallRunAccelerationUpCurve->GetFloatValue(TimeFromStart / AccelerationUpTime);
		else
			Velocity.Z = WallRunVelocityZ > 0 ? 0 : WallRunVelocityZ;
		Velocity = FVector::VectorPlaneProject(Velocity, WallHit.Normal);         
		
		float TangentAccel = Acceleration.GetSafeNormal2D() | Velocity.GetSafeNormal2D();
		bool bForceGravity =  TimeFromStart >= FallStartTime;
		float WallRunGravityScale = WallRunGravityScaleCurve->GetFloatValue(bForceGravity ? 0.f : TangentAccel);
		Velocity.Z += GetGravityZ() * WallRunGravityScale * timeTick;   //TODO SLOW DOWN

		if (Velocity.SizeSquared2D() < pow(MinWallRunSpeed, 2) || Velocity.Z < -MaxVerticalWallRunSpeed)
		{
			Velocity += WallHit.ImpactNormal * WallJumpOffForce;
			GEngine->AddOnScreenDebugMessage(-1, 2, FColor::Yellow, "Bad Velocity");
			SetMovementMode(MOVE_Falling);
			StartNewPhysics(remainingTime, Iterations);

			return;
		}

		// Compute move parameters
		const FVector Delta = timeTick * Velocity; // dx = v * dt
		const bool bZeroDelta = Delta.IsNearlyZero();
		bool bWasMoved;

		FHitResult MoveHit(1.f);
		if (bZeroDelta)
		{
			remainingTime = 0.f;
			break;
		}
		else
		{
			bWasMoved = SafeMoveUpdatedComponent(Delta, UpdatedComponent->GetComponentQuat(), true, MoveHit);

			if (MoveHit.IsValidBlockingHit() && ((WallHit.ImpactNormal | MoveHit.ImpactNormal) >= FMath::Cos(FMath::DegreesToRadians(MaxWallTurnAngle))))
			{
				SlideAlongSurface(Delta, 1.f - MoveHit.Time, MoveHit.Normal, MoveHit, true);
				WallHit = MoveHit;
			}

			FHitResult Hit;
			FVector WallAttractionDelta = -WallHit.Normal * WallAttractionForce * timeTick;
			SafeMoveUpdatedComponent(WallAttractionDelta, UpdatedComponent->GetComponentQuat(), true, Hit);
		}

		if (UpdatedComponent->GetComponentLocation().Equals(OldLocation, 1e-3f))
		{
			Velocity = FVector::Zero();
			remainingTime = 0.f;
			break;
		}

		Velocity = (UpdatedComponent->GetComponentLocation() - OldLocation) / timeTick; // v = dx / dt
		OnComponentWallRelativeRotationChanged.Broadcast(CalculateRelativeRotationAlpha());
		TimeFromStart += timeTick;
		

		FVector StartD = UpdatedComponent->GetComponentLocation();  
		FVector EndD = StartD + Velocity * 0.1f; 
		DrawDebugLine(GetWorld(), StartD, EndD, FColor::Blue, false, 0.f, 0, 2.0f);
	}

	GEngine->AddOnScreenDebugMessage(-1, 0.f, FColor::Green, FString::Printf(TEXT("Speed: %.2f"), Velocity.Length()));

	FVector Start = UpdatedComponent->GetComponentLocation();
	FVector CastDirection = Velocity.GetSafeNormal2D().Cross(FVector(0.f, 0.f, Safe_bWallRunIsLeft ? 1.f : -1.f));
	FVector CastDelta = CastDirection * CapR() * 2;
	FVector End = Start + CastDelta;
	auto Params = InputCharacterOwner->GetIgnoreCharacterParams();
	FHitResult FloorHit, WallHit;
	GetWorld()->LineTraceSingleByProfile(WallHit, Start, End, "BlockAll", Params);
	GetWorld()->LineTraceSingleByProfile(FloorHit, Start, Start + FVector::DownVector * (CapHH() + MinWallRunHeight * .5f), "BlockAll", Params);
	
	if (FloorHit.IsValidBlockingHit() || !WallHit.IsValidBlockingHit() || Velocity.SizeSquared2D() < pow(MinWallRunSpeed, 2))
	{
		Velocity += WallHit.ImpactNormal * WallJumpOffForce;
		SetMovementMode(MOVE_Falling);
		GEngine->AddOnScreenDebugMessage(-1, 2, FColor::Yellow, "Too close to ground / No wall / Too low speed");
	}
}
#pragma endregion Wall Run

void UInputCharacterMovementComponent::StartDash()
{
	SetMovementMode(MOVE_Custom, static_cast<uint8>(ECustomMovementMode::CMOVE_Dash));
}

void UInputCharacterMovementComponent::StopDash()
{
	if (IsDashing()) SetMovementMode(MOVE_Falling); // Выход из рывка
}


#pragma region Helpers
bool UInputCharacterMovementComponent::IsCustomMovementMode(ECustomMovementMode InCustomMovementMode) const
{
	return MovementMode == MOVE_Custom && CustomMovementMode == InCustomMovementMode;
}
bool UInputCharacterMovementComponent::IsMovementMode(EMovementMode InMovementMode) const
{
	return InMovementMode == MovementMode;
}

float UInputCharacterMovementComponent::CapR() const
{
	return CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleRadius();
}

float UInputCharacterMovementComponent::CapHH() const
{
	return CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
}

bool UInputCharacterMovementComponent::TraceToWall(FHitResult& OutHit) const
{
	FVector Start = UpdatedComponent->GetComponentLocation();
	FVector CastDirection = Velocity.GetSafeNormal2D().Cross(FVector(0.f, 0.f, Safe_bWallRunIsLeft ? 1.f : -1.f));
	FVector CastDelta = CastDirection * CapR() * 2;
	FVector End = Start + CastDelta;
	auto Params = InputCharacterOwner->GetIgnoreCharacterParams();

	return GetWorld()->LineTraceSingleByProfile(OutHit, Start, End, "BlockAll", Params);
}

float UInputCharacterMovementComponent::CalculateRelativeRotationAlpha() const
{
	return (Safe_bWallRunIsLeft ? 1.f : -1.f) * (InputCharacterOwner->GetActorForwardVector() | Velocity.GetSafeNormal2D()) ;
}

void UInputCharacterMovementComponent::ResetWallRunSameSideCooldown()
{
	bCanWallRunSameSide = true;
}
#pragma endregion Helpers