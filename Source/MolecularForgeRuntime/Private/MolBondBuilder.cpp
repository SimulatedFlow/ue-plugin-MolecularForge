// Copyright 2026 Silvan Teufel All Rights Reserved.

#include "MolBondBuilder.h"
#include "MolecularStructure.h"
#include "MolElementTable.h"
#include "Async/ParallelFor.h"

namespace
{
	/** Zuschlag auf die Summe der kovalenten Radien. Entspricht der Praxis in Jmol und VMD. */
	constexpr float GBondTolerance = 0.45f;

	/** Unterhalb dieses Abstands liegen zwei Atome faktisch aufeinander — das ist ein Artefakt, keine Bindung. */
	constexpr float GMinBondDistance = 0.4f;

	/**
	 * Obergrenze fuer die Zellenzahl des Grids. Ueberschreitet eine Struktur sie, wird die
	 * Zellkante vergroessert. Das kostet etwas Rechenzeit pro Zelle, verhindert aber, dass
	 * eine ausgedehnte, duenn besetzte Struktur hunderte Megabyte fuer leere Zellen belegt.
	 */
	constexpr int64 GMaxGridCells = 8 * 1024 * 1024;

	constexpr int32 GBondParallelThreshold = 4096;
}

namespace MolecularForge
{
	void BuildBondsByDistance(UMolecularStructure& Structure)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(MolecularForge_BuildBonds);

		Structure.Bonds.Reset();

		const int32 NumAtoms = Structure.GetNumAtoms();
		if (NumAtoms < 2)
		{
			return;
		}

		const TArray<FVector3f>& Positions = Structure.AtomPositions;
		const TArray<uint8>& Elements = Structure.AtomElements;

		// ---- Radien vorziehen und groesste moegliche Bindungslaenge bestimmen ----
		// Die Tabellenzugriffe hier einmal zu erledigen spart im heissen Paarungslauf
		// je Nachbarpaar einen indirekten Zugriff.

		TArray<float> CovalentRadii;
		CovalentRadii.SetNumUninitialized(NumAtoms);

		float MaxRadius = 0.f;
		for (int32 i = 0; i < NumAtoms; ++i)
		{
			const float R = GetElement(Elements[i]).CovalentRadius;
			CovalentRadii[i] = R;
			MaxRadius = FMath::Max(MaxRadius, R);
		}

		const float MaxBondLength = 2.f * MaxRadius + GBondTolerance;

		// ---- Atome markieren, die nicht gebunden werden sollen ----
		// Einatomige Residuen sind freie Ionen. Siehe Begruendung im Header.

		TBitArray<> Bondable(true, NumAtoms);
		for (const FMolResidue& Residue : Structure.Residues)
		{
			if (Residue.NumAtoms == 1 && Structure.AtomFlags.IsValidIndex(Residue.FirstAtom))
			{
				if ((Structure.AtomFlags[Residue.FirstAtom] & MolAtom_Hetatm) != 0)
				{
					Bondable[Residue.FirstAtom] = false;
				}
			}
		}

		// ---- Uniform-Grid aufbauen ----

		const FBox Bounds3d = Structure.GetBoundsAngstrom();
		const FVector3f BoundsMin(Bounds3d.Min);
		const FVector3f Extent = FVector3f(Bounds3d.Max) - BoundsMin;

		float CellSize = FMath::Max(MaxBondLength, 1.f);

		FIntVector GridDim;
		auto ComputeGridDim = [&Extent](float InCellSize)
		{
			return FIntVector(
				FMath::Max(1, FMath::CeilToInt(Extent.X / InCellSize) + 1),
				FMath::Max(1, FMath::CeilToInt(Extent.Y / InCellSize) + 1),
				FMath::Max(1, FMath::CeilToInt(Extent.Z / InCellSize) + 1));
		};

		GridDim = ComputeGridDim(CellSize);
		while (static_cast<int64>(GridDim.X) * GridDim.Y * GridDim.Z > GMaxGridCells)
		{
			CellSize *= 2.f;
			GridDim = ComputeGridDim(CellSize);
		}

		const int32 NumCells = GridDim.X * GridDim.Y * GridDim.Z;
		const float InvCellSize = 1.f / CellSize;

		// Zellindex je Atom — rein rechnerisch und unabhaengig, also parallel.
		TArray<int32> AtomCell;
		AtomCell.SetNumUninitialized(NumAtoms);

		ParallelFor(NumAtoms, [&](int32 i)
		{
			const FVector3f Local = (Positions[i] - BoundsMin) * InvCellSize;
			const int32 X = FMath::Clamp(FMath::FloorToInt(Local.X), 0, GridDim.X - 1);
			const int32 Y = FMath::Clamp(FMath::FloorToInt(Local.Y), 0, GridDim.Y - 1);
			const int32 Z = FMath::Clamp(FMath::FloorToInt(Local.Z), 0, GridDim.Z - 1);
			AtomCell[i] = (Z * GridDim.Y + Y) * GridDim.X + X;
		}, NumAtoms < GBondParallelThreshold ? EParallelForFlags::ForceSingleThread : EParallelForFlags::None);

