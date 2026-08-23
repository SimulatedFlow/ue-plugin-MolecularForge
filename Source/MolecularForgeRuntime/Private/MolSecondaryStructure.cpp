// Copyright 2026 Silvan Teufel All Rights Reserved.

#include "MolSecondaryStructure.h"
#include "MolecularStructure.h"
#include "MolResidueTable.h"
#include "Async/ParallelFor.h"

namespace
{
	/**
	 * q1*q2*f aus der Kabsch-Sander-Formel: 0,42 e * 0,20 e * 332 kcal*A/(mol*e^2).
	 * Ergebnis der Energieformel ist damit kcal/mol bei Abstaenden in Angstroem.
	 */
	constexpr float GChargeFactor = 27.888f;

	/** Unterhalb dieser Energie gilt die Wasserstoffbruecke als vorhanden. */
	constexpr float GHBondEnergyCutoff = -0.5f;

	/**
	 * Liegen zwei der beteiligten Atome naeher als das beieinander, ist die Formel
	 * nicht mehr sinnvoll (sie divergiert). DSSP setzt in dem Fall einen festen,
	 * stark negativen Wert — die Bruecke gilt dann als sicher vorhanden.
	 */
	constexpr float GMinimalDistance = 0.5f;
	constexpr float GClampedEnergy = -9.9f;

	/** Maximaler CA-CA-Abstand, bei dem eine Rueckgratbruecke ueberhaupt moeglich ist. */
	constexpr float GCaCutoff = 9.0f;

	/** Laenge der N-H-Bindung in Angstroem. */
	constexpr float GNHBondLength = 1.0f;

	/** Hoechster C-N-Abstand, bei dem zwei Residuen noch als peptidverknuepft gelten. */
	constexpr float GPeptideBondMax = 2.5f;

	constexpr int32 GParallelThreshold = 256;

	/** Rueckgratdaten eines Residuums, aufbereitet fuer den Energielauf. */
	struct FBackbone
	{
		FVector3f N = FVector3f::ZeroVector;
		FVector3f CA = FVector3f::ZeroVector;
		FVector3f C = FVector3f::ZeroVector;
		FVector3f O = FVector3f::ZeroVector;
		FVector3f H = FVector3f::ZeroVector;

		int32 ChainIndex = INDEX_NONE;

		/** N, CA, C und O sind alle vorhanden. */
		bool bComplete = false;

		/** Kann als Donor auftreten: hat ein Amid-H (also nicht Prolin, und Vorgaenger bekannt). */
		bool bDonor = false;

		/** Kann als Akzeptor auftreten: hat C und O. */
		bool bAcceptor = false;
	};

	/**
	 * Die zwei besten Bruecken, in denen dieses Residuum den N-H-Donor stellt.
	 * DSSP behaelt ebenfalls genau zwei — mehr sind geometrisch kaum moeglich, und die
	 * feste Groesse haelt die Struktur POD und den Speicher vorhersagbar.
	 */
	struct FHBondPartners
	{
		int32 Acceptor[2] = { INDEX_NONE, INDEX_NONE };
		float Energy[2] = { 0.f, 0.f };

		void Offer(int32 InAcceptor, float InEnergy)
		{
			if (InEnergy < Energy[0])
			{
				Acceptor[1] = Acceptor[0];
				Energy[1] = Energy[0];
				Acceptor[0] = InAcceptor;
				Energy[0] = InEnergy;
			}
			else if (InEnergy < Energy[1])
			{
				Acceptor[1] = InAcceptor;
				Energy[1] = InEnergy;
			}
		}

		bool AcceptsFrom(int32 InAcceptor) const
		{
			return InAcceptor != INDEX_NONE && (Acceptor[0] == InAcceptor || Acceptor[1] == InAcceptor);
		}
	};

