#pragma once
#include "CoreMinimal.h"
#include "AIController.h"
#include "Navigation/PathFollowingComponent.h"
#include "VOFollowingComponent.generated.h"

class UVOWorldSubsystem;

USTRUCT(BlueprintType)
struct FVOParams
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VO") float TauHorizon = 0.5f;      // seconds
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VO") float MaxSpeed = 400.f;       // cm/s
    //UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VO") float MaxAccel = 1024.f;      // cm/s^2
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VO") float NeighborRange = 600.f;  // cm
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VO") float AgentRadius = 34.f;     // cm
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VO") int32 AngleSamples = 12;      // around desired dir
};

/// Velocity obstacle truncated cone (3 constraints)
USTRUCT(BlueprintType)
struct FVOCone
{
	GENERATED_BODY()
	FVector2D Apex;
	
	FVector2D LeftRayApex;
	FVector2D LeftRayNormal;		// (a, b)
	float LeftRayOffset;			// c
		
	FVector2D RightRayApex;	
	FVector2D RightRayNormal;		// (a, b)
	float RightRayOffset;			// c

	FVector2D TimeHorizonNormal;	// (a, b)
	float TimeHorizonOffset;		// c
};

struct FVOOutsideSegment
{
	FVector2D	P1;
	FVector2D	P2;
	FVector2D	OutsideNormal;
	float		OutsideOffset;
};

UCLASS(ClassGroup=AI, meta=(BlueprintSpawnableComponent))
class VOBASIC_API UVOFollowingComponent : public UPathFollowingComponent
{
    GENERATED_BODY()
public:
    UVOFollowingComponent();

    // UActorComponent
    virtual void OnRegister() override;
    virtual void OnUnregister() override;
    virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // Desired target
    UFUNCTION(BlueprintCallable, Category="VO") void SetMoveGoal(const FVector& InGoal) { bHasGoal = true; Goal = InGoal; }
    UFUNCTION(BlueprintCallable, Category="VO") void ClearMoveGoal() { bHasGoal = false; }

    // Debug toggle (per component)
    UPROPERTY(EditAnywhere, Category="VO|Debug") bool bDebugDraw = false;
	UPROPERTY(EditAnywhere, Category="VO|Debug") FColor DebugDrawColor = FColor::Red;

	// Parameters
    UPROPERTY(EditAnywhere, Category="VO") FVOParams Params;

    // Accessors for subsystem
    FVector GetOwnerLocation() const;
    FVector GetOwnerVelocity() const;
    float   GetAgentRadius() const { return Params.AgentRadius; }

protected:
    // Compute next velocity
    FVector ComputeVelocity(const FVector& CurVel, const FVector& DesiredVel, const TArray<struct FVONeighborView>& Neis) const;
	
    /// @param R Minkowski sum radius
    /// @param C Position of obstacle relative to agent
    /// @param Vel Current velocity of obstacle
    FVOCone ComputeVOCone(const float R, const FVector2D& C, const FVector2D& Vel) const;

	bool TryFindIntersections(
		const float A1, const float B1, const float C1,
		const float A2, const float B2, const float C2,
		FVector2D* OutPoint) const;
	
    // Collision-time test for candidate velocity v against neighbor (classic discs)
    bool WillCollideWithinTau(const FVector2D& RelativePosition, const FVector2D& RelativeVelocity, float Radius, float TimeHorizon, float* OutTOI) const;
	
	

	void DrawVOCones(TArray<FVOCone>& Cone) const;
	void DrawCombinedVO(const TArray<TArray<FVOOutsideSegment>>& OutsideSegmentsByRays) const;

	APawn* GetControlledPawn() const;

private:
    bool bHasGoal = false;
    FVector Goal = FVector::ZeroVector;
};
