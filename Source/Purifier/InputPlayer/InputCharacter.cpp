// Fill out your copyright notice in the Description page of Project Settings.


#include "Purifier/InputPlayer/InputCharacter.h"
#include <EnhancedInputSubsystems.h>
#include <EnhancedInputComponent.h>

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/TimelineComponent.h"
#include "GameFramework/CharacterMovementComponent.h"






// Sets default values
AInputCharacter::AInputCharacter()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	Camera = CreateDefaultSubobject<UCameraComponent>("Camera");
	Camera->SetupAttachment(RootComponent);
	Camera->bUsePawnControlRotation = true;

	WallRunTimeline = CreateDefaultSubobject<UTimelineComponent>(TEXT("WallRunTimeline"));
	DashTimeline = CreateDefaultSubobject<UTimelineComponent>(TEXT("DashTimeline"));
	
	
	CapsuleComponent = GetCapsuleComponent();
	CapsuleComponent->OnComponentHit.AddDynamic(this, &AInputCharacter::OnCollisionHit);
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
	WallRunTimeline->AddInterpFloat(WallRunCurve, WallRunProgress);
	WallRunTimeline->SetTimelineLength(1.f);
	WallRunTimeline->SetTimelineLengthMode(ETimelineLengthMode::TL_TimelineLength);
	WallRunTimeline->SetLooping(true);

	 //
	                                    //

	DashSpeedCoefficient = GetSpeedCoefficient();

	BaseAirControl = GetCharacterMovement()->AirControl;
}

// Called every frame
void AInputCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	//float CurrentSpeed = GetVelocity().Size();
	//GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Green, FString::Printf(TEXT("Current Speed: %f"), CurrentSpeed));
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
		Input->BindAction(DashAction, ETriggerEvent::Triggered, this, &AInputCharacter::StartDash);
	}
}

//Move character according to the input
void AInputCharacter::Move(const FInputActionValue& InputValue)
{
	MoveInputVector = InputValue.Get<FVector2D>();
	if (IsValid(Controller))
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
	//AddMovementInput(DashVector * Value * DashSpeedCoefficient * 100); ???
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

void AInputCharacter::OnCollisionHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	//GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Green, FString::Printf(TEXT("ImpactNormal: %f, %f, %f"), Hit.ImpactNormal.X, Hit.ImpactNormal.Y, Hit.ImpactNormal.Z));

	if (bWallRunning || !SurfaceIsWallRunnable(Hit.ImpactNormal))
	{
		return;
	}
	if (GetActorRightVector().Dot(Hit.ImpactNormal) > 0)
	{
		WallRunSide = EWallRunSide::Left;
		WallRunDirection = Hit.ImpactNormal.Cross(FVector(0.f, 0.f, -1.f));
	}
	else
	{
		WallRunSide = EWallRunSide::Right;
		WallRunDirection = Hit.ImpactNormal.Cross(FVector(0.f, 0.f, 1.f));
	}

	
	if (AreRequiredKeysDown())
	{
		StartWallRun();
	}
}

bool AInputCharacter::SurfaceIsWallRunnable(FVector SurfaceNormal)
{
	//GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Green, FString::Printf(TEXT("SurfaceNormal.Z: %f"), SurfaceNormal.Z));
	if (SurfaceNormal.Z < -0.05f)
	{
		return false;
	}

	FVector SurfaceNormalProjection = FVector(SurfaceNormal.X, SurfaceNormal.Y, 0).GetSafeNormal();
	float angle = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(SurfaceNormalProjection, SurfaceNormal)));

	//GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Green, FString::Printf(TEXT("Angle: %f"), angle));
	//GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Green, FString::Printf(TEXT("WalkAngle: %f"), GetCharacterMovement()->GetWalkableFloorAngle()));

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
	bWallRunning = true;
	GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Green, TEXT("StartedWallRun"));
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->AirControl = 1.f;
	GetCharacterMovement()->GravityScale = 0.f;
	GetCharacterMovement()->SetPlaneConstraintNormal(FVector(0.f, 0.f, 1.f));

	WallRunTimeline->Play();
}

void AInputCharacter::UpdateWallRun()
{
	GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Green, TEXT("1"));

	if (!AreRequiredKeysDown())
	{
		EndWallRun();
		return;
	}

	GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Green, TEXT("2"));

	FHitResult Hit;
	FVector EndLocation = GetActorLocation() + FVector(0.f, 0.f, WallRunSide == EWallRunSide::Right ? -1.f : 1.f).Cross(WallRunDirection) * 200.f;

	if (!GetWorld()->LineTraceSingleByChannel(Hit, GetActorLocation(), EndLocation, ECollisionChannel::ECC_Visibility))
	{
		EndWallRun();
		return;
	}

	GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Green, TEXT("3"));

	if (GetActorRightVector().Dot(Hit.ImpactNormal) > 0)
	{
		GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Green, TEXT("3.1"));
		if (WallRunSide != EWallRunSide::Left)
		{
			EndWallRun();
			return;
		}

		WallRunSide = EWallRunSide::Left;
		WallRunDirection = Hit.ImpactNormal.Cross(FVector(0.f, 0.f, -1.f));
	}
	else
	{
		GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Green, TEXT("3.2"));
		if (WallRunSide != EWallRunSide::Right)
		{
			EndWallRun();
			return;
		}

		WallRunSide = EWallRunSide::Right;
		WallRunDirection = Hit.ImpactNormal.Cross(FVector(0.f, 0.f, 1.f));
		
	}

	GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Green, TEXT("WallRunTick"));
	AddMovementInput(FVector(WallRunDirection.X, WallRunDirection.Y, 0.f));

}

void AInputCharacter::EndWallRun()
{
	WallRunTimeline->Stop();
	GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Green, TEXT("EndedWallRun"));
	GetCharacterMovement()->SetPlaneConstraintNormal(FVector(0.f, 0.f, 0.f));
	GetCharacterMovement()->GravityScale = 1.f;
	GetCharacterMovement()->AirControl = BaseAirControl; //Air Control

	bWallRunning = false;
}
