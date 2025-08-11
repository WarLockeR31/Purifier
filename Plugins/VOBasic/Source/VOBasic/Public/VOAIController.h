#pragma once
#include "CoreMinimal.h"
#include "AIController.h"
#include "VOFollowingComponent.h"
#include "VOAIController.generated.h"

class UVOFollowingComponent;

UCLASS()
class VOBASIC_API AVOAIController : public AAIController
{
	GENERATED_BODY()
public:
	AVOAIController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UFUNCTION(BlueprintCallable, Category="VO")
	UVOFollowingComponent* GetVOFollowing() const { return FindComponentByClass<UVOFollowingComponent>(); }
};