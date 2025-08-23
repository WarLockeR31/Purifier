#pragma once
#include "CoreMinimal.h"
#include "Navigation/PathFollowingComponent.h"
#include "VOFollowingComponent.generated.h"

class UVOManager;

USTRUCT(BlueprintType)
struct FVOParams
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VO") float TauHorizon		= 0.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VO") float MaxSpeed		= 400.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VO") float NeighborRange	= 600.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VO") float AgentRadius	= 34.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VO") int32 AngleSamples	= 12;
};

USTRUCT(BlueprintType)
struct FVOCone
{
	GENERATED_BODY()
	FVector2D Apex;
	FVector2D LeftRayApex;
	FVector2D LeftRayNormal;
	float     LeftRayOffset;
	FVector2D RightRayApex;
	FVector2D RightRayNormal;
	float     RightRayOffset;
	FVector2D TimeHorizonNormal;
	float     TimeHorizonOffset;
};

struct FVOOutsideSegment
{
	FVector2D P1;
	FVector2D P2;
	FVector2D OutsideNormal;
	float     OutsideOffset;
};

UCLASS(ClassGroup=AI, meta=(BlueprintSpawnableComponent))
class VOBASIC_API UVOFollowingComponent : public UPathFollowingComponent
{
	GENERATED_BODY()
// Fields & Properties
public:
	UPROPERTY(EditAnywhere, Category="VO|Debug")	bool		bDebugDraw = false;
	UPROPERTY(EditAnywhere, Category="VO|Debug")	FColor		DebugDrawColor = FColor::Red;
	UPROPERTY(EditAnywhere, Category="VO")			FVOParams	Params;
	
public:
	UVOFollowingComponent();
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category="VO") void SetMoveGoal(const FVector& InGoal) { bHasGoal = true; Goal = InGoal; }
	UFUNCTION(BlueprintCallable, Category="VO") void ClearMoveGoal()					{ bHasGoal = false; }
	

	FVector GetOwnerLocation()	const;
	FVector GetOwnerVelocity()	const;
	float   GetAgentRadius()	const	{ return Params.AgentRadius; }
	bool	HasVOGoal()			const	{ return bHasGoal; }
	FVector GetMoveGoal()		const	{ return Goal; }
	
private:
	bool	bHasGoal	= false;
	FVector Goal		= FVector::ZeroVector;
};
