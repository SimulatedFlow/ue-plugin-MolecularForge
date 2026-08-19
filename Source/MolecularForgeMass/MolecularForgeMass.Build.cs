// Copyright Simulated Flow. All Rights Reserved.

using UnrealBuildTool;

public class MolecularForgeMass : ModuleRules
{
	public MolecularForgeMass(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		CppStandard = CppStandardVersion.Cpp20;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"MolecularForgeRuntime",
			// Der Mesoskala-Renderer benutzt die Kugelkomponente fuer die nahe Stufe.
			"MolecularForgeRender",
			// MassEntity/MassCore sind in UE 5.8 Engine-Runtime-Module (kein Plugin).
			"MassEntity",
			"MassCore",
			// Public, weil UMolMesoscaleTrait von UMassEntityTraitBase ableitet.
			"MassSpawner"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			// Aus dem MassGameplay-Plugin.
			"MassCommon",
			"MassMovement",
			"Projects"
		});
	}
}
