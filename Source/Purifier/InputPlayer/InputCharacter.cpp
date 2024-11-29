// Fill out your copyright notice in the Description page of Project Settings.


#include "Purifier/InputPlayer/InputCharacter.h"
#include <EnhancedInputComponent.h>
#include <EnhancedInputSubsystems.h>

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/TimelineComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include <Kismet/KismetMathLibrary.h>
#include <Purifier/InputPlayer/HandSwayComponent.h>


// Sets default values
AInputCharacter::AInputCharacter()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	Camera = CreateDefaultSubobject<UCameraComponent>("Camera");
	//GetMesh()->SetupAttachment(Camera);
	Camera->SetupAttachment(RootComponent);
	Camera->bUsePawnControlRotation = true;

	GetMesh()->SetupAttachment(Camera);

	HandSwayComponent = CreateDefaultSubobject<UHandSwayComponent>("HandSway");

	WalkingTimeline = CreateDefaultSubobject<UTimelineComponent>(TEXT("WalkingTimeline"));
	WallRunTimeline = CreateDefaultSubobject<UTimelineComponent>(TEXT("WallRunTimeline"));
	DashTimeline = CreateDefaultSubobject<UTimelineComponent>(TEXT("DashTimeline"));
	
	GetCapsuleComponent()->OnComponentHit.AddDynamic(this, &AInputCharacter::OnCollisionHit);  //WallRun on
}

// Called when the game starts or when spawned
void AInputCharacter::BeginPlay()
{
	Super::BeginPlay();

	GetCharacterMovement()->MaxAcceleration = 100000.f;

	FOnTimelineFloat DashProgress;
	DashProgress.BindUFunction(this, FName("DashTimelineProgress"));
	DashTimeline->AddInterpFloat(DashCurve, DashProgress);
	DashTimeline->SetPlayRate(1.f / DashDuration);

	FOnTimelineEvent TimelineFinishedCallback;
	TimelineFinishedCallback.BindUFunction(this, FName("OnDashFinished"));
	DashTimeline->SetTimelineFinishedFunc(TimelineFinishedCallback);

	FOnTimelineFloat WallRunProgress;
	WallRunProgress.BindUFunction(this, FName("UpdateWallRun"));
	WallRunTimeline->AddInterpFloat(DashCurve, WallRunProgress);
	WallRunTimeline->SetLooping(true);

	FOnTimelineFloat WalkingProgress;
	WalkingProgress.BindUFunction(this, FName("UpdateWalkingHandSway"));
	WalkingTimeline->AddInterpFloat(WalkingLeftRightCurve, WalkingProgress);
	/*WalkingTimeline->AddInterpFloat(WalkingUpDownCurve, WalkingProgress);
	WalkingTimeline->AddInterpFloat(WalkingRollCurve, WalkingProgress);*/
	WalkingTimeline->SetLooping(true);
	WalkingTimeline->PlayFromStart();
	

	DashSpeedCoefficient = GetSpeedCoefficient();

	BaseAirControl = GetCharacterMovement()->AirControl;
}

// Called every frame
void AInputCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	//UpdateLocationLagPos();
}

// Called to bind functionality to input
void AInputCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	//Add input mapping context
	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		//Get local player subsystem
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			//Add input context
			Subsystem->AddMappingContext(InputMapping, 0);
		}
	}

	if (UEnhancedInputComponent* Input = CastChecked<UEnhancedInputComponent>(PlayerInputComponent))
	{
		Input->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AInputCharacter::Move);
		Input->BindAction(LookAction, ETriggerEvent::Triggered, this, &AInputCharacter::Look);
		Input->BindAction(JumpAction, ETriggerEvent::Triggered, this, &AInputCharacter::Jump);
		Input->BindAction(DashAction, ETriggerEvent::Triggered, this, &AInputCharacter::StartDash);  //Dash on
	}
}

//Move character according to the input
void AInputCharacter::Move(const FInputActionValue& InputValue)
{
	MoveInputVector = InputValue.Get<FVector2D>();
	if (IsValid(Controller) && !bWallRunning)
	{
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		AddMovementInput(ForwardDirection, MoveInputVector.Y);
		AddMovementInput(RightDirection, MoveInputVector.X);
	}
}

void AInputCharacter::Look(const FInputActionValue& InputValue)
{
	FVector2D InputVector = InputValue.Get<FVector2D>();
	if (IsValid(Controller))
	{
		AddControllerYawInput(InputVector.X);
		AddControllerPitchInput(InputVector.Y);
	}
} 

void AInputCharacter::Jump()
{
	Super::Jump();
} 

#pragma region Dash
//_____________________________________________________________________________________________________
void AInputCharacter::StartDash()
{
	if (bDashing)
		return;

	GetCharacterMovement()->StopMovementImmediately();
	bDashing = true;
	GetCharacterMovement()->BrakingFrictionFactor = 0.f;
	

	const FRotator Rotation = Controller->GetControlRotation();
	const FRotator YawRotation(0, Rotation.Yaw, 0);

	const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	DashVector = (ForwardDirection * MoveInputVector.Y + RightDirection * MoveInputVector.X).GetSafeNormal();

	//Timeline start
	DashTimeline->PlayFromStart();
}

