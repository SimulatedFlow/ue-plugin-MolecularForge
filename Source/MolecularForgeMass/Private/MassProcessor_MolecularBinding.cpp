// Copyright 2026 Silvan Teufel All Rights Reserved.

#include "MassProcessor_MolecularBinding.h"
#include "MolMesoscaleFragments.h"
#include "MolMesoscaleMath.h"

#include "Mass/EntityFragments.h"
#include "MassCommonTypes.h"
#include "MassExecutionContext.h"

UMassProcessor_MolecularBinding::UMassProcessor_MolecularBinding()
	// Beide Abfragen an den Prozessor binden — siehe Anmerkung im Bewegungsprozessor.
	: SnapshotQuery(*this)
	, BindingQuery(*this)
{
	bAutoRegisterWithProcessingPhases = true;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::AllNetModes);

	// Nach der Bewegung: gebunden wird auf den Positionen dieses Bildes und nicht auf
	// denen des letzten.
	ProcessingPhase = EMassProcessingPhase::PostPhysics;
}

void UMassProcessor_MolecularBinding::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	SnapshotQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	SnapshotQuery.AddRequirement<FMolMoleculeFragment>(EMassFragmentAccess::ReadOnly);
	SnapshotQuery.AddConstSharedRequirement<FMolMesoscaleParameters>(EMassFragmentPresence::All);
	SnapshotQuery.AddTagRequirement<FMolMoleculeTag>(EMassFragmentPresence::All);

	BindingQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	BindingQuery.AddRequirement<FMolMoleculeFragment>(EMassFragmentAccess::ReadWrite);
	BindingQuery.AddConstSharedRequirement<FMolMesoscaleParameters>(EMassFragmentPresence::All);
	BindingQuery.AddTagRequirement<FMolMoleculeTag>(EMassFragmentPresence::All);
}

FIntVector UMassProcessor_MolecularBinding::ComputeCell(const FVector3f& Position, float InverseCellSize) const
{
	return FIntVector(
		FMath::FloorToInt(Position.X * InverseCellSize),
		FMath::FloorToInt(Position.Y * InverseCellSize),
		FMath::FloorToInt(Position.Z * InverseCellSize));
}

void UMassProcessor_MolecularBinding::BuildGrid(float CellSize)
{
	CellHeadIndex.Reset();
	NextInCell.SetNumUninitialized(SnapshotPositions.Num());

	if (SnapshotPositions.IsEmpty() || CellSize <= 0.f)
	{
		return;
	}

	CellHeadIndex.Reserve(SnapshotPositions.Num());
	const float InverseCellSize = 1.f / CellSize;

	// Verkettete Liste je Zelle: der Kopf zeigt auf den zuletzt eingetragenen Eintrag,
	// jeder Eintrag auf seinen Vorgaenger. Ein Uniform-Grid ueber eine feste Huelle waere
	// schneller, braeuchte aber die Ausdehnung vorab — und die aendert sich hier mit
	// jedem Bild, weil die Molekuele wandern.
	for (int32 Index = 0; Index < SnapshotPositions.Num(); ++Index)
	{
		const FIntVector Cell = ComputeCell(SnapshotPositions[Index], InverseCellSize);

		int32& Head = CellHeadIndex.FindOrAdd(Cell, INDEX_NONE);
		NextInCell[Index] = Head;
		Head = Index;
	}
}

