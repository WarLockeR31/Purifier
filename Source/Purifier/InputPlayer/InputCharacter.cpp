// Fill out your copyright notice in the Description page of Project Settings.


#include "Purifier/InputPlayer/InputCharacter.h"
#include <EnhancedInputSubsystems.h>
#include <EnhancedInputComponent.h>

#include "Camera/CameraComponent.h"
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

	DashTimeline = CreateDefaultSubobject<UTimelineComponent>(TEXT("DashTimeline"));
}

// Called when the game starts or when spawned
void AInputCharacter::BeginPlay()
{
	Super::BeginPlay();

	// Настройка функции для изменения положения персонажа при обновлении Timeline
	FOnTimelineFloat DashProgress;
	DashProgress.BindUFunction(this, FName("DashTimelineProgress"));
	DashTimeline->AddInterpFloat(DashCurve, DashProgress);

	// Настройка функции, вызываемой при завершении Timeline
	FOnTimelineEvent TimelineFinishedCallback;
	TimelineFinishedCallback.BindUFunction(this, FName("OnDashFinished"));
	DashTimeline->SetTimelineFinishedFunc(TimelineFinishedCallback);
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
		//Get forward direction
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		//Add movement input
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

	bIsDashing = true;
	GetCharacterMovement()->BrakingFrictionFactor = 0.f;

	//Get forward direction
	const FRotator Rotation = Controller->GetControlRotation();
	const FRotator YawRotation(0, Rotation.Yaw, 0);

	const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	FVector DashVector = (ForwardDirection * MoveInputVector.Y + RightDirection * MoveInputVector.X).GetSafeNormal();

	DashStartLocation = GetActorLocation();
	DashEndLocation = DashStartLocation + DashVector * DashDistance;

	// Запуск Timeline
	DashTimeline->PlayFromStart();
}

void AInputCharacter::DashTimelineProgress(float Value)
{
	if (DashCurve)
	{
		// Получаем скорость из кривой и перемещаем персонажа
		FVector NewLocation = FMath::Lerp(DashStartLocation, DashEndLocation, Value);
		SetActorLocation(NewLocation);
	}
}

void AInputCharacter::OnDashFinished()
{
	GetCharacterMovement()->BrakingFrictionFactor = 0.f;
	bIsDashing = false;
}