void AInputCharacter::DashTimelineProgress(float Value)
{
	LaunchCharacter(DashVector * Value * DashSpeedCoefficient, true, true);
}

void AInputCharacter::OnDashFinished()
{
	LaunchCharacter(DashVector * DashDistance * 50.f, true, true);
	GetCharacterMovement()->BrakingFrictionFactor = 2.f;

	GetWorldTimerManager().SetTimer(DashHandle, this, &AInputCharacter::ResetDashCooldown, DashCooldown, false);
}

void AInputCharacter::ResetDashCooldown()
{
	bDashing = false;
}

float AInputCharacter::GetSpeedCoefficient() const
{
	float MinTime, MaxTime;
	DashCurve->GetTimeRange(MinTime, MaxTime);

	float step = (MaxTime - MinTime) / 200.f;
	float ApproximateCurveS = 0.f;
	for (float i = MinTime; i < MaxTime; i += step)
	{
		ApproximateCurveS += DashCurve->GetFloatValue(i + step) * step;
	}

	return DashDistance / ApproximateCurveS / DashDuration * 100.f;
}

//_____________________________________________________________________________________________________
#pragma endregion Dash

void AInputCharacter::OnCollisionHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (bWallRunning || !SurfaceIsWallRunnable(Hit.ImpactNormal))
	{
		return;
	}

	if (GetActorRightVector().Dot(Hit.ImpactNormal) > 0)
	{
		WallRunSide = EWallRunSide::Left;
		WallRunDirection = Hit.ImpactNormal.Cross(FVector(0.f, 0.f, 1.f));
	}
	else
	{
		WallRunSide = EWallRunSide::Right;
		WallRunDirection = Hit.ImpactNormal.Cross(FVector(0.f, 0.f, -1.f));
	}

	if (AreRequiredKeysDown())
	{
		StartWallRun();
	}
}

#pragma region WallRun
//_____________________________________________________________________________________________________
bool AInputCharacter::SurfaceIsWallRunnable(const FVector SurfaceNormal) const
{
	if (SurfaceNormal.Z < -0.05f)
	{
		return false;
	}

	FVector SurfaceNormalProjection = FVector(SurfaceNormal.X, SurfaceNormal.Y, 0).GetSafeNormal();
	float angle = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(SurfaceNormalProjection, SurfaceNormal)));

	return angle < GetCharacterMovement()->GetWalkableFloorAngle();
}

bool AInputCharacter::AreRequiredKeysDown() const
{
	if (MoveInputVector.Y < 0.1f)
	{ 
		return false;
	}

	return MoveInputVector.X > 0.1f  && (WallRunSide == EWallRunSide::Right) || 
		   MoveInputVector.X < -0.1f && (WallRunSide == EWallRunSide::Left);
}

void AInputCharacter::StartWallRun()
{
	GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Green, TEXT("StartedWallRun"));

	bWallRunning = true;
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->AirControl = 1.f;
	GetCharacterMovement()->GravityScale = 0.f;
	GetCharacterMovement()->SetPlaneConstraintNormal(FVector(0.f, 0.f, 1.f));
	WallRunTimeline->Play();
}

void AInputCharacter::UpdateWallRun()
{
	if (!AreRequiredKeysDown())
	{
		EndWallRun();
		return;
	}

	FHitResult Hit;
	TArray<AActor*> actorsToIgnore;
	actorsToIgnore.Add(this);
	FVector EndLocation = GetActorLocation() + WallRunDirection.Cross(FVector(0.f, 0.f, WallRunSide == EWallRunSide::Left ? 1.f : -1.f)) * 90.f;
		
	if (!UKismetSystemLibrary::LineTraceSingle(GetWorld(), GetActorLocation(), EndLocation, UEngineTypes::ConvertToTraceType(ECC_Visibility), false, actorsToIgnore, EDrawDebugTrace::ForOneFrame, Hit, true))
	{
		EndWallRun();
		return;
	}

	if (GetActorRightVector().Dot(Hit.ImpactNormal) > 0)
	{
		if (WallRunSide != EWallRunSide::Left)
		{
			EndWallRun();
			return;
		}

		WallRunSide = EWallRunSide::Left;
		WallRunDirection = Hit.ImpactNormal.Cross(FVector(0.f, 0.f, 1.f));
	}
	else
	{
		if (WallRunSide != EWallRunSide::Right)
		{
			EndWallRun();
			return;
		}

		WallRunSide = EWallRunSide::Right;
		WallRunDirection = Hit.ImpactNormal.Cross(FVector(0.f, 0.f, -1.f)); 
		
	}

	LaunchCharacter(FVector(WallRunDirection.X, WallRunDirection.Y, 0.f) * 2000.f, true, true);
}

void AInputCharacter::EndWallRun()
{
	WallRunTimeline->Stop();
	GetCharacterMovement()->SetPlaneConstraintNormal(FVector(0.f, 0.f, 0.f));
	GetCharacterMovement()->GravityScale = 1.f;
	GetCharacterMovement()->AirControl = BaseAirControl;
	bWallRunning = false;
	GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Green, TEXT("EndedWallRun"));
}


