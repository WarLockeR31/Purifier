// Fill out your copyright notice in the Description page of Project Settings.


#include "Purifier/InputPlayer/InputCharacter.h"
#include <EnhancedInputSubsystems.h>
#include <EnhancedInputComponent.h>

#include "Camera/CameraComponent.h"
#include "Components/TimelineComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include <format>
// Sets default values
AInputCharacter::AInputCharacter()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	Camera = CreateDefaultSubobject<UCameraComponent>("Camera");
	Camera->SetupAttachment(RootComponent);
	Camera->bUsePawnControlRotation = true;

	DashTimeline = CreateDefaultSubobject<UTimelineComponent>(TEXT("DashTimeline"));
}

// Called when the game starts or when spawned
void AInputCharacter::BeginPlay()
{
	Super::BeginPlay();


	FOnTimelineFloat DashProgress;
	DashProgress.BindUFunction(this, FName("DashTimelineProgress"));
	DashTimeline->AddInterpFloat(DashCurve, DashProgress);
	DashTimeline->SetPlayRate(1.f / DashDuration);

	FOnTimelineEvent TimelineFinishedCallback;
	TimelineFinishedCallback.BindUFunction(this, FName("OnDashFinished"));
	DashTimeline->SetTimelineFinishedFunc(TimelineFinishedCallback);
	

	DashSpeedCoefficient = GetSpeedCoefficient();
}

// Called every frame
void AInputCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

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
	if (bIsDashing)
		return;

	GetCharacterMovement()->StopMovementImmediately();
	bIsDashing = true;
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
	LaunchCharacter(FVector(0.0001, 0, 0), true, true);
	GetCharacterMovement()->BrakingFrictionFactor = 2.f;

	GetWorldTimerManager().SetTimer(DashHandle, this, &AInputCharacter::ResetDashCooldown, DashCooldown, false);
}

void AInputCharacter::ResetDashCooldown()
{
	bIsDashing = false;
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
