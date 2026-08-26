// Copyright 2026 Silvan Teufel All Rights Reserved.

using System.IO;
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
			// MassEntity ist in UE 5.8 ein Engine-Runtime-Modul (kein Plugin).
			"MassEntity",
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

		// MassCore ist erst mit dem Mass-Umbau aus MassEntity herausgeloest worden. Es ist
		// nicht in jedem 5.8-Stand vorhanden: die Fab-Pruefung vom 25.08.2026 brach mit
		// "Could not find definition for module 'MassCore'" ab, waehrend genau dasselbe
		// Archiv hier gegen 5.8 fehlerfrei durchbaute. Ein fester Eintrag laesst den Build
		// also auf der einen Haelfte der 5.8-Staende scheitern, ein Weglassen auf der
		// anderen — dort fehlen beim Linken FMassFragment, FMassTag, FMassEntityHandle und
		// FMassConstSharedFragment.
		//
		// Deshalb wird das Modul nur angezogen, wenn die Engine es tatsaechlich mitbringt.
		// Wo es fehlt, liegen dieselben Typen noch in MassEntity, das ohnehin oben steht.
		// Die zugehoerigen Kopfdateien unter "Mass/" werden im Quelltext mit
		// __has_include abgesichert.
		foreach (string Kandidat in new string[]
		{
			Path.Combine(EngineDirectory, "Source", "Runtime", "Mass", "MassCore"),
			Path.Combine(EngineDirectory, "Source", "Runtime", "MassCore"),
		})
		{
			if (File.Exists(Path.Combine(Kandidat, "MassCore.Build.cs")))
			{
				PublicDependencyModuleNames.Add("MassCore");
				break;
			}
		}
	}
}
