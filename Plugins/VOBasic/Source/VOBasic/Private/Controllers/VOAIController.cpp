#include "Controllers/VOAIController.h"

#include "Settings/VOConfigProvider.h"
#include "Components/VOFollowingComponent.h"
#include "Settings/VOSettings.h"

AVOAIController::AVOAIController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UVOFollowingComponent>(TEXT("PathFollowingComponent")))
{
	VOFollowingComponent = Cast<UVOFollowingComponent>(GetPathFollowingComponent());
}

void AVOAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	UVOFollowingComponent* VO = GetVOFollowing();
	if (!VO) return;

	// Try to get a profile from Pawn
	bool bGotProfile = false;

	if (InPawn && InPawn->GetClass()->ImplementsInterface(UVOConfigProvider::StaticClass()))
	{
		FName ProfileName = NAME_None;
		if (IVOConfigProvider::Execute_GetVOProfileName(InPawn, ProfileName))
		{
			VO->SetVOProfile(ProfileName);
			bGotProfile = true;
		}

		FVOParams Overrides;
		if (IVOConfigProvider::Execute_GetVOOverrides(InPawn, Overrides)) {
			VO->SetVOOverrides(Overrides, true);
		}
	}

	// If we didn't get a profile from Pawn, try to get it from settings
	if (!bGotProfile)
	{
		const UVOSettings* Settings = UVOSettings::Get();
		const FName Resolved = Settings->ResolvePresetForPawnClass(InPawn ? InPawn->GetClass() : nullptr);
		VO->SetVOProfile(Resolved);
	}
}