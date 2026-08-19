// Copyright Simulated Flow. All Rights Reserved.

#include "MolecularStructureActor.h"
#include "MolecularAtomsComponent.h"
#include "MolecularStructure.h"
#include "MolStructureIO.h"
#include "Misc/Paths.h"

AMolecularStructureActor::AMolecularStructureActor()
{
	PrimaryActorTick.bCanEverTick = false;

	AtomsComponent = CreateDefaultSubobject<UMolecularAtomsComponent>(TEXT("AtomsComponent"));
	RootComponent = AtomsComponent;
}

void AMolecularStructureActor::BeginPlay()
{
	Super::BeginPlay();

	if (bLoadOnBeginPlay && !StructureFilePath.IsEmpty())
	{
		LoadNow();
	}
}

void AMolecularStructureActor::LoadNow()
{
	if (StructureFilePath.IsEmpty())
	{
		UE_LOG(LogMolecularForge, Warning, TEXT("%s: Kein Dateipfad gesetzt."), *GetName());
		return;
	}

	FString ResolvedPath = StructureFilePath;
	if (FPaths::IsRelative(ResolvedPath))
	{
		ResolvedPath = FPaths::Combine(FPaths::ProjectDir(), ResolvedPath);
	}

	LoadedStructure = NewObject<UMolecularStructure>(this);

	const FMolParseResult Result = MolecularForge::ParseStructureFile(ResolvedPath, LoadOptions, *LoadedStructure);
	if (!Result.bSuccess)
	{
		UE_LOG(LogMolecularForge, Warning, TEXT("%s: %s"), *GetName(), *Result.Error);
		LoadedStructure = nullptr;
		AtomsComponent->SetStructure(nullptr);
		return;
	}

	// Die Komponente muss dieselbe Umrechnung benutzen wie das Laden, sonst passen
	// Radien und Positionen nicht zusammen.
	AtomsComponent->UnitsPerAngstrom = LoadOptions.UnitsPerAngstrom;
	AtomsComponent->SetStructure(LoadedStructure);
}
