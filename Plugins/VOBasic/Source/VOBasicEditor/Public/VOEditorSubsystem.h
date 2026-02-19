// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "TickableEditorObject.h"
#include "VOEditorSubsystem.generated.h"

/**
 * 
 */
UCLASS()
class VOBASICEDITOR_API UVOEditorSubsystem : public UEditorSubsystem, public FTickableEditorObject
{
	GENERATED_BODY()

public:
	virtual void Tick(float DeltaTime) override;
	virtual ETickableTickType GetTickableTickType() const override
	{
		return ETickableTickType::Always;
	}
	virtual TStatId GetStatId() const override
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(VOEditorSubsystem, STATGROUP_Tickables);
	}

private:
	void DrawForActor(UWorld* World, APawn*Pawn);
	void DrawDebugCapsulePrism(
 		const UWorld* InWorld,
 		FVector const& BaseStart,   
 		FVector const& BaseEnd,     
 		float Radius,
 		float Height,
 		int32 Segments,
 		FColor const& Color,
 		bool bPersistentLines = false,
    	float LifeTime = -1.f,
    	uint8 DepthPriority = 0,
    	float Thickness = 0.f);
};
