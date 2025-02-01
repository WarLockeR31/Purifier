


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
	//WallRunCollider = (UPrimitiveComponent*)OwnerInputCharacter->GetCapsuleComponent();
	
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
	//WallRunCollider->OnComponentHit.AddDynamic(this, &UWallRunComponent::OnCollisionHit);  //WallRun on

	if (AInputCharacter* OwnerCast = Cast<AInputCharacter>(GetOwner()))
	{
		OwnerInputCharacter = OwnerCast;
		InputCharacterMovementComponent = OwnerInputCharacter->GetInputCharacterMovement();
		//WallRunCollider->SetupAttachment(OwnerInputCharacter->GetRootComponent());
		WallRunCollider->AttachToComponent(OwnerInputCharacter->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Incorrect owner of the HandSwayComponent"));
	}

	FOnTimelineFloat WallRunProgress;
	WallRunProgress.BindUFunction(this, FName("UpdateWallRun"));
	WallRunTimeline->AddInterpFloat(WallRunCurve, WallRunProgress);
	WallRunTimeline->SetLooping(true);

	BaseAirControl = 2.f;
	//BaseAirControl = InputCharacterMovementComponent->AirControl;
}


// Called every frame
void UWallRunComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

void UWallRunComponent::OnWallTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, 
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{

	//FHitResult Hit;
	
	//UKismetSystemLibrary::LineTraceSingle(GetWorld(), OwnerInputCharacter->GetActorLocation(), SweepResult.ImpactPoint, UEngineTypes::ConvertToTraceType(ECC_Visibility), false, actorsToIgnore, EDrawDebugTrace::Persistent, Hit, true);

	//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("Overlap"));
	//FString BoolText = bFromSweep ? TEXT("true") : TEXT("false");
	//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, FString::Printf(TEXT("bFromSweep: %s"), *BoolText));

	if (bWallRunning)
	{
		return;
	}

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

	if (bWallRunning || !SurfaceIsWallRunnable(Hit->ImpactNormal))
	{
		return;
	}
	//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("1"));
	if (OwnerInputCharacter->GetActorRightVector().Dot(Hit->ImpactNormal) > 0)
	{
		WallRunSide = EWallRunSide::Left;
		WallRunDirection = Hit->ImpactNormal.Cross(FVector(0.f, 0.f, 1.f));
	}
	else
	{
		WallRunSide = EWallRunSide::Right;
		WallRunDirection = Hit->ImpactNormal.Cross(FVector(0.f, 0.f, -1.f));
	}

	if (AreRequiredKeysDown())
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("2"));
		StartWallRun();
	}
}

//void UWallRunComponent::OnCollisionHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
//	UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
//{
//	if (bWallRunning || !SurfaceIsWallRunnable(Hit.ImpactNormal))
//	{
//		return;
//	}
//
//	if (OwnerInputCharacter->GetActorRightVector().Dot(Hit.ImpactNormal) > 0)
//	{
//		WallRunSide = EWallRunSide::Left;
//		WallRunDirection = Hit.ImpactNormal.Cross(FVector(0.f, 0.f, 1.f));
//	}
//	else
//	{
//		WallRunSide = EWallRunSide::Right;
//		WallRunDirection = Hit.ImpactNormal.Cross(FVector(0.f, 0.f, -1.f));
//	}
//
//	if (AreRequiredKeysDown())
//	{
//		StartWallRun();
//	}
//}

#pragma region WallRun
//_____________________________________________________________________________________________________
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

	if (MoveInputVector.Y < 0.1f)
	{
		return false;
	}

	return MoveInputVector.X > 0.1f && (WallRunSide == EWallRunSide::Right) ||
		MoveInputVector.X < -0.1f && (WallRunSide == EWallRunSide::Left);
}

void UWallRunComponent::StartWallRun()
{
	GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Green, TEXT("StartedWallRun"));

	bWallRunning = true;
	InputCharacterMovementComponent->StopMovementImmediately();
	InputCharacterMovementComponent->AirControl = 1.f;
	InputCharacterMovementComponent->GravityScale = 0.f;
	InputCharacterMovementComponent->SetPlaneConstraintNormal(FVector(0.f, 0.f, 1.f));
	WallRunTimeline->Play();
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
	FVector EndLocation = OwnerInputCharacter->GetActorLocation() + WallRunDirection.Cross(FVector(0.f, 0.f, WallRunSide == EWallRunSide::Left ? 1.f : -1.f)) * 90.f;

	if (!UKismetSystemLibrary::LineTraceSingle(GetWorld(), OwnerInputCharacter->GetActorLocation(), EndLocation, UEngineTypes::ConvertToTraceType(ECC_Visibility), false, actorsToIgnore, EDrawDebugTrace::ForOneFrame, Hit, true))
	{
		EndWallRun();
		return;
	}

	if (OwnerInputCharacter->GetActorRightVector().Dot(Hit.ImpactNormal) > 0)
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

	OwnerInputCharacter->LaunchCharacter(FVector(WallRunDirection.X, WallRunDirection.Y, 0.f) * 2000.f, true, true);
}

void UWallRunComponent::EndWallRun()
{
	WallRunTimeline->Stop();
	InputCharacterMovementComponent->SetPlaneConstraintNormal(FVector(0.f, 0.f, 0.f));
	InputCharacterMovementComponent->GravityScale = 1.f;
	InputCharacterMovementComponent->AirControl = BaseAirControl;
	bWallRunning = false;
	GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Green, TEXT("EndedWallRun"));
}


//_____________________________________________________________________________________________________
#pragma endregion WallRun