// Fill out your copyright notice in the Description page of Project Settings.


#include "InputCharacter.h"
#include <EnhancedInputComponent.h>
#include <EnhancedInputSubsystems.h>
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/TimelineComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include <Kismet/KismetMathLibrary.h>
#include <Purifier/Dash/BaseDashComponent.h>
#include <Purifier/InputCharacter/HandSwayComponent.h>
#include "Purifier/Weapons/WeaponComponent.h"
#include "InputCharacterMovementComponent.h"


// Sets default values
AInputCharacter::AInputCharacter()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	Camera = CreateDefaultSubobject<UCameraComponent>("Camera");
	Camera->SetupAttachment(RootComponent);
	Camera->bUsePawnControlRotation = true;

	GetMesh()->SetupAttachment(Camera);

	HandSwayComponent = CreateDefaultSubobject<UHandSwayComponent>("HandSway");
	WallRunAttachCameraRollTimeline = CreateDefaultSubobject<UTimelineComponent>("CameraRollTimeline");
	WeaponComponent = CreateDefaultSubobject<UWeaponComponent>("Weapon");
	WeaponComponent->SetRaycastStartPoint(Camera);
	
	InputCharacterMovementComponent = Cast<UInputCharacterMovementComponent>(GetCharacterMovement());
}

// Called when the game starts or when spawned
void AInputCharacter::BeginPlay()
{
	Super::BeginPlay();

	InputCharacterMovementComponent->OnComponentWallRelativeRotationChanged.AddDynamic(this, &AInputCharacter::UpdateWallRunCameraRoll);
	
	DashComponent = FindComponentByClass<UBaseDashComponent>();

	FOnTimelineFloat WallRunAttachProgress;
	WallRunAttachProgress.BindUFunction(this, FName("UpdateWallRunAttachCameraRoll"));
	WallRunAttachCameraRollTimeline->AddInterpFloat(WallRunAttachCameraRollAlpha, WallRunAttachProgress);
	WallRunAttachCameraRollTimeline->SetPlayRate(1.0f / WallRunAttachDuration);
}

// Called every frame
void AInputCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green, FString::Printf(TEXT("Value: %d"), JumpCurrentCount));
	
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
		Input->BindAction(DashAction, ETriggerEvent::Triggered, this, &AInputCharacter::Dash);  //Dash on

		Input->BindAction(SelectWeapon1Action, ETriggerEvent::Triggered, this, &AInputCharacter::SelectWeapon1);
		Input->BindAction(SelectWeapon2Action, ETriggerEvent::Triggered, this, &AInputCharacter::SelectWeapon2);
		Input->BindAction(SelectWeapon3Action, ETriggerEvent::Triggered, this, &AInputCharacter::SelectWeapon3);
		Input->BindAction(SelectWeapon4Action, ETriggerEvent::Triggered, this, &AInputCharacter::SelectWeapon4);
		Input->BindAction(SelectWeapon5Action, ETriggerEvent::Triggered, this, &AInputCharacter::SelectWeapon5);

		Input->BindAction(PrimaryFireAction, ETriggerEvent::Triggered, this, &AInputCharacter::PrimaryFire);
		Input->BindAction(SecondaryFireAction, ETriggerEvent::Triggered, this, &AInputCharacter::SecondaryFire);
	}
}

void AInputCharacter::ResetJumpState()
{
	bPressedJump = false;
	bWasJumping = false;
	JumpKeyHoldTime = 0.0f;
	JumpForceTimeRemaining = 0.0f;

	if (InputCharacterMovementComponent && !InputCharacterMovementComponent->IsFalling() && !InputCharacterMovementComponent->IsDashing())
	{
		JumpCurrentCount = 0;
		JumpCurrentCountPreJump = 0;
	}
}

void AInputCharacter::Dash()
{
	DashComponent->StartDash();
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

FCollisionQueryParams AInputCharacter::GetIgnoreCharacterParams() const
{
	FCollisionQueryParams Params;

	TArray<AActor*> CharacterChildren;
	GetAllChildActors(CharacterChildren);
	Params.AddIgnoredActors(CharacterChildren);
	Params.AddIgnoredActor(this);

	return Params;
}

void AInputCharacter::CancelDash()
{
	if (DashComponent && !DashComponent->IsInstantDash())
	{
		GEngine->AddOnScreenDebugMessage(-1, 2, FColor::Yellow, "Dash Cancelling");
		DashComponent->CancelDash();
	}
}

UInputCharacterMovementComponent* AInputCharacter::GetInputCharacterMovement()
{
	return InputCharacterMovementComponent;
}

void AInputCharacter::OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode)
{
	Super::OnMovementModeChanged(PrevMovementMode, PreviousCustomMode);

	if (InputCharacterMovementComponent->IsWallRunning())
	{
		WallRunAttachCameraRollTimeline->PlayFromStart();
		if (PrevMovementMode == MOVE_Custom && PreviousCustomMode == CMOVE_Dash)
			CancelDash();
	}

	if (PrevMovementMode == MOVE_Custom && PreviousCustomMode == CMOVE_WallRun)
	{
		WallRunAttachCameraRollTimeline->Reverse();
	}

	if (PrevMovementMode == MOVE_Walking && InputCharacterMovementComponent->IsFalling())
	{
		float NormalizedVelocity = UKismetMathLibrary::NormalizeToRange(GetVelocity().Length(), 0.f, GetCharacterMovement()->MaxWalkSpeed);
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
	GEngine->AddOnScreenDebugMessage(-1, 2, FColor::Yellow, "Coyote Time Passed");
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



void AInputCharacter::OnDashStart()
{
	GetCharacterMovement()->BrakingFrictionFactor = 0.f;
	InputCharacterMovementComponent->StartDash();
}

void AInputCharacter::OnDashEnd()
{
	InputCharacterMovementComponent->StopDash();
	GetCharacterMovement()->BrakingFrictionFactor = 2.f;
}

FVector AInputCharacter::GetMoveDirection() const
{
	return FVector();
}

FVector2D AInputCharacter::GetInputDirection() const
{
	return MoveInputVector;
}

void AInputCharacter::UpdateWallRunCameraRoll(float NewAlpha)
{
	WallRunRelativeMaxCameraRoll = NewAlpha * WallRunMaxCameraRoll;
	
	if (!WallRunAttachCameraRollTimeline->IsPlaying())
	{
		FRotator OwnerControlRotation = Controller->GetControlRotation();
		OwnerControlRotation.Roll = WallRunRelativeMaxCameraRoll;
		Controller->SetControlRotation(OwnerControlRotation);
	}
}

void AInputCharacter::UpdateWallRunAttachCameraRoll(float RollAlpha)
{
	WallRunMaxAttachCameraRoll = RollAlpha * WallRunRelativeMaxCameraRoll;

	SetCameraRoll(WallRunMaxAttachCameraRoll);
}

void AInputCharacter::SetCameraRoll(float NewRoll)
{
	FRotator OwnerControlRotation = Controller->GetControlRotation();
	OwnerControlRotation.Roll = NewRoll;
	Controller->SetControlRotation(OwnerControlRotation);
}