		// Counting Sort: zaehlen, Praefixsumme, einsortieren. Ergebnis ist ein
		// zusammenhaengendes Array, in dem die Atome jeder Zelle nebeneinander liegen.
		TArray<int32> CellStart;
		CellStart.SetNumZeroed(NumCells + 1);

		for (int32 i = 0; i < NumAtoms; ++i)
		{
			++CellStart[AtomCell[i] + 1];
		}
		for (int32 c = 0; c < NumCells; ++c)
		{
			CellStart[c + 1] += CellStart[c];
		}

		TArray<int32> CellAtoms;
		CellAtoms.SetNumUninitialized(NumAtoms);
		{
			TArray<int32> Cursor = CellStart;
			for (int32 i = 0; i < NumAtoms; ++i)
			{
				CellAtoms[Cursor[AtomCell[i]]++] = i;
			}
		}

		// ---- Paarungslauf ----

		const int32 NumChunks = (NumAtoms >= GBondParallelThreshold)
			? FMath::Clamp(FTaskGraphInterface::Get().GetNumWorkerThreads(), 1, 64)
			: 1;
		const int32 ChunkSize = FMath::DivideAndRoundUp(NumAtoms, NumChunks);

		TArray<TArray<FMolBond>> PerChunkBonds;
		PerChunkBonds.SetNum(NumChunks);

		ParallelFor(NumChunks, [&](int32 ChunkIndex)
		{
			TArray<FMolBond>& OutBonds = PerChunkBonds[ChunkIndex];
			// Erfahrungswert: in Proteinen hat ein Atom im Mittel gut zwei Bindungen,
			// jede zaehlt aber nur beim kleineren Index. Ein Slot je Atom passt gut.
			OutBonds.Reserve(ChunkSize);

			const int32 Begin = ChunkIndex * ChunkSize;
			const int32 End = FMath::Min(Begin + ChunkSize, NumAtoms);

			for (int32 i = Begin; i < End; ++i)
			{
				if (!Bondable[i])
				{
					continue;
				}

				const FVector3f Pi = Positions[i];
				const float Ri = CovalentRadii[i];

				const int32 Cell = AtomCell[i];
				const int32 CellX = Cell % GridDim.X;
				const int32 CellY = (Cell / GridDim.X) % GridDim.Y;
				const int32 CellZ = Cell / (GridDim.X * GridDim.Y);

				for (int32 dz = -1; dz <= 1; ++dz)
				{
					const int32 Z = CellZ + dz;
					if (Z < 0 || Z >= GridDim.Z) { continue; }

					for (int32 dy = -1; dy <= 1; ++dy)
					{
						const int32 Y = CellY + dy;
						if (Y < 0 || Y >= GridDim.Y) { continue; }

						for (int32 dx = -1; dx <= 1; ++dx)
						{
							const int32 X = CellX + dx;
							if (X < 0 || X >= GridDim.X) { continue; }

							const int32 Neighbour = (Z * GridDim.Y + Y) * GridDim.X + X;
							const int32 SliceBegin = CellStart[Neighbour];
							const int32 SliceEnd = CellStart[Neighbour + 1];

							for (int32 s = SliceBegin; s < SliceEnd; ++s)
							{
								const int32 j = CellAtoms[s];

								// Jedes Paar genau einmal, und zwar beim kleineren Index.
								// Das haelt die Liste duplikatfrei, ohne dass Threads sich
								// abstimmen muessten.
								if (j <= i || !Bondable[j])
								{
									continue;
								}

								const float MaxDist = Ri + CovalentRadii[j] + GBondTolerance;
								const float DistSq = FVector3f::DistSquared(Pi, Positions[j]);

								if (DistSq <= MaxDist * MaxDist && DistSq >= GMinBondDistance * GMinBondDistance)
								{
									FMolBond& Bond = OutBonds.AddDefaulted_GetRef();
									Bond.AtomA = i;
									Bond.AtomB = j;
									Bond.Order = 1;
								}
							}
						}
					}
				}
			}
		}, NumChunks == 1 ? EParallelForFlags::ForceSingleThread : EParallelForFlags::None);

		int32 TotalBonds = 0;
		for (const TArray<FMolBond>& Chunk : PerChunkBonds)
		{
			TotalBonds += Chunk.Num();
		}

		Structure.Bonds.Reserve(TotalBonds);
		for (const TArray<FMolBond>& Chunk : PerChunkBonds)
		{
			Structure.Bonds.Append(Chunk);
		}

		UE_LOG(LogMolecularForge, Verbose, TEXT("Bindungen abgeleitet: %d bei %d Atomen (Zellkante %.2f A, %d Zellen)"),
			Structure.Bonds.Num(), NumAtoms, CellSize, NumCells);
	}
}
