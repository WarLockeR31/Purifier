using UnrealBuildTool;

public class VOBasicEditor : ModuleRules
{
    public VOBasicEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
				"CoreUObject",
				"Engine",
				"InputCore",
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "VOBasic",
				"UnrealEd",
				"EditorSubsystem",
				"AIModule",
            }
        );
    }
}