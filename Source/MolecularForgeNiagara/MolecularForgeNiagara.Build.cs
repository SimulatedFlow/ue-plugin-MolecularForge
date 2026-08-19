// Copyright Simulated Flow. All Rights Reserved.

using UnrealBuildTool;

public class MolecularForgeNiagara : ModuleRules
{
	public MolecularForgeNiagara(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		CppStandard = CppStandardVersion.Cpp20;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"MolecularForgeRuntime",
			"Niagara"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"NiagaraCore",
			"Projects"
		});
	}
}
