// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Purifier/Dash/Dashable.h"
#include "InputActionValue.h"
#include "Purifier/Weapons/WeaponComponent.h"
#include "InputCharacter.generated.h"

UCLASS()
class PURIFIER_API AInputCharacter : public ACharacter, public IDashable
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, meta = (AllowPrivateAccess = "true"))
	class UCameraComponent* Camera;


	UPROPERTY(VisibleAnywhere, Category = "EnhancedInput")
	FVector2D MoveInputVector;

	UPROPERTY(VisibleAnywhere, Category = "Dash")
	class UBaseDashComponent* DashComponent;

	UPROPERTY(VisibleAnywhere, Category = "HandsSway")
	class UHandSwayComponent* HandSwayComponent;
	UPROPERTY(VisibleAnywhere, Category = "Movement")
	class UInputCharacterMovementComponent* InputCharacterMovementComponent;
	UPROPERTY(VisibleAnywhere, Category = "Weapon")
	UWeaponComponent* WeaponComponent;

protected:

#pragma region Input
	UPROPERTY(EditAnywhere, Category = "EnhancedInput")
	class UInputMappingContext* InputMapping;

	UPROPERTY(EditAnywhere, Category = "EnhancedInput")
	class UInputAction* MoveAction;

	UPROPERTY(EditAnywhere, Category = "EnhancedInput")
	class UInputAction* JumpAction;

	UPROPERTY(EditAnywhere, Category = "EnhancedInput")
	class UInputAction* LookAction;

	UPROPERTY(EditAnywhere, Category = "EnhancedInput")
	class UInputAction* DashAction;

	UPROPERTY(EditAnywhere, Category = "EnhancedInput")
	class UInputAction* PrimaryFireAction;

	UPROPERTY(EditAnywhere, Category = "EnhancedInput")
	class UInputAction* SecondaryFireAction;

	UPROPERTY(EditAnywhere, Category = "EnhancedInput")
	class UInputAction* SelectWeapon1Action;

	UPROPERTY(EditAnywhere, Category = "EnhancedInput")
	class UInputAction* SelectWeapon2Action;

	UPROPERTY(EditAnywhere, Category = "EnhancedInput")
	class UInputAction* SelectWeapon3Action;

	UPROPERTY(EditAnywhere, Category = "EnhancedInput")
	class UInputAction* SelectWeapon4Action;

	UPROPERTY(EditAnywhere, Category = "EnhancedInput")
	class UInputAction* SelectWeapon5Action;
#pragma endregion Input


	UPROPERTY(EditAnywhere, Category = "Movement")
	float CoyoteTime;

	UPROPERTY(VisibleAnywhere, Category = "Movement")
	FTimerHandle CoyoteTimerHandle;


#pragma region WallRun
	UPROPERTY(EditAnywhere, Category = "WallRun")
	float WallRunMaxCameraRoll;

	UPROPERTY(VisibleAnywhere, Category = "WallRun")
	float WallRunRelativeMaxCameraRoll;

	UPROPERTY(VisibleAnywhere, Category = "WallRun")
	float WallRunMaxAttachCameraRoll;

	UPROPERTY(VisibleAnywhere, Category = "WallRun")
	class UTimelineComponent* WallRunAttachCameraRollTimeline;

	UPROPERTY(EditDefaultsOnly) UCurveFloat* WallRunAttachCameraRollAlpha;

	UPROPERTY(EditAnywhere, Category = "WallRun")
	float WallRunAttachDuration;
#pragma endregion

public:
	// Sets default values for this character's properties
	AInputCharacter();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	virtual void ResetJumpState() override;

protected:
	void Move(const FInputActionValue& InputValue);
	void Look(const FInputActionValue& InputValue);
	void Jump();
	void Dash();
	void SelectWeapon1() { WeaponComponent->SelectWeapon(1); };
	void SelectWeapon2() { WeaponComponent->SelectWeapon(2); };
	void SelectWeapon3() { WeaponComponent->SelectWeapon(3); };
	void SelectWeapon4() { WeaponComponent->SelectWeapon(4); };
	void SelectWeapon5() { WeaponComponent->SelectWeapon(5); };
	void PrimaryFire() { WeaponComponent->FirePrimary(); };
	void SecondaryFire() { WeaponComponent->FireSecondary(); };
	

	
	


	virtual void OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode) override; 
	
	UFUNCTION()
	void UpdateWallRunCameraRoll(float NewAlpha);

	UFUNCTION()
	void UpdateWallRunAttachCameraRoll(float RollAlpha);
		
	virtual void Landed(const FHitResult& Hit) override;

	virtual void CheckJumpInput(float DeltaTime) override;
	//virtual bool CanJumpInternal_Implementation() const override;
	virtual void OnJumped_Implementation() override;

	void OnCoyoteTimePassed();

public:
	UInputCharacterMovementComponent* GetInputCharacterMovement();

	virtual void OnDashStart() override;
	virtual void OnDashEnd() override;

	virtual FVector GetMoveDirection() const override;  // Метод для получения направления рывка.
	virtual FVector2D GetInputDirection() const override;

	FCollisionQueryParams GetIgnoreCharacterParams() const;

	void CancelDash();

	// Helpers	
private:
	void SetCameraRoll(float NewRoll);
};