void UMassProcessor_MolecularBinding::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	const float DeltaSeconds = Context.GetDeltaTimeSeconds();
	if (DeltaSeconds <= 0.f)
	{
		return;
	}

	// ---- Stufe 1: Abbild einsammeln ----

	SnapshotPositions.Reset();
	SnapshotRadii.Reset();
	SnapshotHandles.Reset();
	CachedCellSize = 0.f;

	bool bBindingEnabled = false;

	SnapshotQuery.ForEachEntityChunk(Context, [this, &bBindingEnabled](FMassExecutionContext& ChunkContext)
	{
		const FMolMesoscaleParameters& Parameters =
			ChunkContext.GetConstSharedFragment<FMolMesoscaleParameters>();

		if (!Parameters.bEnableBinding)
		{
			return;
		}

		bBindingEnabled = true;
		CachedCellSize = FMath::Max(CachedCellSize, Parameters.BindingGridCellSize);

		const TConstArrayView<FTransformFragment> Transforms =
			ChunkContext.GetFragmentView<FTransformFragment>();
		const TConstArrayView<FMolMoleculeFragment> Molecules =
			ChunkContext.GetFragmentView<FMolMoleculeFragment>();

		const int32 NumEntities = ChunkContext.GetNumEntities();
		SnapshotPositions.Reserve(SnapshotPositions.Num() + NumEntities);

		for (int32 Index = 0; Index < NumEntities; ++Index)
		{
			SnapshotPositions.Add(FVector3f(Transforms[Index].GetTransform().GetLocation()));
			SnapshotRadii.Add(Molecules[Index].ContactRadius * Parameters.UnitsPerAngstrom);
			SnapshotHandles.Add(ChunkContext.GetEntity(Index));
		}
	});

	if (!bBindingEnabled || SnapshotPositions.Num() < 2)
	{
		LastBoundCount = 0;
		return;
	}

	if (CachedCellSize <= 0.f)
	{
		// Notfallwert: der groesste vorkommende Kontaktdurchmesser. Kleinere Zellen
		// wuerden Partner uebersehen, die knapp ausserhalb der Nachbarzellen liegen.
		float MaxRadius = 0.f;
		for (float Radius : SnapshotRadii)
		{
			MaxRadius = FMath::Max(MaxRadius, Radius);
		}
		CachedCellSize = FMath::Max(2.f * MaxRadius, 1.f);
	}

	BuildGrid(CachedCellSize);

	// ---- Stufe 2: entscheiden ----

	std::atomic<int32> BoundCount{ 0 };
	const float InverseCellSize = 1.f / CachedCellSize;

	BindingQuery.ParallelForEachEntityChunk(Context,
		[this, DeltaSeconds, InverseCellSize, &BoundCount](FMassExecutionContext& ChunkContext)
	{
		const FMolMesoscaleParameters& Parameters =
			ChunkContext.GetConstSharedFragment<FMolMesoscaleParameters>();

		if (!Parameters.bEnableBinding)
		{
			return;
		}

		const TConstArrayView<FTransformFragment> Transforms =
			ChunkContext.GetFragmentView<FTransformFragment>();
		const TArrayView<FMolMoleculeFragment> Molecules =
			ChunkContext.GetMutableFragmentView<FMolMoleculeFragment>();

		for (int32 Index = 0; Index < ChunkContext.GetNumEntities(); ++Index)
		{
			FMolMoleculeFragment& Molecule = Molecules[Index];

			// Bereits gebunden: nur pruefen, ob es sich loest.
			if (Molecule.BoundTo.IsSet())
			{
				if (MolecularForge::ShouldUnbind(Parameters.UnbindProbabilityPerSecond,
					DeltaSeconds, Molecule.RandomStream.GetFraction()))
				{
					Molecule.BoundTo.Reset();
				}
				else
				{
					BoundCount.fetch_add(1, std::memory_order_relaxed);
				}
				continue;
			}

			const FVector3f Position(Transforms[Index].GetTransform().GetLocation());
			const FMassEntityHandle Self = ChunkContext.GetEntity(Index);
			const float OwnRadius = Molecule.ContactRadius * Parameters.UnitsPerAngstrom;

			const FIntVector Cell = ComputeCell(Position, InverseCellSize);

			int32 NearestCandidate = INDEX_NONE;
			float NearestDistanceSquared = TNumericLimits<float>::Max();

			for (int32 dz = -1; dz <= 1; ++dz)
			{
				for (int32 dy = -1; dy <= 1; ++dy)
				{
					for (int32 dx = -1; dx <= 1; ++dx)
					{
						const FIntVector Neighbour(Cell.X + dx, Cell.Y + dy, Cell.Z + dz);
						const int32* Head = CellHeadIndex.Find(Neighbour);
						if (!Head)
						{
							continue;
						}

						for (int32 Other = *Head; Other != INDEX_NONE; Other = NextInCell[Other])
						{
							if (SnapshotHandles[Other] == Self)
							{
								continue;
							}

							const float DistanceSquared =
								FVector3f::DistSquared(Position, SnapshotPositions[Other]);

							if (DistanceSquared < NearestDistanceSquared)
							{
								NearestDistanceSquared = DistanceSquared;
								NearestCandidate = Other;
							}
						}
					}
				}
			}

			if (NearestCandidate == INDEX_NONE)
			{
				continue;
			}

			const float ContactSum = OwnRadius + SnapshotRadii[NearestCandidate];

			if (MolecularForge::ShouldBind(NearestDistanceSquared, ContactSum,
				Parameters.BindProbabilityPerSecond, DeltaSeconds, Molecule.RandomStream.GetFraction()))
			{
				Molecule.BoundTo = SnapshotHandles[NearestCandidate];
				BoundCount.fetch_add(1, std::memory_order_relaxed);
			}
		}
	});

	LastBoundCount = BoundCount.load(std::memory_order_relaxed);
}
