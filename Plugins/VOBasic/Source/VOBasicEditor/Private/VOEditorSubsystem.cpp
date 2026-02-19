// Fill out your copyright notice in the Description page of Project Settings.


#include "VOEditorSubsystem.h"

#include "AIController.h"
#include "EngineUtils.h"
#include "Selection.h"
#include "Settings/VOSettings.h"

void UVOEditorSubsystem::Tick(float DeltaTime)
{
	if (!GEditor)
		return;

	for (const FWorldContext& Context : GEditor->GetWorldContexts())
	{
		UWorld* World = Context.World();
		if (!World)
			continue;

		if (World->WorldType == EWorldType::EditorPreview)
		{
			for (TActorIterator<APawn> It(World); It; ++It)
			{
				DrawForActor(World, *It);
			}
		}
		else if (World->WorldType == EWorldType::Editor || World->WorldType == EWorldType::PIE)
		{
			TArray<UObject*> SelectedObjects;
			GEditor->GetSelectedActors()->GetSelectedObjects(APawn::StaticClass(), SelectedObjects);
			for (UObject* Obj : SelectedObjects)
			{
				if (APawn* Pawn = Cast<APawn>(Obj))
				{
					DrawForActor(World, Pawn);
				}
			}
		}
	}
}

void UVOEditorSubsystem::DrawForActor(UWorld* World, APawn*Pawn)
{
	const UVOSettings* Settings = UVOSettings::Get();
	if (!Settings)
		return;

	FVector Loc = FVector::ZeroVector;
	FVOParams Params;
	bool bHasParams = false;

	if (AAIController* AIC = Cast<AAIController>(Pawn->GetController()))
	{
		if (auto* VO = AIC->FindComponentByClass<UVOFollowingComponent>())
		{
			Params = VO->GetEffectiveParams();
			Loc = VO->GetCrowdAgentLocation();
			bHasParams = true;
		}
	}

	if (!bHasParams)
	{
		FName PresetName = Settings->ResolvePresetForPawnClass(Pawn->GetClass());

		if (const FVOParams* Preset = Settings->FindPreset(PresetName))
		{
			Params = *Preset;
			bHasParams = true;
		}
	}

	if (bHasParams)
	{
		if (Loc == FVector::ZeroVector)
		{
			Loc = Pawn->GetActorLocation();
			float HalfHeight = Pawn->GetSimpleCollisionHalfHeight();
			Loc.Z -= HalfHeight;
		}
				
		FColor Color = FColor::Cyan;

		DrawDebugCylinder(World, Loc, Loc + FVector(0, 0, Params.AgentHeight), Params.AgentRadius, 16, Color, false, -1.f, 0, 1.f);
	}
}