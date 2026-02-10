// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class VOBasic : ModuleRules
{
	public VOBasic(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(
			new string[] {
				
			}
			);
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
				
			}
			);
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"AIModule",
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"NavigationSystem",
				"DeveloperSettings",
				"Navmesh",
			}
			);
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				
			}
			);
		
		PublicDefinitions.Add("SAVE_VO_PATHS=1");
		PublicDefinitions.Add("DEBUG_ON=1");
		PublicDefinitions.Add("AO_SFM_REPULSION=1");
	}
}
