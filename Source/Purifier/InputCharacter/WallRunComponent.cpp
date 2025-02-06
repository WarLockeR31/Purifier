#include "WallRunComponent.h"
#include "Components/TimelineComponent.h"
#include "InputCharacter.h"
#include <Kismet/KismetSystemLibrary.h>
#include "InputCharacterMovementComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/CapsuleComponent.h"

// Sets default values for this component's properties
UWallRunComponent::UWallRunComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	WallRunTimeline = CreateDefaultSubobject<UTimelineComponent>(TEXT("WallRunTimeline"));
	WallRunAttachTimeline = CreateDefaultSubobject<UTimelineComponent>(TEXT("WallRunAttachTimeline"));
	WallRunDetachTimeline = CreateDefaultSubobject<UTimelineComponent>(TEXT("WallRunDetachTimeline"));
	WallRunSlowDownTimeline = CreateDefaultSubobject<UTimelineComponent>(TEXT("WallRunSlowDownTimeline"));


	//WallRunTimeline->AddTickPrerequisiteComponent(WallRunAttachTimeline);
	WallRunAttachTimeline->AddTickPrerequisiteComponent(WallRunTimeline);
	WallRunSlowDownTimeline->AddTickPrerequisiteComponent(WallRunTimeline);
}


// Called when the game starts
void UWallRunComponent::BeginPlay()
{
	Super::BeginPlay();

	// Настраиваем триггерную зону
	
	WallRunCollider->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	WallRunCollider->SetCollisionObjectType(ECollisionChannel::ECC_WorldDynamic);
	WallRunCollider->SetCollisionResponseToAllChannels(ECR_Ignore);
	WallRunCollider->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Overlap); // Реагирует только на стены

	WallRunCollider->OnComponentBeginOverlap.AddDynamic(this, &UWallRunComponent::OnWallTriggerBeginOverlap);

	if (AInputCharacter* OwnerCast = Cast<AInputCharacter>(GetOwner()))
	{
		OwnerInputCharacter = OwnerCast;
		InputCharacterMovementComponent = OwnerInputCharacter->GetInputCharacterMovement();
		OwnerContoller = OwnerInputCharacter->GetController();
		WallRunCollider->AttachToComponent(OwnerInputCharacter->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
		BaseAirControl = InputCharacterMovementComponent->AirControl;
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Incorrect owner of the HandSwayComponent"));
	}

	FOnTimelineFloat WallRunProgress;
	WallRunProgress.BindUFunction(this, FName("UpdateWallRun"));
	WallRunTimeline->AddInterpFloat(WallRunCurve, WallRunProgress);
	WallRunTimeline->SetLooping(true);

	FOnTimelineFloat WallRunAttachProgress;
	WallRunAttachProgress.BindUFunction(this, FName("UpdateWallRunAttach"));
	WallRunAttachTimeline->AddInterpFloat(WallRunAttachRollCurve, WallRunAttachProgress);
	//WallRunAttachTimeline->SetTimelineLength(WallRunAttachDuration);
	WallRunAttachTimeline->SetPlayRate(1.0f / WallRunAttachDuration);
	FOnTimelineEvent WallRunAttachTimelineFinished;
	/*WallRunAttachTimelineFinished.BindUFunction(this, FName("OnWallRunAttachEnd"));
	WallRunAttachTimeline->SetTimelineFinishedFunc(WallRunAttachTimelineFinished);*/
	WallRunAttachTimelineFinished.BindUFunction(WallRunSlowDownTimeline, FName("PlayFromStart"));
	WallRunAttachTimeline->SetTimelineFinishedFunc(WallRunAttachTimelineFinished);

	FOnTimelineFloat WallRunSlowDownProgress;
	WallRunSlowDownProgress.BindUFunction(this, FName("UpdateWallRunSlowDown"));
	WallRunSlowDownTimeline->AddInterpFloat(WallRunSlowDownCurve, WallRunSlowDownProgress);
	//WallRunSlowDownTimeline->SetTimelineLength(WallRunSlowDownDuration);
	WallRunSlowDownTimeline->SetPlayRate(1.0f / WallRunSlowDownDuration);

	FOnTimelineFloat WallRunDetachProgress;
	WallRunDetachProgress.BindUFunction(this, FName("UpdateWallRunDetach"));
	WallRunDetachTimeline->AddInterpFloat(WallRunDetachRollCurve, WallRunDetachProgress);
	WallRunDetachTimeline->SetPlayRate(1.0f / WallRunAttachDuration);

	AlignSpeedCoefficient = GetSpeedCoefficient(
		WallRunCollider->GetScaledCapsuleRadius() - OwnerInputCharacter->GetCapsuleComponent()->GetScaledCapsuleRadius(),
		WallRunAttachDuration,
		500
	);
}