//_____________________________________________________________________________________________________
#pragma endregion WallRun

void AInputCharacter::UpdateWalkingHandSway(float Value)
{
	float CurTLTime = WalkingTimeline->GetPlaybackPosition();

	float WalkAnomOffsetX = FMath::Lerp(-0.4f, 0.4f, WalkingLeftRightCurve->GetFloatValue(CurTLTime));
	float WalkAnomOffsetZ = FMath::Lerp(-0.35f, 0.2f, WalkingUpDownCurve->GetFloatValue(CurTLTime));
	WalkAnimOffset.Set(WalkAnomOffsetX, 0.f, WalkAnomOffsetZ);

	//float WalkAnomOffsetZ = FMath::Lerp(1.f, -1.f, WalkingRollCurve->GetFloatValue(CurTLTime));
	//WalkAnimTilt.Pitch = FMath::Lerp(1.f, -1.f, WalkingRollCurve->GetFloatValue(CurTLTime));
	WalkAnimTilt = FRotator(0.f, FMath::Lerp(1.f, -1.f, WalkingRollCurve->GetFloatValue(CurTLTime)), 0.f);

	float NormalizedVelocity = UKismetMathLibrary::NormalizeToRange(GetVelocity().Length(), 0.f, BaseWalkSpeed);
	WalkAnimAlpha = GetCharacterMovement()->IsFalling() ? 0.f : NormalizedVelocity;

	WalkingTimeline->SetPlayRate(FMath::Lerp(0.f, 1.65f, WalkAnimAlpha));
	UpdateLocationLagPos();

	
}


void AInputCharacter::UpdateLocationLagPos()
{
	const FVector Velocity = this->GetVelocity();

	const FRotator Rotation = Controller->GetControlRotation();
	const FRotator YawRotation(0, Rotation.Yaw, 0);

	const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
	const FVector UpDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Z);


	float ForwardVelocity = FVector::DotProduct(Velocity, ForwardDirection);
	float RightVelocity = FVector::DotProduct(Velocity, RightDirection);
	float UpVelocity = FVector::DotProduct(Velocity, UpDirection);

	FVector NewLocationLagPos = 2 * FVector(RightVelocity / BaseWalkSpeed, ForwardVelocity / -BaseWalkSpeed, UpVelocity / -GetCharacterMovement()->JumpZVelocity);

	NewLocationLagPos = NewLocationLagPos.GetClampedToSize(0.f, 4.f);

	LocationLagPos = FMath::VInterpTo(LocationLagPos, NewLocationLagPos, GetWorld()->GetDeltaSeconds(), (1.f / GetWorld()->GetDeltaSeconds()) / 6.f);
}

FVector AInputCharacter::GetLocationLagPos()
{
	return LocationLagPos;
}

void AInputCharacter::OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode)
{
	Super::OnMovementModeChanged(PrevMovementMode, PreviousCustomMode);

	if (GetCharacterMovement()->IsFalling())
	{
		float NormalizedVelocity = UKismetMathLibrary::NormalizeToRange(GetVelocity().Length(), 0.f, BaseWalkSpeed);
		float CoyoteTimeModifier = FMath::Lerp(0.25f, 1.f, FMath::Clamp(NormalizedVelocity, 0.f, 1.f));

		float CorrecterCoyoteTime = CoyoteTime * CoyoteTimeModifier;

		GetWorld()->GetTimerManager().SetTimer(
			CoyoteTimerHandle,
			this,
			&AInputCharacter::OnCoyoteTimePassed,
			CorrecterCoyoteTime,
			false);
	}
}

void AInputCharacter::OnCoyoteTimePassed()
{

}

void AInputCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);

	GetWorld()->GetTimerManager().ClearTimer(CoyoteTimerHandle);
}

void AInputCharacter::CheckJumpInput(float DeltaTime)
{
	JumpCurrentCountPreJump = JumpCurrentCount;

	if (GetCharacterMovement())
	{
		if (bPressedJump)
		{
			// If this is the first jump and we're already falling,
			// then increment the JumpCount to compensate.
			const bool bFirstJump = JumpCurrentCount == 0;
			if (bFirstJump && GetCharacterMovement()->IsFalling() && GetWorld()->GetTimerManager().GetTimerRemaining(CoyoteTimerHandle) <= 0.f)
			{
				JumpCurrentCount++;
			}

			const bool bDidJump = CanJump() && GetCharacterMovement()->DoJump(bClientUpdating);
			if (bDidJump)
			{
				// Transition from not (actively) jumping to jumping.
				if (!bWasJumping)
				{
					JumpCurrentCount++;
					JumpForceTimeRemaining = GetJumpMaxHoldTime();
					OnJumped();
				}
			}

			bWasJumping = bDidJump;
		}
	}
}

void AInputCharacter::OnJumped_Implementation()
{
	Super::OnJumped_Implementation();

	GetWorld()->GetTimerManager().ClearTimer(CoyoteTimerHandle);
}
