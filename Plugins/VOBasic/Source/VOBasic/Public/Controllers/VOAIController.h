#pragma once
#include "CoreMinimal.h"
#include "AIController.h"
#include "Components/VOFollowingComponent.h"
#include "VOAIController.generated.h"

class UVOFollowingComponent;

UCLASS()
class VOBASIC_API AVOAIController : public AAIController
{
	GENERATED_BODY()
public:
	AVOAIController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UFUNCTION(BlueprintCallable, Category="VO")
	UVOFollowingComponent* GetVOFollowing() const { return VOFollowingComponent.Get(); }

protected:
	virtual void OnPossess(APawn* InPawn) override;

	//UPROPERTY(VisibleDefaultsOnly, Category = AI)
	TObjectPtr<UVOFollowingComponent> VOFollowingComponent;
};