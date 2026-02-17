// Fill out your copyright notice in the Description page of Project Settings.

#include "Settings/VOSettings.h"
#include "UObject/UObjectIterator.h"
#include "Templates/SubclassOf.h"

#include "Components/VOFollowingComponent.h" // TODO: Replace?

const FVOParams& UVOSettings::GetPresetOrDefault(FName Name) const
{
	if (const FVOParams* Found = Presets.Find(Name))
	{
		return *Found;
	}
	if (const FVOParams* Def = Presets.Find(DefaultPreset))
	{
		return *Def;
	}
	
	static FVOParams Fallback; //LOL
	return Fallback;
}

FName UVOSettings::ResolvePresetForPawnClass(UClass* PawnClass) const
{
	if (!PawnClass) return DefaultPreset;

	FName Best = NAME_None;
	int32 BestDepth = -1;

	for (const auto& Pair : ClassToPreset)
	{
		if (!Pair.Key.IsValid()) continue;
		UClass* KeyClass = Pair.Key.LoadSynchronous();
		if (!KeyClass) continue;
		if (PawnClass->IsChildOf(KeyClass))
		{
			// Take class with max depth in hierarchy
			int32 Depth = 0;
			for (UClass* C = PawnClass; C && C != KeyClass; C = C->GetSuperClass()) { ++Depth; }
			if (BestDepth < 0 || Depth < BestDepth) 
			{
				BestDepth = Depth;
				Best = Pair.Value;
			}
		}
	}

	if (Best != NAME_None) return Best;
	return DefaultPreset;
}