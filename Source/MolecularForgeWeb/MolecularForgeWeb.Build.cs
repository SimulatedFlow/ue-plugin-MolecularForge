// Copyright 2026 Simulated Flow All Rights Reserved.

using UnrealBuildTool;

public class MolecularForgeWeb : ModuleRules
{
	public MolecularForgeWeb(ReadOnlyTargetRules Target) : base(Target)
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
			"HTTP",
			"Json",
			"Projects"
		});
	}
}
