// Copyright Silvan Teufel. All Rights Reserved.

#include "MolStructureAssembler.h"
#include "MolecularStructure.h"
#include "MolResidueTable.h"

namespace MolecularForge
{
	void AssembleStructure(
		const TArray<FMolRawAtom>& RawAtoms,
		const TArray<uint8>& ChainBreakBefore,
		const TArray<FMolSecondaryRange>& SecondaryRanges,
		const FMolLoadOptions& Options,
		UMolecularStructure& OutStructure,
		FMolAssembleStats& OutStats)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(MolecularForge_AssembleStructure);

		const int32 NumRaw = RawAtoms.Num();
		const bool bHaveChainBreaks = ChainBreakBefore.Num() == NumRaw;

		OutStructure.PreallocateAtoms(NumRaw);

		int32 OutIndex = 0;
		int32 Discarded = 0;

		int32 CurrentChainIndex = INDEX_NONE;
		int32 CurrentResidueIndex = INDEX_NONE;
		FName LastChainId = NAME_None;
		int32 LastResidueSeq = TNumericLimits<int32>::Min();
		uint8 LastInsertionCode = 0;
		FName LastResidueName = NAME_None;
		bool bHaveCurrent = false;

		for (int32 RawIndex = 0; RawIndex < NumRaw; ++RawIndex)
		{
			const FMolRawAtom& Raw = RawAtoms[RawIndex];
			if (!Raw.bValid)
			{
				continue;
			}

			const EMolResidueClass ResidueClass = ClassifyResidue(Raw.ResidueName);
			const bool bIsWater = ResidueClass == EMolResidueClass::Water;

			if (Options.bDiscardWater && bIsWater)
			{
				++Discarded;
				continue;
			}
			if (Options.bDiscardHydrogen && (Raw.Element == 1 || Raw.Element == 2))
			{
				++Discarded;
				continue;
			}
			// altLoc ' ' heisst "keine Alternative", 'A' ist per Konvention die primaere.
			if (Options.bPrimaryAltLocOnly && Raw.AltLoc != ' ' && Raw.AltLoc != 'A')
			{
				++Discarded;
				continue;
			}

			const bool bForcedBreak = bHaveChainBreaks && ChainBreakBefore[RawIndex] != 0;
			const bool bChainChanged = !bHaveCurrent || Raw.ChainId != LastChainId || bForcedBreak;

			if (bChainChanged)
			{
				FMolChain& NewChain = OutStructure.Chains.AddDefaulted_GetRef();
				NewChain.Id = Raw.ChainId;
				NewChain.FirstResidue = OutStructure.Residues.Num();
				NewChain.NumResidues = 0;
				NewChain.FirstAtom = OutIndex;
				NewChain.NumAtoms = 0;
				CurrentChainIndex = OutStructure.Chains.Num() - 1;

				// Kettenwechsel bedeutet immer auch Residuenwechsel.
				LastResidueSeq = TNumericLimits<int32>::Min();
				LastResidueName = NAME_None;
				LastInsertionCode = 0;
			}

			const bool bResidueChanged = bChainChanged
				|| Raw.ResidueSeq != LastResidueSeq
				|| Raw.InsertionCode != LastInsertionCode
				|| Raw.ResidueName != LastResidueName;

			if (bResidueChanged)
			{
				FMolResidue& NewResidue = OutStructure.Residues.AddDefaulted_GetRef();
				NewResidue.Name = Raw.ResidueName;
				NewResidue.SequenceNumber = Raw.ResidueSeq;
				NewResidue.ChainIndex = CurrentChainIndex;
				NewResidue.FirstAtom = OutIndex;
				NewResidue.NumAtoms = 0;
				NewResidue.InsertionCode = Raw.InsertionCode;
				CurrentResidueIndex = OutStructure.Residues.Num() - 1;

				OutStructure.Chains[CurrentChainIndex].NumResidues += 1;
			}

			uint8 Flags = MolAtom_None;
			if (Raw.bHetatm)											{ Flags |= MolAtom_Hetatm; }
			if (bIsWater)												{ Flags |= MolAtom_Water; }
			if (Raw.AltLoc != ' ' && Raw.AltLoc != 'A')					{ Flags |= MolAtom_AltLocSecondary; }
			if (IsBackboneAtomName(Raw.Name, ResidueClass))				{ Flags |= MolAtom_Backbone; }
			if (IsAnchorAtomName(Raw.Name, ResidueClass))				{ Flags |= MolAtom_Anchor; }

			OutStructure.AtomPositions[OutIndex] = Raw.Position;
			OutStructure.AtomElements[OutIndex] = Raw.Element;
			OutStructure.AtomResidueIndices[OutIndex] = CurrentResidueIndex;
			OutStructure.AtomBFactors[OutIndex] = Raw.BFactor;
			OutStructure.AtomOccupancies[OutIndex] = Raw.Occupancy;
			OutStructure.AtomFlags[OutIndex] = Flags;
			OutStructure.AtomNames[OutIndex] = Raw.Name;

			OutStructure.Residues[CurrentResidueIndex].NumAtoms += 1;
			OutStructure.Chains[CurrentChainIndex].NumAtoms += 1;

			LastChainId = Raw.ChainId;
			LastResidueSeq = Raw.ResidueSeq;
			LastInsertionCode = Raw.InsertionCode;
			LastResidueName = Raw.ResidueName;
			bHaveCurrent = true;

			++OutIndex;
		}

		// Die vorab reservierten Arrays auf die tatsaechlich uebernommene Zahl kuerzen.
		OutStructure.AtomPositions.SetNum(OutIndex, EAllowShrinking::No);
		OutStructure.AtomElements.SetNum(OutIndex, EAllowShrinking::No);
		OutStructure.AtomResidueIndices.SetNum(OutIndex, EAllowShrinking::No);
		OutStructure.AtomBFactors.SetNum(OutIndex, EAllowShrinking::No);
		OutStructure.AtomOccupancies.SetNum(OutIndex, EAllowShrinking::No);
		OutStructure.AtomFlags.SetNum(OutIndex, EAllowShrinking::No);
		OutStructure.AtomNames.SetNum(OutIndex, EAllowShrinking::No);

		// ---- Sekundaerstruktur eintragen ----
		// Erst jetzt moeglich, weil die Bereiche auf Ketten-ID und Sequenznummer verweisen,
		// und beides steht erst nach der Gruppierung fest.

		for (const FMolSecondaryRange& Range : SecondaryRanges)
		{
			for (FMolResidue& Residue : OutStructure.Residues)
			{
				if (!OutStructure.Chains.IsValidIndex(Residue.ChainIndex))
				{
					continue;
				}
				if (OutStructure.Chains[Residue.ChainIndex].Id != Range.ChainId)
				{
					continue;
				}
				if (Residue.SequenceNumber >= Range.FirstSeq && Residue.SequenceNumber <= Range.LastSeq)
				{
					Residue.SecondaryStructure = Range.Kind;
				}
			}
		}

		OutStats.NumAtomsKept = OutIndex;
		OutStats.NumAtomsDiscarded = Discarded;
	}
}
