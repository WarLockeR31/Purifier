#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "VOWorldSubsystem.generated.h"

class UVOFollowingComponent;

USTRUCT()
struct FVONeighborView
{
	GENERATED_BODY()
	FVector Pos = FVector::ZeroVector;   // world XY
	FVector Vel = FVector::ZeroVector;   // world XY
	float Radius = 34.f;                 // cm
};

UCLASS()
class VOBASIC_API UVOWorldSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override { return true; }

	void Register(UVOFollowingComponent* Comp);
	void Unregister(UVOFollowingComponent* Comp);

	// Very simple neighbor query 
	void QueryNeighbors(const UVOFollowingComponent* Querier, const FVector& P, float Range, TArray<FVONeighborView>& Out) const;

private:
	UPROPERTY(Transient)
	TSet<TWeakObjectPtr<UVOFollowingComponent>> Agents;
};