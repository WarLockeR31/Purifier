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
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VO") float TauHorizon = 1.5f;      // seconds
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VO") float MaxSpeed = 400.f;       // cm/s
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VO") float MaxAccel = 1024.f;      // cm/s^2
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VO") float NeighborRange = 600.f;  // cm
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VO") float AgentRadius = 34.f;     // cm
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VO") int32 AngleSamples = 12;      // around desired dir
};

UCLASS(ClassGroup=AI, meta=(BlueprintSpawnableComponent))
class VOBASIC_API UVOFollowingComponent : public UPathFollowingComponent
{
    GENERATED_BODY()
public:
    UVOFollowingComponent();

    // === UActorComponent ===
    virtual void OnRegister() override;
    virtual void OnUnregister() override;
    virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // Desired target (simple — direct point, no navmesh)
    UFUNCTION(BlueprintCallable, Category="VO") void SetMoveGoal(const FVector& InGoal) { bHasGoal = true; Goal = InGoal; }
    UFUNCTION(BlueprintCallable, Category="VO") void ClearMoveGoal() { bHasGoal = false; }

    // Debug toggle (per component)
    UPROPERTY(EditAnywhere, Category="VO|Debug") bool bDebugDraw = false;

    UPROPERTY(EditAnywhere, Category="VO") FVOParams Params;

    // Accessors for subsystem
    FVector GetOwnerLocation() const;
    FVector GetOwnerVelocity() const;
    float   GetAgentRadius() const { return Params.AgentRadius; }

protected:
    // Core step: compute next velocity using basic VO sampling
    FVector ComputeVO(const FVector& CurVel, const FVector& DesiredVel, const TArray<struct FVONeighborView>& Neis) const;

    // Collision-time test for candidate velocity v against neighbor (classic discs)
    bool WillCollideWithinTau(const FVector2D& pRel, const FVector2D& vRel, float R, float Tau, float* OutTOI) const;

    void DrawVOCones(const FVector& P, const TArray<struct FVONeighborView>& Neis) const;

	APawn* GetControlledPawn() const;

private:
    bool bHasGoal = false;
    FVector Goal = FVector::ZeroVector;
};