// Called every frame
void UWallRunComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Magenta, FString::Printf(TEXT("Vel: %.2f"), InputCharacterMovementComponent->Velocity.Length()));
}

void UWallRunComponent::OnWallTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, 
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (bIsWallRunning)
		return;

	TArray<FHitResult> OutHits;
	float DistanceTrace = 200.f;
	FVector StartTrace = WallRunCollider->GetComponentLocation();
	FVector EndTrace = (OwnerInputCharacter->GetVelocity().GetSafeNormal() * DistanceTrace + StartTrace);
	TArray<AActor*> actorsToIgnore;
	actorsToIgnore.Add(OwnerInputCharacter);

	bool bIsHitted = UKismetSystemLibrary::CapsuleTraceMulti(
		GetWorld(),
		StartTrace,
		EndTrace,
		WallRunCollider->GetScaledCapsuleRadius(),
		WallRunCollider->GetScaledCapsuleHalfHeight(),
		UEngineTypes::ConvertToTraceType(ECC_WorldStatic),
		false,
		actorsToIgnore,
		EDrawDebugTrace::ForDuration,
		OutHits,
		true
	);

	FHitResult* Hit = OutHits.FindByPredicate([OtherActor](const FHitResult& Hit)
	{
		return Hit.GetActor() == OtherActor;
	});

	if (!SurfaceIsWallRunnable(Hit->ImpactNormal))
		return;

	bIsWallRunLeft = OwnerInputCharacter->GetActorRightVector().Dot(Hit->ImpactNormal) > 0;
	WallRunDirection = Hit->ImpactNormal.Cross(FVector(0.f, 0.f, bIsWallRunLeft ? 1.f : -1.f)).GetSafeNormal();

	if (WallRunDirection.Dot(OwnerInputCharacter->GetVelocity()) < 0)
	{
		WallRunDirection *= -1.f;
		bIsWallRunLeft = !bIsWallRunLeft;
	}

	if (AreRequiredKeysDown())
	{
		StartWallRun();
	}
}

//_____________________________________________________________________________________________________
void UWallRunComponent::StartWallRun()
{
	GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Green, TEXT("StartedWallRun"));

	bIsWallRunning = true;
	InputCharacterMovementComponent->StopMovementImmediately();
	InputCharacterMovementComponent->AirControl = 1.f;
	InputCharacterMovementComponent->GravityScale = 0.f;
	InputCharacterMovementComponent->SetPlaneConstraintNormal(FVector(0.f, 0.f, 1.f));
	WallRunTimeline->Play();

	WallRunAttachTimeline->PlayFromStart();
}

void UWallRunComponent::UpdateWallRun()
{
	if (!AreRequiredKeysDown())
	{
		EndWallRun();
		return;
	}

	FHitResult Hit;
	TArray<AActor*> actorsToIgnore;
	actorsToIgnore.Add(OwnerInputCharacter);
	FVector EndLocation = OwnerInputCharacter->GetActorLocation() + WallRunDirection.Cross(FVector(0.f, 0.f, bIsWallRunLeft ? 1.f : -1.f)) * 90.f;

	if (!UKismetSystemLibrary::LineTraceSingle(GetWorld(), OwnerInputCharacter->GetActorLocation(), EndLocation, UEngineTypes::ConvertToTraceType(ECC_Visibility), false, actorsToIgnore, EDrawDebugTrace::ForDuration, Hit, true))
	{
		EndWallRun();
		return;
	}

	//if ()

	WallRunDirection = Hit.ImpactNormal.Cross(FVector(0.f, 0.f, bIsWallRunLeft ? 1.f : -1.f));

	UpdateWallRunCameraRoll();

	if (WallRunAttachTimeline->IsPlaying())
		return;

	WallRunCurrentSpeed *= WallRunSpeedAlpha;
	InputCharacterMovementComponent->Velocity = WallRunDirection.GetSafeNormal() * WallRunCurrentSpeed;
	//OwnerInputCharacter->LaunchCharacter(FVector(WallRunDirection.X, WallRunDirection.Y, 0.f) * 2000.f, true, true);
	//InputCharacterMovementComponent->Velocity = AlignDirection * WallRunAttachAlignSpeedCurve->GetFloatValue(CurTLTime) * AlignSpeedCoefficient;
}

void UWallRunComponent::EndWallRun()
{
	WallRunDetachTimeline->PlayFromStart();
	WallRunTimeline->Stop();
	InputCharacterMovementComponent->SetPlaneConstraintNormal(FVector(0.f, 0.f, 0.f));
	InputCharacterMovementComponent->GravityScale = 1.f;
	InputCharacterMovementComponent->AirControl = BaseAirControl;
	bIsWallRunning = false;
	GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Green, TEXT("EndedWallRun"));
}

