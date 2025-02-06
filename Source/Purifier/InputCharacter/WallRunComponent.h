

#pragma once



#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InputCharacterMovementComponent.h"
#include "InputCharacter.h"

#include "WallRunComponent.generated.h"


//UENUM(BlueprintType)
//enum class EWallRunEndReason : uint8 {
//	FallOffWall = 0 UMETA(DisplayName = "FallOffWall"),
//	JumpedOffWall = 1  UMETA(DisplayName = "JumpedOffWall"),
//};

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class PURIFIER_API UWallRunComponent : public UActorComponent
{
	GENERATED_BODY()

	UPROPERTY()
	AInputCharacter* OwnerInputCharacter;

	UPROPERTY()
	UInputCharacterMovementComponent* InputCharacterMovementComponent;

	UPROPERTY()
	AController* OwnerContoller;

	UPROPERTY(VisibleAnywhere, Category = "WallRun")
	FVector WallRunDirection;

	UPROPERTY(VisibleAnywhere, Category = "WallRun")
	bool bIsWallRunning;

	UPROPERTY(VisibleAnywhere, Category = "WallRun")
	float BaseAirControl;

	UPROPERTY(VisibleAnywhere, Category = "WallRun")
	bool bIsWallRunLeft;


	UPROPERTY(EditAnywhere, Category = "WallRun")
	UCapsuleComponent* WallRunCollider;

	UPROPERTY(VisibleAnywhere, Category = "WallRun")
	class UTimelineComponent* WallRunTimeline;

	UPROPERTY(EditAnywhere, Category = "WallRun")
	UCurveFloat* WallRunCurve;

	UPROPERTY(VisibleAnywhere, Category = "WallRun")
	class UTimelineComponent* WallRunAttachTimeline;

	UPROPERTY(VisibleAnywhere, Category = "WallRun")
	class UTimelineComponent* WallRunSlowDownTimeline;

	UPROPERTY(VisibleAnywhere, Category = "WallRun")
	class UTimelineComponent* WallRunDetachTimeline;

	UPROPERTY(EditAnywhere, Category = "WallRun")
	UCurveFloat* WallRunAttachRollCurve;

	UPROPERTY(EditAnywhere, Category = "WallRun")
	UCurveFloat* WallRunAttachAlignSpeedCurve;

	UPROPERTY(EditAnywhere, Category = "WallRun")
	UCurveFloat* WallRunDetachRollCurve;

	UPROPERTY(EditAnywhere, Category = "WallRun")
	UCurveFloat* WallRunSlowDownCurve;

	UPROPERTY(VisibleAnywhere, Category = "WallRun")
	float WallRunSpeedAlpha = 1.f;

	UPROPERTY(EditAnywhere, Category = "WallRun")
	float WallRunAttachDuration;

	UPROPERTY(EditAnywhere, Category = "WallRun")
	float WallRunMaxCameraRoll;

	UPROPERTY(VisibleAnywhere, Category = "WallRun")
	float WallRunMaxAttachCameraRoll;

	UPROPERTY(VisibleAnywhere, Category = "WallRun")
	float AlignSpeedCoefficient;

	UPROPERTY(VisibleAnywhere, Category = "WallRun")
	float WallRunCurrentSpeed;

	UPROPERTY(EditAnywhere, Category = "WallRun")
	float WallRunSlowDownDuration;

public:	
	// Sets default values for this component's properties
	UWallRunComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

		
	UFUNCTION()
	void OnWallTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	//WallRun helper
	bool SurfaceIsWallRunnable(const FVector SurfaceNormal) const;
	//WallRun helper
	bool AreRequiredKeysDown() const;

	//WallRun
	void StartWallRun();
	UFUNCTION()
	void UpdateWallRun();
	void EndWallRun();

	UFUNCTION()
	void UpdateWallRunAttach(float Roll);

	UFUNCTION()
	void UpdateWallRunSlowDown(float Speed);

	UFUNCTION()
	void UpdateWallRunDetach(float Roll);

	float CalculateCurrentCameraRoll() const;

	void UpdateWallRunCameraRoll();

	float GetSpeedCoefficient(float Distance, float Duration, int StepsNum) const;
};
