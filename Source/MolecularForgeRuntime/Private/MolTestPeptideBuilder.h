// Copyright 2026 Silvan Teufel All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MolecularStructure.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * Baut Peptidketten mit idealer Geometrie und vorgegebenen Torsionswinkeln.
 *
 * Der Sinn ist, Verfahren gegen bekannte Physik pruefen zu koennen statt gegen selbst
 * gesetzte Erwartungswerte: eine Kette mit phi = -57 und psi = -47 *ist* eine rechtsgaengige
 * Alpha-Helix. Wer dort keine findet oder die Winkel anders zurueckliest, hat einen Fehler.
 *
 * Atomreihenfolge je Residuum: N, CA, C, O. Damit liegt Atom `Residuum * 4 + k`.
 */
namespace MolTestPeptide
{
	// Standardgeometrie des Proteinrueckgrats (Engh & Huber).
	inline constexpr float BondNCA = 1.458f;
	inline constexpr float BondCAC = 1.525f;
	inline constexpr float BondCN  = 1.329f;
	inline constexpr float BondCO  = 1.231f;

	inline constexpr float AngleNCAC = 111.2f;
	inline constexpr float AngleCACN = 116.2f;
	inline constexpr float AngleCNCA = 121.7f;
	inline constexpr float AngleCACO = 120.8f;

	/** Torsionswinkel einer idealen Alpha-Helix. */
	inline constexpr float HelixPhi = -57.f;
	inline constexpr float HelixPsi = -47.f;

	/** Torsionswinkel eines gestreckten Beta-Strangs. */
	inline constexpr float StrandPhi = -139.f;
	inline constexpr float StrandPsi = 135.f;

	/** Versatz der vier Rueckgratatome innerhalb eines Residuums. */
	inline constexpr int32 OffsetN = 0;
	inline constexpr int32 OffsetCA = 1;
	inline constexpr int32 OffsetC = 2;
	inline constexpr int32 OffsetO = 3;

	/**
	 * Setzt ein Atom relativ zu drei bekannten Atomen (NeRF-Verfahren).
	 * Gegeben sind Bindungslaenge C-D, Bindungswinkel B-C-D und Torsionswinkel A-B-C-D.
	 */
	inline FVector3f PlaceAtom(const FVector3f& A, const FVector3f& B, const FVector3f& C,
		float BondLength, float BondAngleDeg, float TorsionDeg)
	{
		const float Theta = FMath::DegreesToRadians(BondAngleDeg);
		const float Torsion = FMath::DegreesToRadians(TorsionDeg);

		const FVector3f BC = (C - B).GetSafeNormal();
		const FVector3f Normal = FVector3f::CrossProduct(B - A, BC).GetSafeNormal();
		const FVector3f InPlane = FVector3f::CrossProduct(Normal, BC);

		return C
			+ BC * (-BondLength * FMath::Cos(Theta))
			+ InPlane * (BondLength * FMath::Sin(Theta) * FMath::Cos(Torsion))
			+ Normal * (BondLength * FMath::Sin(Theta) * FMath::Sin(Torsion));
	}

	inline void BuildIdealPeptide(int32 NumResidues, float PhiDeg, float PsiDeg,
		const TArray<FName>& ResidueNames, UMolecularStructure& Out)
	{
		Out.Reset();

		TArray<FVector3f> N, CA, C, O;
		N.SetNum(NumResidues);
		CA.SetNum(NumResidues);
		C.SetNum(NumResidues);
		O.SetNum(NumResidues);

		// Erstes Residuum als Startpunkt in der xy-Ebene.
		N[0] = FVector3f::ZeroVector;
		CA[0] = FVector3f(BondNCA, 0.f, 0.f);

		const float StartAngle = FMath::DegreesToRadians(AngleNCAC);
		C[0] = CA[0] + FVector3f(-FMath::Cos(StartAngle), FMath::Sin(StartAngle), 0.f) * BondCAC;

		for (int32 i = 0; i < NumResidues; ++i)
		{
			// Der Carbonyl-Sauerstoff steht dem folgenden Stickstoff gegenueber.
			O[i] = PlaceAtom(N[i], CA[i], C[i], BondCO, AngleCACO, PsiDeg + 180.f);

			if (i + 1 < NumResidues)
			{
				N[i + 1] = PlaceAtom(N[i], CA[i], C[i], BondCN, AngleCACN, PsiDeg);
				// Omega ist in trans-Konfiguration festgenagelt, wie in fast allen Proteinen.
				CA[i + 1] = PlaceAtom(CA[i], C[i], N[i + 1], BondNCA, AngleCNCA, 180.f);
				C[i + 1] = PlaceAtom(C[i], N[i + 1], CA[i + 1], BondCAC, AngleNCAC, PhiDeg);
			}
		}

		const int32 NumAtoms = NumResidues * 4;
		Out.PreallocateAtoms(NumAtoms);

		FMolChain& Chain = Out.Chains.AddDefaulted_GetRef();
		Chain.Id = FName("A");
		Chain.FirstResidue = 0;
		Chain.NumResidues = NumResidues;
		Chain.FirstAtom = 0;
		Chain.NumAtoms = NumAtoms;

		static const FName NameN(TEXT("N"));
		static const FName NameCA(TEXT("CA"));
		static const FName NameC(TEXT("C"));
		static const FName NameO(TEXT("O"));

		int32 AtomIndex = 0;
		for (int32 i = 0; i < NumResidues; ++i)
		{
			FMolResidue& Residue = Out.Residues.AddDefaulted_GetRef();
			Residue.Name = ResidueNames.IsValidIndex(i) ? ResidueNames[i] : FName("ALA");
			Residue.SequenceNumber = i + 1;
			Residue.ChainIndex = 0;
			Residue.FirstAtom = AtomIndex;
			Residue.NumAtoms = 4;

			const FVector3f Positions[4] = { N[i], CA[i], C[i], O[i] };
			const FName Names[4] = { NameN, NameCA, NameC, NameO };
			const uint8 Elements[4] = { 7, 6, 6, 8 };

			for (int32 k = 0; k < 4; ++k)
			{
				Out.AtomPositions[AtomIndex] = Positions[k];
				Out.AtomNames[AtomIndex] = Names[k];
				Out.AtomElements[AtomIndex] = Elements[k];
				Out.AtomResidueIndices[AtomIndex] = i;
				Out.AtomBFactors[AtomIndex] = 0.f;
				Out.AtomOccupancies[AtomIndex] = 1.f;
				Out.AtomFlags[AtomIndex] = MolAtom_Backbone | (k == OffsetCA ? MolAtom_Anchor : 0);
				++AtomIndex;
			}
		}

		Out.FinalizeAfterLoad();
	}
}

#endif // WITH_DEV_AUTOMATION_TESTS
