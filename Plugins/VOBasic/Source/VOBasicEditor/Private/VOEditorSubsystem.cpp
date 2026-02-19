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

void UVOEditorSubsystem::DrawForActor(UWorld* World, APawn* Pawn)
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
		const FVector TopOffset(0, 0, Params.AgentHeight);

		switch (Params.Shape)
		{
		case EVOAgentShape::Circle:
			DrawDebugCylinder(World, Loc, Loc + TopOffset, Params.AgentRadius, 16, Color, false, -1.f, 0, 1.f);
			break;
		case EVOAgentShape::Capsule:
			FVector2D S1, S2;
			UVOFollowingComponent::GetAgentCapsuleSegment(Pawn, Params, S1, S2);
			DrawDebugCapsulePrism(
				World,
				FVector(S1.X, S1.Y, Loc.Z),
				FVector(S2.X, S2.Y, Loc.Z),
				Params.AgentRadius,
				Params.AgentHeight,
				16,
				FColor::Cyan
            );
			break;
		default:
			UE_LOG(LogTemp, Warning, TEXT("Unsupported shape: %d"), (int32)Params.Shape);
		}
	}
}

void UVOEditorSubsystem::DrawDebugCapsulePrism(
	const UWorld* InWorld,
	FVector const& BaseStart,
	FVector const& BaseEnd,
	float Radius,
	float Height,
	int32 Segments,
	FColor const& Color,
	bool bPersistentLines,
	float LifeTime,
	uint8 DepthPriority,
	float Thickness)
{
	if (!InWorld) return;

	const FVector TopOffset(0.f, 0.f, Height);

	DrawDebugCylinder(InWorld, BaseStart, BaseStart + TopOffset, Radius, Segments, Color,
	   bPersistentLines, LifeTime, DepthPriority, Thickness);
	DrawDebugCylinder(InWorld, BaseEnd, BaseEnd + TopOffset, Radius, Segments, Color,
	   bPersistentLines, LifeTime, DepthPriority, Thickness);
	
	FVector Dir = (BaseEnd - BaseStart).GetSafeNormal();
	if (Dir.IsNearlyZero()) return;

	FVector SideOffset = FVector(-Dir.Y, Dir.X, 0.f) * Radius;

	DrawDebugLine(InWorld, BaseStart + SideOffset, BaseEnd + SideOffset, Color,
	   bPersistentLines, LifeTime, DepthPriority, Thickness);
	DrawDebugLine(InWorld, BaseStart - SideOffset, BaseEnd - SideOffset, Color,
	   bPersistentLines, LifeTime, DepthPriority, Thickness);
	DrawDebugLine(InWorld, BaseStart + TopOffset + SideOffset, BaseEnd + TopOffset +
	   SideOffset, Color, bPersistentLines, LifeTime, DepthPriority, Thickness);
	DrawDebugLine(InWorld, BaseStart + TopOffset - SideOffset, BaseEnd + TopOffset -
	   SideOffset, Color, bPersistentLines, LifeTime, DepthPriority, Thickness);
}