	/**
	 * Energie der Wasserstoffbruecke zwischen der C=O-Gruppe des Akzeptors und der
	 * N-H-Gruppe des Donors, nach Kabsch und Sander.
	 */
	float HBondEnergy(const FBackbone& Donor, const FBackbone& Acceptor)
	{
		const float rON = FVector3f::Dist(Acceptor.O, Donor.N);
		const float rCH = FVector3f::Dist(Acceptor.C, Donor.H);
		const float rOH = FVector3f::Dist(Acceptor.O, Donor.H);
		const float rCN = FVector3f::Dist(Acceptor.C, Donor.N);

		if (rON < GMinimalDistance || rCH < GMinimalDistance ||
			rOH < GMinimalDistance || rCN < GMinimalDistance)
		{
			return GClampedEnergy;
		}

		return GChargeFactor * (1.f / rON + 1.f / rCH - 1.f / rOH - 1.f / rCN);
	}
}

namespace MolecularForge
{
	void ComputeSecondaryStructure(UMolecularStructure& Structure)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(MolecularForge_ComputeSecondaryStructure);

		const int32 NumResidues = Structure.GetNumResidues();
		for (FMolResidue& Residue : Structure.Residues)
		{
			Residue.SecondaryStructure = EMolSecondaryStructure::Coil;
		}

		if (NumResidues < 4)
		{
			return;
		}

		// ---- Rueckgrat einsammeln ----

		TArray<FBackbone> Backbones;
		Backbones.SetNum(NumResidues);

		static const FName NameN(TEXT("N"));
		static const FName NameCA(TEXT("CA"));
		static const FName NameC(TEXT("C"));
		static const FName NameO(TEXT("O"));
		static const FName NamePro(TEXT("PRO"));

		for (int32 r = 0; r < NumResidues; ++r)
		{
			const FMolResidue& Residue = Structure.Residues[r];
			if (ClassifyResidue(Residue.Name) != EMolResidueClass::AminoAcid)
			{
				continue;
			}

			FBackbone& Backbone = Backbones[r];
			Backbone.ChainIndex = Residue.ChainIndex;

			bool bHasN = false, bHasCA = false, bHasC = false, bHasO = false;

			const int32 End = Residue.FirstAtom + Residue.NumAtoms;
			for (int32 a = Residue.FirstAtom; a < End && a < Structure.GetNumAtoms(); ++a)
			{
				const FName AtomName = Structure.AtomNames[a];
				if (AtomName == NameN)		{ Backbone.N = Structure.AtomPositions[a];  bHasN = true; }
				else if (AtomName == NameCA){ Backbone.CA = Structure.AtomPositions[a]; bHasCA = true; }
				else if (AtomName == NameC)	{ Backbone.C = Structure.AtomPositions[a];  bHasC = true; }
				else if (AtomName == NameO)	{ Backbone.O = Structure.AtomPositions[a];  bHasO = true; }
			}

			Backbone.bComplete = bHasN && bHasCA && bHasC && bHasO;
			Backbone.bAcceptor = bHasC && bHasO;
		}

		// ---- Amid-Wasserstoff schaetzen ----
		// Kristallstrukturen enthalten fast nie Wasserstoff, aber seine Lage folgt aus dem
		// Rueckgrat: er sitzt am Stickstoff, in Gegenrichtung zur C=O-Bindung des Vorgaengers.
		// Prolin hat an dieser Stelle keinen Wasserstoff und faellt als Donor aus — das ist
		// kein Detail, sondern der Grund, warum Prolin Helices bricht.

		for (int32 r = 1; r < NumResidues; ++r)
		{
			FBackbone& Backbone = Backbones[r];
			const FBackbone& Previous = Backbones[r - 1];

			if (!Backbone.bComplete || !Previous.bComplete)
			{
				continue;
			}
			if (Backbone.ChainIndex != Previous.ChainIndex)
			{
				continue;
			}
			if (Structure.Residues[r].Name == NamePro)
			{
				continue;
			}

			// Fehlende Reste in Kristallstrukturen hinterlassen Luecken. Zwei Residuen, die
			// im Array nebeneinander stehen, muessen deshalb nicht verknuepft sein.
			if (FVector3f::Dist(Previous.C, Backbone.N) > GPeptideBondMax)
			{
				continue;
			}

			const FVector3f CarbonylDirection = (Previous.C - Previous.O).GetSafeNormal();
			if (CarbonylDirection.IsNearlyZero())
			{
				continue;
			}

			Backbone.H = Backbone.N + CarbonylDirection * GNHBondLength;
			Backbone.bDonor = true;
		}

