

#pragma once



#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InputCharacterMovementComponent.h"
#include "InputCharacter.h"

#include "WallRunComponent.generated.h"

UENUM(BlueprintType)
enum class EWallRunSide : uint8 {
	Left = 0 UMETA(DisplayName = "LEFT"),
	Right = 1  UMETA(DisplayName = "RIGHT"),
};

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

	UPROPERTY(VisibleAnywhere, Category = "WallRun")
	FVector WallRunDirection;

	UPROPERTY(VisibleAnywhere, Category = "WallRun")
	bool bWallRunning;

	UPROPERTY(VisibleAnywhere, Category = "WallRun")
	EWallRunSide WallRunSide;

	UPROPERTY(VisibleAnywhere, Category = "WallRun")
	float BaseAirControl;

	UPROPERTY(VisibleAnywhere, Category = "WallRun")
	class UTimelineComponent* WallRunTimeline;

	UPROPERTY(EditAnywhere, Category = "WallRun")
	UCurveFloat* WallRunCurve;
	
	UPROPERTY(EditAnywhere, Category = "WallRun")
	UPrimitiveComponent* WallRunCollider;

public:	
	// Sets default values for this component's properties
	UWallRunComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

		
	//WallRun trigger
	UFUNCTION()
	void OnCollisionHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);
	//WallRun helper
	bool SurfaceIsWallRunnable(const FVector SurfaceNormal) const;
	//WallRun helper
	bool AreRequiredKeysDown() const;

	//WallRun
	void StartWallRun();
	UFUNCTION()
	void UpdateWallRun();
	void EndWallRun();

};