bool UWallRunComponent::SurfaceIsWallRunnable(const FVector SurfaceNormal) const
{
	GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, FString::Printf(TEXT("Normal.z: %.2f"), SurfaceNormal.Z));
	if (SurfaceNormal.Z < -0.05f)
	{
		return false;
	}

	FVector SurfaceNormalProjection = FVector(SurfaceNormal.X, SurfaceNormal.Y, 0).GetSafeNormal();
	float angle = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(SurfaceNormalProjection, SurfaceNormal)));
	GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, FString::Printf(TEXT("Angle: %.2f"), angle));
	return angle < InputCharacterMovementComponent->GetWalkableFloorAngle();
}

bool UWallRunComponent::AreRequiredKeysDown() const
{
	FVector2D MoveInputVector = OwnerInputCharacter->GetInputDirection();
	FVector WolrdMoveInputVector = OwnerInputCharacter->GetActorForwardVector() * MoveInputVector.Y + OwnerInputCharacter->GetActorRightVector() * MoveInputVector.X;

	return WolrdMoveInputVector.Dot(WallRunDirection) > 0;
}


void UWallRunComponent::UpdateWallRunAttach(float Roll)
{
	GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, FString::Printf(TEXT("Angle: %.2f"), WallRunMaxCameraRoll * Roll));
	WallRunMaxAttachCameraRoll = WallRunMaxCameraRoll * (bIsWallRunLeft ? Roll : -Roll);

	float CurTLTime = WallRunAttachTimeline->GetPlaybackPosition();

	//Align section
	FVector AlignDirection = WallRunDirection.Cross(FVector(0.f, 0.f, bIsWallRunLeft ? 1.f : -1.f)).GetSafeNormal();
	InputCharacterMovementComponent->Velocity = AlignDirection * WallRunAttachAlignSpeedCurve->GetFloatValue(CurTLTime) * AlignSpeedCoefficient;
	GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, InputCharacterMovementComponent->Velocity.ToString());
	/*OwnerInputCharacter->LaunchCharacter(
		AlignDirection * WallRunAttachAlignSpeedCurve->GetFloatValue(CurTLTime) * AlignSpeedCoefficient, 
		false, 
		false
	);*/

	FVector WallRunStartVelocity = (AlignDirection * WallRunAttachAlignSpeedCurve->GetFloatValue(CurTLTime) * AlignSpeedCoefficient) + WallRunDirection.GetSafeNormal() * 2000.f * Roll;
	WallRunCurrentSpeed = WallRunStartVelocity.Length();
	InputCharacterMovementComponent->Velocity = WallRunStartVelocity;
	//OwnerInputCharacter->LaunchCharacter(FVector(WallRunDirection.X, WallRunDirection.Y, 0.f) * 2000.f, true, true);
}

void UWallRunComponent::UpdateWallRunSlowDown(float Speed)
{
	GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, FString::Printf(TEXT("Angle: %.2f"), Speed));

	WallRunSpeedAlpha = Speed;
}

void UWallRunComponent::UpdateWallRunDetach(float Roll)
{
	WallRunMaxAttachCameraRoll = WallRunMaxCameraRoll * (bIsWallRunLeft ? Roll : -Roll);
	UpdateWallRunCameraRoll();
}

float UWallRunComponent::CalculateCurrentCameraRoll() const
{
	float angleAlpha = FVector::DotProduct(OwnerInputCharacter->GetActorForwardVector(), WallRunDirection.GetSafeNormal());
	return WallRunMaxAttachCameraRoll * angleAlpha;
}

void UWallRunComponent::UpdateWallRunCameraRoll()
{
	FRotator OwnerControlRotation = OwnerContoller->GetControlRotation();
	OwnerControlRotation.Roll = CalculateCurrentCameraRoll();
	OwnerContoller->SetControlRotation(OwnerControlRotation);
}

float UWallRunComponent::GetSpeedCoefficient(float Distance, float Duration, int StepsNum) const
{
	float MinTime, MaxTime;
	WallRunAttachAlignSpeedCurve->GetTimeRange(MinTime, MaxTime);

	float step = (MaxTime - MinTime) / StepsNum;
	float ApproximateCurveS = 0.f;
	for (float i = MinTime; i < MaxTime; i += step)
	{
		ApproximateCurveS += WallRunAttachAlignSpeedCurve->GetFloatValue(i + step) * step;
	}

	return Distance / ApproximateCurveS / Duration;
}