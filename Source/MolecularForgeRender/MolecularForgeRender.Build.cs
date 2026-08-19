// Copyright Silvan Teufel. All Rights Reserved.

using UnrealBuildTool;

public class MolecularForgeRender : ModuleRules
{
	public MolecularForgeRender(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		CppStandard = CppStandardVersion.Cpp20;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"MolecularForgeRuntime"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"RenderCore",
			"RHI",
			"Projects"
		});
	}
}
