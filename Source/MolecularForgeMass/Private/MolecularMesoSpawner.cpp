// Copyright 2026 Silvan Teufel All Rights Reserved.

#include "MolecularMesoSpawner.h"
#include "MolecularForgeTypes.h"

// Siehe MolecularForgeMass.Build.cs: MassCore ist nicht in jedem 5.8-Stand abgespalten.
#if __has_include("Mass/EntityFragments.h")
#include "Mass/EntityFragments.h"
#else
#include "MassCommonFragments.h"
#endif
#include "MassEntityManager.h"
#include "MassEntitySubsystem.h"
#include "MassMovementFragments.h"
#include "Engine/World.h"

AMolecularMesoSpawner::AMolecularMesoSpawner()
{
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	// Eine Art als Ausgangspunkt, damit der Actor nach dem Platzieren etwas tut.
	Species.Add(FMolMesoSpecies());
}

void AMolecularMesoSpawner::BeginPlay()
{
	Super::BeginPlay();

	if (bSpawnOnBeginPlay)
	{
		SpawnPopulation();
	}
}

void AMolecularMesoSpawner::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearPopulation();
	Super::EndPlay(EndPlayReason);
}

void AMolecularMesoSpawner::ClearPopulation()
{
	if (SpawnedEntities.IsEmpty())
	{
		return;
	}

	UWorld* World = GetWorld();
	UMassEntitySubsystem* Subsystem = World ? World->GetSubsystem<UMassEntitySubsystem>() : nullptr;

	if (Subsystem)
	{
		FMassEntityManager& EntityManager = Subsystem->GetMutableEntityManager();
		for (const FMassEntityHandle& Entity : SpawnedEntities)
		{
			if (EntityManager.IsEntityValid(Entity))
			{
				EntityManager.DestroyEntity(Entity);
			}
		}
	}

	SpawnedEntities.Reset();
}

int32 AMolecularMesoSpawner::SpawnPopulation()
{
	ClearPopulation();

	UWorld* World = GetWorld();
	UMassEntitySubsystem* Subsystem = World ? World->GetSubsystem<UMassEntitySubsystem>() : nullptr;

	if (!Subsystem)
	{
		UE_LOG(LogMolecularForge, Warning,
			TEXT("Mesoskala-Spawner: kein Mass-Entity-Subsystem, es wird nichts gesetzt."));
		return 0;
	}

	int32 TotalCount = 0;
	for (const FMolMesoSpecies& Entry : Species)
	{
		TotalCount += FMath::Max(0, Entry.Count);
	}

	if (TotalCount <= 0)
	{
		return 0;
	}

	FMassEntityManager& EntityManager = Subsystem->GetMutableEntityManager();

	// Die gemeinsamen Parameter einmal anlegen. Sie liegen als Const-Shared-Fragment
	// genau einmal im Speicher, egal wie viele Molekuele darauf zeigen — und die
	// Prozessoren verlangen sie ausdruecklich, sonst greift ihre Abfrage nicht.
	FMolMesoscaleParameters Validated = Parameters;
	Validated.BoundsExtent = Validated.BoundsExtent.ComponentMax(FVector(1.0, 1.0, 1.0));
	Validated.BackboneDistance = FMath::Max(Validated.BackboneDistance, Validated.FullDetailDistance);
	Validated.BlobDistance = FMath::Max(Validated.BlobDistance, Validated.BackboneDistance);

	if (Validated.BindingGridCellSize <= 0.f)
	{
		float MaxRadius = 0.f;
		for (const FMolMesoSpecies& Entry : Species)
		{
			MaxRadius = FMath::Max(MaxRadius, Entry.ContactRadiusAngstrom);
		}
		Validated.BindingGridCellSize =
			FMath::Max(2.f * MaxRadius * Validated.UnitsPerAngstrom, 1.f);
	}

	const FConstSharedStruct SharedParameters =
		EntityManager.GetOrCreateConstSharedFragment(Validated);

	FMassArchetypeSharedFragmentValues SharedValues;
	SharedValues.Add(SharedParameters);

	TArray<const UScriptStruct*> Composition;
	Composition.Add(FTransformFragment::StaticStruct());
	Composition.Add(FMassVelocityFragment::StaticStruct());
	Composition.Add(FMolMoleculeFragment::StaticStruct());
	Composition.Add(FMolMoleculeTag::StaticStruct());

	const FMassArchetypeHandle Archetype = EntityManager.CreateArchetype(Composition);

	TArray<FMassEntityHandle> Created;
	EntityManager.BatchCreateEntities(Archetype, SharedValues, TotalCount, Created);

	// Verteilung im erlaubten Raum. Mit festem Startwert ist der Aufbau wiederholbar —
	// ohne das saehe jede Aufnahme anders aus, und ein Video liesse sich nicht neu drehen.
	FRandomStream Placement(RandomSeed);

	const FBox3f Bounds = Validated.GetBounds();
	const FVector3f Extent = Bounds.Max - Bounds.Min;

	int32 Index = 0;
	int32 SpeciesIndex = 0;

	for (const FMolMesoSpecies& Entry : Species)
	{
		for (int32 i = 0; i < Entry.Count && Index < Created.Num(); ++i, ++Index)
		{
			const FMassEntityHandle Entity = Created[Index];

			const FVector3f Position = Bounds.Min + FVector3f(
				Placement.GetFraction() * Extent.X,
				Placement.GetFraction() * Extent.Y,
				Placement.GetFraction() * Extent.Z);

			FTransformFragment& Transform = EntityManager.GetFragmentDataChecked<FTransformFragment>(Entity);
			Transform.GetMutableTransform().SetLocation(FVector(Position));

			FMolMoleculeFragment& Molecule =
				EntityManager.GetFragmentDataChecked<FMolMoleculeFragment>(Entity);
			Molecule.SpeciesIndex = SpeciesIndex;
			Molecule.ContactRadius = Entry.ContactRadiusAngstrom;

			// Jede Entity bekommt ihre eigene Zufallsquelle. Ohne das waere die
			// Diffusion entweder ein Engpass oder von der Reihenfolge abhaengig, in der
			// die Threads die Chunks abarbeiten.
			Molecule.RandomStream.Initialize(RandomSeed + Index * 7919);
		}
		++SpeciesIndex;
	}

	SpawnedEntities = MoveTemp(Created);

	UE_LOG(LogMolecularForge, Log,
		TEXT("Mesoskala: %d Molekuele in %d Arten gesetzt."), SpawnedEntities.Num(), Species.Num());

	return SpawnedEntities.Num();
}
