// Copyright Simulated Flow. All Rights Reserved.

#include "MolecularStructureActor.h"
#include "MolecularAtomsComponent.h"
#include "MolecularBondsComponent.h"
#include "MolecularCartoonComponent.h"
#include "MolecularSurfaceComponent.h"
#include "MolecularStructure.h"
#include "MolStructureIO.h"
#include "Misc/Paths.h"

AMolecularStructureActor::AMolecularStructureActor()
{
	PrimaryActorTick.bCanEverTick = false;

	AtomsComponent = CreateDefaultSubobject<UMolecularAtomsComponent>(TEXT("AtomsComponent"));
	RootComponent = AtomsComponent;

	BondsComponent = CreateDefaultSubobject<UMolecularBondsComponent>(TEXT("BondsComponent"));
	BondsComponent->SetupAttachment(RootComponent);

	CartoonComponent = CreateDefaultSubobject<UMolecularCartoonComponent>(TEXT("CartoonComponent"));
	CartoonComponent->SetupAttachment(RootComponent);

	SurfaceComponent = CreateDefaultSubobject<UMolecularSurfaceComponent>(TEXT("SurfaceComponent"));
	SurfaceComponent->SetupAttachment(RootComponent);
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
		BondsComponent->SetStructure(nullptr);
		CartoonComponent->SetStructure(nullptr);
		SurfaceComponent->SetStructure(nullptr);
		return;
	}

	ApplyRepresentation();
}

void AMolecularStructureActor::ApplyRepresentation()
{
	// Alle drei Komponenten muessen dieselbe Umrechnung benutzen wie das Laden, sonst
	// liegen Band, Staebe und Kugeln nicht uebereinander.
	const float Scale = LoadOptions.UnitsPerAngstrom;

	const bool bShowBonds =
		Representation == EMolRepresentation::BallAndStick
		|| Representation == EMolRepresentation::Backbone;

	const bool bShowCartoon = Representation == EMolRepresentation::Cartoon;
	const bool bShowSurface = Representation == EMolRepresentation::Surface;

	AtomsComponent->UnitsPerAngstrom = Scale;
	AtomsComponent->Representation = Representation;
	AtomsComponent->ColorScheme = ColorScheme;
	AtomsComponent->SetStructure(LoadedStructure);

	BondsComponent->SetVisibility(bShowBonds);
	if (bShowBonds)
	{
		BondsComponent->UnitsPerAngstrom = Scale;
		BondsComponent->ColorScheme = ColorScheme;
		BondsComponent->SetStructure(LoadedStructure);
	}
	else
	{
		// Nicht nur ausblenden, sondern auch leeren: eine unsichtbare Komponente mit
		// hunderttausend Instanzen kostet weiter Speicher.
		BondsComponent->SetStructure(nullptr);
	}

	CartoonComponent->SetVisibility(bShowCartoon);
	if (bShowCartoon)
	{
		CartoonComponent->UnitsPerAngstrom = Scale;
		CartoonComponent->ColorScheme = ColorScheme;
		CartoonComponent->SetStructure(LoadedStructure);
	}
	else
	{
		CartoonComponent->SetStructure(nullptr);
	}

	SurfaceComponent->SetVisibility(bShowSurface);
	if (bShowSurface)
	{
		SurfaceComponent->UnitsPerAngstrom = Scale;
		SurfaceComponent->ColorScheme = ColorScheme;
		SurfaceComponent->SetStructure(LoadedStructure);
	}
	else
	{
		SurfaceComponent->SetStructure(nullptr);
	}
}

#if WITH_EDITOR
void AMolecularStructureActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	static const TSet<FName> ReapplyTriggers = {
		GET_MEMBER_NAME_CHECKED(AMolecularStructureActor, Representation),
		GET_MEMBER_NAME_CHECKED(AMolecularStructureActor, ColorScheme)
	};

	if (ReapplyTriggers.Contains(PropertyChangedEvent.GetPropertyName()) && LoadedStructure)
	{
		ApplyRepresentation();
	}
}
#endif