		// ---- Nachbarschaftsgitter ueber die CA-Atome ----

		TArray<int32> Candidates;
		Candidates.Reserve(NumResidues);
		for (int32 r = 0; r < NumResidues; ++r)
		{
			if (Backbones[r].bComplete)
			{
				Candidates.Add(r);
			}
		}

		if (Candidates.Num() < 4)
		{
			return;
		}

		FVector3f GridMin(TNumericLimits<float>::Max());
		FVector3f GridMax(TNumericLimits<float>::Lowest());
		for (int32 r : Candidates)
		{
			GridMin = FVector3f::Min(GridMin, Backbones[r].CA);
			GridMax = FVector3f::Max(GridMax, Backbones[r].CA);
		}

		const float CellSize = GCaCutoff;
		const FVector3f Extent = GridMax - GridMin;
		const FIntVector GridDim(
			FMath::Max(1, FMath::CeilToInt(Extent.X / CellSize) + 1),
			FMath::Max(1, FMath::CeilToInt(Extent.Y / CellSize) + 1),
			FMath::Max(1, FMath::CeilToInt(Extent.Z / CellSize) + 1));

		const int32 NumCells = GridDim.X * GridDim.Y * GridDim.Z;

		auto CellOf = [&](const FVector3f& Position)
		{
			const FVector3f Local = (Position - GridMin) / CellSize;
			const int32 X = FMath::Clamp(FMath::FloorToInt(Local.X), 0, GridDim.X - 1);
			const int32 Y = FMath::Clamp(FMath::FloorToInt(Local.Y), 0, GridDim.Y - 1);
			const int32 Z = FMath::Clamp(FMath::FloorToInt(Local.Z), 0, GridDim.Z - 1);
			return FIntVector(X, Y, Z);
		};

		TArray<int32> CellStart;
		CellStart.SetNumZeroed(NumCells + 1);

		TArray<int32> CandidateCell;
		CandidateCell.SetNumUninitialized(Candidates.Num());

		for (int32 i = 0; i < Candidates.Num(); ++i)
		{
			const FIntVector Cell = CellOf(Backbones[Candidates[i]].CA);
			CandidateCell[i] = (Cell.Z * GridDim.Y + Cell.Y) * GridDim.X + Cell.X;
			++CellStart[CandidateCell[i] + 1];
		}
		for (int32 c = 0; c < NumCells; ++c)
		{
			CellStart[c + 1] += CellStart[c];
		}

		TArray<int32> CellResidues;
		CellResidues.SetNumUninitialized(Candidates.Num());
		{
			TArray<int32> Cursor = CellStart;
			for (int32 i = 0; i < Candidates.Num(); ++i)
			{
				CellResidues[Cursor[CandidateCell[i]]++] = Candidates[i];
			}
		}

		// ---- Energielauf ----
		// Jeder Donor sucht seine Akzeptoren selbst und schreibt nur in den eigenen Eintrag.

		TArray<FHBondPartners> Partners;
		Partners.SetNum(NumResidues);

		const float CaCutoffSq = GCaCutoff * GCaCutoff;

