// Copyright 2026 Simulated Flow All Rights Reserved.

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
			"MolecularForgeRuntime",
			// Traegt das Cartoon-Band. Anders als Atome und Bindungen laesst es sich
			// nicht instanzieren — es ist eine einzige durchgehende Flaeche.
			"ProceduralMeshComponent"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"RenderCore",
			"RHI",
			"Projects"
		});
	}
}
