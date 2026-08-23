// Copyright 2026 Silvan Teufel All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MolecularStructure.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * Testhilfen fuer die Darstellungsseite.
 *
 * Baut Minimalstrukturen aus vorgegebenen Ankerpositionen. Absichtlich keine vollstaendige
 * Peptidgeometrie: geprueft werden sollen Spline und Mesh-Erzeugung, nicht der Kettenbau.
 * Je Residuum reichen die drei Atome, die diese Verfahren anfassen — CA als Anker sowie
 * C und O fuer die Querrichtung.
 */
namespace MolTest
{
	/** Abstand zweier aufeinanderfolgender CA-Atome in einer Polypeptidkette. */
	inline constexpr float CaSpacing = 3.8f;

	inline UMolecularStructure* BuildChain(
		const TArray<FVector3f>& AnchorPositions,
		const TArray<FVector3f>& CarbonylDirections,
		const TArray<EMolSecondaryStructure>& SecondaryStructures = {})
	{
		UMolecularStructure* Structure = NewObject<UMolecularStructure>();

		const int32 NumResidues = AnchorPositions.Num();
		const int32 NumAtoms = NumResidues * 3;
		Structure->PreallocateAtoms(NumAtoms);

		FMolChain& Chain = Structure->Chains.AddDefaulted_GetRef();
		Chain.Id = FName("A");
		Chain.FirstResidue = 0;
		Chain.NumResidues = NumResidues;
		Chain.FirstAtom = 0;
		Chain.NumAtoms = NumAtoms;

		static const FName NameCA(TEXT("CA"));
		static const FName NameC(TEXT("C"));
		static const FName NameO(TEXT("O"));

		int32 AtomIndex = 0;
		for (int32 i = 0; i < NumResidues; ++i)
		{
			FMolResidue& Residue = Structure->Residues.AddDefaulted_GetRef();
			Residue.Name = FName("ALA");
			Residue.SequenceNumber = i + 1;
			Residue.ChainIndex = 0;
			Residue.FirstAtom = AtomIndex;
			Residue.NumAtoms = 3;

			const FVector3f Anchor = AnchorPositions[i];
			const FVector3f Carbonyl = CarbonylDirections.IsValidIndex(i)
				? CarbonylDirections[i].GetSafeNormal()
				: FVector3f::ZAxisVector;

			const FVector3f CarbonPos = Anchor + FVector3f(0.f, 0.5f, 0.f);
			const FVector3f OxygenPos = CarbonPos + Carbonyl * 1.23f;

			const FVector3f Positions[3] = { Anchor, CarbonPos, OxygenPos };
			const FName Names[3] = { NameCA, NameC, NameO };
			const uint8 Elements[3] = { 6, 6, 8 };
			const uint8 Flags[3] =
			{
				MolAtom_Backbone | MolAtom_Anchor,
				MolAtom_Backbone,
				MolAtom_Backbone
			};

			for (int32 k = 0; k < 3; ++k)
			{
				Structure->AtomPositions[AtomIndex] = Positions[k];
				Structure->AtomNames[AtomIndex] = Names[k];
				Structure->AtomElements[AtomIndex] = Elements[k];
				Structure->AtomResidueIndices[AtomIndex] = i;
				Structure->AtomBFactors[AtomIndex] = 0.f;
				Structure->AtomOccupancies[AtomIndex] = 1.f;
				Structure->AtomFlags[AtomIndex] = Flags[k];
				++AtomIndex;
			}
		}

		// FinalizeAfterLoad leitet die Kettenart ab und ueberschreibt die Sekundaerstruktur
		// nicht — die wird deshalb erst danach gesetzt.
		Structure->FinalizeAfterLoad();

		for (int32 i = 0; i < NumResidues; ++i)
		{
			if (SecondaryStructures.IsValidIndex(i))
			{
				Structure->Residues[i].SecondaryStructure = SecondaryStructures[i];
			}
		}

		return Structure;
	}

	/**
	 * Baut eine lose Ansammlung von Atomen ohne Polymerzusammenhang.
	 * Fuer alles, was nur Positionen und Radien braucht — etwa die Oberflaechenerzeugung.
	 */
	inline UMolecularStructure* BuildAtomCloud(const TArray<FVector3f>& Positions, uint8 Element = 6)
	{
		UMolecularStructure* Structure = NewObject<UMolecularStructure>();

		const int32 Count = Positions.Num();
		Structure->PreallocateAtoms(Count);

		FMolChain& Chain = Structure->Chains.AddDefaulted_GetRef();
		Chain.Id = FName("A");
		Chain.FirstResidue = 0;
		Chain.NumResidues = Count;
		Chain.FirstAtom = 0;
		Chain.NumAtoms = Count;

		for (int32 i = 0; i < Count; ++i)
		{
			FMolResidue& Residue = Structure->Residues.AddDefaulted_GetRef();
			Residue.Name = FName("LIG");
			Residue.SequenceNumber = i + 1;
			Residue.ChainIndex = 0;
			Residue.FirstAtom = i;
			Residue.NumAtoms = 1;

			Structure->AtomPositions[i] = Positions[i];
			Structure->AtomNames[i] = FName("C");
			Structure->AtomElements[i] = Element;
			Structure->AtomResidueIndices[i] = i;
			Structure->AtomBFactors[i] = 0.f;
			Structure->AtomOccupancies[i] = 1.f;
			Structure->AtomFlags[i] = MolAtom_Hetatm;
		}

		Structure->FinalizeAfterLoad();
		return Structure;
	}

	/** Gerade Kette entlang X mit typischem CA-Abstand. */
	inline TArray<FVector3f> MakeStraightAnchors(int32 Count)
	{
		TArray<FVector3f> Anchors;
		Anchors.Reserve(Count);
		for (int32 i = 0; i < Count; ++i)
		{
			Anchors.Add(FVector3f(i * CaSpacing, 0.f, 0.f));
		}
		return Anchors;
	}
}

#endif // WITH_DEV_AUTOMATION_TESTS