		ParallelFor(Candidates.Num(), [&](int32 CandidateIndex)
		{
			const int32 DonorRes = Candidates[CandidateIndex];
			const FBackbone& Donor = Backbones[DonorRes];
			if (!Donor.bDonor)
			{
				return;
			}

			FHBondPartners& Out = Partners[DonorRes];
			const FIntVector Cell = CellOf(Donor.CA);

			for (int32 dz = -1; dz <= 1; ++dz)
			{
				const int32 Z = Cell.Z + dz;
				if (Z < 0 || Z >= GridDim.Z) { continue; }

				for (int32 dy = -1; dy <= 1; ++dy)
				{
					const int32 Y = Cell.Y + dy;
					if (Y < 0 || Y >= GridDim.Y) { continue; }

					for (int32 dx = -1; dx <= 1; ++dx)
					{
						const int32 X = Cell.X + dx;
						if (X < 0 || X >= GridDim.X) { continue; }

						const int32 Neighbour = (Z * GridDim.Y + Y) * GridDim.X + X;
						for (int32 s = CellStart[Neighbour]; s < CellStart[Neighbour + 1]; ++s)
						{
							const int32 AcceptorRes = CellResidues[s];
							const FBackbone& Acceptor = Backbones[AcceptorRes];

							if (!Acceptor.bAcceptor)
							{
								continue;
							}
							// Ein Residuum bindet nicht an sich selbst oder seinen direkten
							// Nachbarn — die Geometrie der Peptidbindung laesst das nicht zu.
							if (FMath::Abs(AcceptorRes - DonorRes) < 2)
							{
								continue;
							}
							if (FVector3f::DistSquared(Donor.CA, Acceptor.CA) > CaCutoffSq)
							{
								continue;
							}

							const float Energy = HBondEnergy(Donor, Acceptor);
							if (Energy < GHBondEnergyCutoff)
							{
								Out.Offer(AcceptorRes, Energy);
							}
						}
					}
				}
			}
		}, Candidates.Num() < GParallelThreshold ? EParallelForFlags::ForceSingleThread : EParallelForFlags::None);

		// ---- Muster auswerten ----
		// Ab hier sequenziell und billig. HBond(Co, Nh) heisst: die C=O-Gruppe von Co
		// nimmt die Bruecke der N-H-Gruppe von Nh auf — die Schreibweise aus der Arbeit.

		auto HBond = [&Partners, NumResidues](int32 Co, int32 Nh)
		{
			if (Co < 0 || Nh < 0 || Co >= NumResidues || Nh >= NumResidues)
			{
				return false;
			}
			return Partners[Nh].AcceptsFrom(Co);
		};

		auto SameChain = [&Backbones, NumResidues](int32 A, int32 B)
		{
			if (A < 0 || B < 0 || A >= NumResidues || B >= NumResidues)
			{
				return false;
			}
			return Backbones[A].ChainIndex != INDEX_NONE
				&& Backbones[A].ChainIndex == Backbones[B].ChainIndex;
		};

		// n-Turn an Position i heisst: Bruecke von der C=O-Gruppe von i zur N-H-Gruppe von i+n.
		auto Turn = [&](int32 i, int32 n)
		{
			return SameChain(i, i + n) && HBond(i, i + n);
		};

		TArray<EMolSecondaryStructure> Assigned;
		Assigned.Init(EMolSecondaryStructure::Coil, NumResidues);

		auto MarkRange = [&Assigned, NumResidues](int32 First, int32 Last, EMolSecondaryStructure Kind)
		{
			for (int32 r = FMath::Max(0, First); r <= FMath::Min(NumResidues - 1, Last); ++r)
			{
				if (Assigned[r] == EMolSecondaryStructure::Coil)
				{
					Assigned[r] = Kind;
				}
			}
		};

		// Alpha-Helix zuerst: zwei aufeinanderfolgende 4-Turns. Das ist der haeufigste
		// und stabilste Zustand, deshalb hat er Vorrang vor allem Weiteren.
		for (int32 i = 1; i < NumResidues; ++i)
		{
			if (Turn(i - 1, 4) && Turn(i, 4))
			{
				MarkRange(i, i + 3, EMolSecondaryStructure::Helix);
			}
		}

