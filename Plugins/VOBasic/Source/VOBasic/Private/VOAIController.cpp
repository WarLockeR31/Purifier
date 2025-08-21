#include "VOAIController.h"
#include "VOFollowingComponent.h"

AVOAIController::AVOAIController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UVOFollowingComponent>(TEXT("PathFollowingComponent")))
{
	
}