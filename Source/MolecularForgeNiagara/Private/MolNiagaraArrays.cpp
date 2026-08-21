// Copyright 2026 Simulated Flow All Rights Reserved.

#include "MolNiagaraArrays.h"
#include "MolecularStructure.h"
#include "MolElementTable.h"

namespace MolecularForge::NiagaraParameterNames
{
	const FName AtomPositions(TEXT("MolAtomPositions"));
	const FName AtomColors(TEXT("MolAtomColors"));
	const FName AtomRadii(TEXT("MolAtomRadii"));
	const FName AtomCount(TEXT("MolAtomCount"));
	const FName BondStarts(TEXT("MolBondStarts"));
	const FName BondEnds(TEXT("MolBondEnds"));
	const FName BondCount(TEXT("MolBondCount"));
}

namespace
{
	/**
	 * Entscheidet, ob ein Atom uebergeben wird.
	 * Die Auswahlregeln entsprechen absichtlich denen der Kugeldarstellung — wer im
	 * Partikeleffekt dasselbe sehen will wie im Mesh, soll dieselben Schalter benutzen.
	 */
	bool ShouldIncludeAtom(const UMolecularStructure& Structure, int32 AtomIndex,
		const FMolNiagaraOptions& Options)
	{
		const uint8 Flags = Structure.AtomFlags[AtomIndex];

		if (!Options.bShowWater && (Flags & MolAtom_Water) != 0)
		{
			return false;
		}
		if (!Options.bShowHydrogen && Structure.AtomElements[AtomIndex] == 1)
		{
			return false;
		}

		switch (Options.AtomSubset)
		{
		case EMolRepresentation::Backbone:
			return (Flags & MolAtom_Backbone) != 0 || (Flags & MolAtom_Hetatm) != 0;

		case EMolRepresentation::AlphaTrace:
			return (Flags & MolAtom_Anchor) != 0 || (Flags & MolAtom_Hetatm) != 0;

		case EMolRepresentation::Cartoon:
			return (Flags & MolAtom_Hetatm) != 0;

		case EMolRepresentation::Surface:
		case EMolRepresentation::SpaceFilling:
		case EMolRepresentation::BallAndStick:
		default:
			return true;
		}
	}
}

namespace MolecularForge
{
	void BuildNiagaraArrays(const UMolecularStructure& Structure,
		const FMolNiagaraOptions& Options, FMolNiagaraArrays& OutArrays)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(MolecularForge_BuildNiagaraArrays);

		OutArrays.Reset();

		const int32 NumAtoms = Structure.GetNumAtoms();
		if (NumAtoms == 0)
		{
			return;
		}

		const float Scale = Options.UnitsPerAngstrom;

		// Erst sammeln, welche Atome ueberhaupt in Frage kommen. Ohne diesen Zwischenschritt
		// liesse sich nicht gleichmaessig ausduennen, weil die Gesamtzahl erst am Ende
		// feststuende — und dann waere schon die Haelfte geschrieben.
		TArray<int32> Candidates;
		Candidates.Reserve(NumAtoms);

		for (int32 a = 0; a < NumAtoms; ++a)
		{
			if (ShouldIncludeAtom(Structure, a, Options))
			{
				Candidates.Add(a);
			}
		}

		OutArrays.NumAtomsBeforeLimit = Candidates.Num();

		if (Candidates.IsEmpty())
		{
			return;
		}

		// Ausduennen mit gleichmaessigem Schritt. Vorne abzuschneiden waere einfacher,
		// wuerde aber schlicht das halbe Molekuel weglassen — und zwar sichtbar.
		const bool bLimited = Options.MaxAtoms > 0 && Candidates.Num() > Options.MaxAtoms;
		const int32 FinalCount = bLimited ? Options.MaxAtoms : Candidates.Num();

		OutArrays.Positions.Reserve(FinalCount);
		OutArrays.Colors.Reserve(FinalCount);
		OutArrays.Radii.Reserve(FinalCount);

		// Merkt sich, welches Atom an welcher Stelle gelandet ist — die Bindungen
		// brauchen das gleich, um nur uebrig gebliebene Enden zu verbinden.
		TSet<int32> Included;
		Included.Reserve(FinalCount);

		for (int32 i = 0; i < FinalCount; ++i)
		{
			const int32 SourceIndex = bLimited
				? Candidates[static_cast<int32>(
					(static_cast<int64>(i) * Candidates.Num()) / FinalCount)]
				: Candidates[i];

			Included.Add(SourceIndex);

			OutArrays.Positions.Add(FVector(Structure.AtomPositions[SourceIndex]) * Scale);
			OutArrays.Colors.Add(Structure.GetAtomColor(SourceIndex, Options.ColorScheme, Options.UniformColor));
			OutArrays.Radii.Add(
				GetElement(Structure.AtomElements[SourceIndex]).VdWRadius * Options.RadiusScale * Scale);
		}

		if (!Options.bIncludeBonds)
		{
			return;
		}

		OutArrays.BondStarts.Reserve(Structure.GetNumBonds());
		OutArrays.BondEnds.Reserve(Structure.GetNumBonds());

		for (const FMolBond& Bond : Structure.Bonds)
		{
			// Eine Bindung, deren Ende ausgeduennt wurde, wuerde ins Leere zeigen.
			if (!Included.Contains(Bond.AtomA) || !Included.Contains(Bond.AtomB))
			{
				continue;
			}

			OutArrays.BondStarts.Add(FVector(Structure.AtomPositions[Bond.AtomA]) * Scale);
			OutArrays.BondEnds.Add(FVector(Structure.AtomPositions[Bond.AtomB]) * Scale);
		}
	}
}