		// Faltblatt: Bruecken zwischen im Strang entfernten Residuen. Parallel und
		// antiparallel haben verschiedene Muster, beide zaehlen.
		//
		// Alle Residuenpaare durchzugehen waere quadratisch — bei einem Ribosom mit
		// zehntausenden Residuen sind das Milliarden Durchlaeufe fuer eine Handvoll Treffer.
		// Stattdessen liefern die gefundenen Wasserstoffbruecken die Kandidaten: jede Bruecke
		// schlaegt ihr Residuenpaar und dessen unmittelbare Nachbarn vor, denn alle vier
		// Bruecken-Muster verweisen nur auf Positionen mit Abstand eins. Da jedes Residuum
		// hoechstens zwei Bruecken traegt, bleibt die Kandidatenmenge linear.
		{
			TSet<uint64> CandidatePairs;
			CandidatePairs.Reserve(NumResidues * 4);

			auto AddCandidate = [&CandidatePairs, NumResidues](int32 A, int32 B)
			{
				if (A < 0 || B < 0 || A >= NumResidues || B >= NumResidues)
				{
					return;
				}
				const int32 Low = FMath::Min(A, B);
				const int32 High = FMath::Max(A, B);
				// Zu nah beieinander ist keine Bruecke, sondern eine Windung.
				if (High - Low < 3)
				{
					return;
				}
				CandidatePairs.Add((static_cast<uint64>(Low) << 32) | static_cast<uint32>(High));
			};

			for (int32 Nh = 0; Nh < NumResidues; ++Nh)
			{
				for (int32 Slot = 0; Slot < 2; ++Slot)
				{
					const int32 Co = Partners[Nh].Acceptor[Slot];
					if (Co == INDEX_NONE)
					{
						continue;
					}
					for (int32 di = -1; di <= 1; ++di)
					{
						for (int32 dj = -1; dj <= 1; ++dj)
						{
							AddCandidate(Co + di, Nh + dj);
						}
					}
				}
			}

			for (const uint64 Packed : CandidatePairs)
			{
				const int32 i = static_cast<int32>(Packed >> 32);
				const int32 j = static_cast<int32>(Packed & 0xFFFFFFFF);

				const bool bParallel =
					(HBond(i - 1, j) && HBond(j, i + 1)) ||
					(HBond(j - 1, i) && HBond(i, j + 1));

				const bool bAntiparallel =
					(HBond(i, j) && HBond(j, i)) ||
					(HBond(i - 1, j + 1) && HBond(j - 1, i + 1));

				if (bParallel || bAntiparallel)
				{
					MarkRange(i, i, EMolSecondaryStructure::Sheet);
					MarkRange(j, j, EMolSecondaryStructure::Sheet);
				}
			}
		}

		// 3-10- und Pi-Helix nur noch dort, wo nichts Staerkeres steht.
		for (int32 i = 1; i < NumResidues; ++i)
		{
			if ((Turn(i - 1, 3) && Turn(i, 3)))
			{
				MarkRange(i, i + 2, EMolSecondaryStructure::Helix);
			}
			if ((Turn(i - 1, 5) && Turn(i, 5)))
			{
				MarkRange(i, i + 4, EMolSecondaryStructure::Helix);
			}
		}

		// Uebrige n-Turns werden als Turn markiert.
		for (int32 i = 0; i < NumResidues; ++i)
		{
			for (int32 n = 3; n <= 5; ++n)
			{
				if (Turn(i, n))
				{
					MarkRange(i + 1, i + n - 1, EMolSecondaryStructure::Turn);
				}
			}
		}

		for (int32 r = 0; r < NumResidues; ++r)
		{
			Structure.Residues[r].SecondaryStructure = Assigned[r];
		}

		int32 NumHelix = 0, NumSheet = 0, NumTurn = 0;
		for (EMolSecondaryStructure Kind : Assigned)
		{
			switch (Kind)
			{
			case EMolSecondaryStructure::Helix:	++NumHelix; break;
			case EMolSecondaryStructure::Sheet:	++NumSheet; break;
			case EMolSecondaryStructure::Turn:	++NumTurn;  break;
			default: break;
			}
		}

		UE_LOG(LogMolecularForge, Verbose,
			TEXT("Sekundaerstruktur berechnet: %d Helix, %d Faltblatt, %d Turn von %d Residuen."),
			NumHelix, NumSheet, NumTurn, NumResidues);
	}

	void ApplySecondaryStructurePolicy(UMolecularStructure& Structure,
		const FMolLoadOptions& Options, bool bFileHadAnnotations)
	{
		switch (Options.SecondaryStructureSource)
		{
		case EMolSecondaryStructureSource::FromFile:
			break;

		case EMolSecondaryStructureSource::Compute:
			ComputeSecondaryStructure(Structure);
			break;

		case EMolSecondaryStructureSource::FromFileElseCompute:
			if (!bFileHadAnnotations)
			{
				ComputeSecondaryStructure(Structure);
			}
			break;
		}
	}
}
