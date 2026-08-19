// Copyright Simulated Flow. All Rights Reserved.

#include "MolecularStructure.h"
#include "MolElementTable.h"
#include "MolResidueTable.h"
#include "Async/ParallelFor.h"

namespace
{
	/** Farben der offiziellen AlphaFold-Konfidenzskala. Wiedererkennungswert zaehlt hier mehr als Geschmack. */
	const FLinearColor GPlddtVeryHigh	= FLinearColor::FromSRGBColor(FColor(0x00, 0x53, 0xD6));	// > 90
	const FLinearColor GPlddtConfident	= FLinearColor::FromSRGBColor(FColor(0x65, 0xCB, 0xF3));	// 70..90
	const FLinearColor GPlddtLow		= FLinearColor::FromSRGBColor(FColor(0xFF, 0xDB, 0x13));	// 50..70
	const FLinearColor GPlddtVeryLow	= FLinearColor::FromSRGBColor(FColor(0xFF, 0x7D, 0x45));	// < 50

	const FLinearColor GSsHelix	= FLinearColor::FromSRGBColor(FColor(0xE8, 0x35, 0x5C));
	const FLinearColor GSsSheet	= FLinearColor::FromSRGBColor(FColor(0xFF, 0xC1, 0x07));
	const FLinearColor GSsTurn	= FLinearColor::FromSRGBColor(FColor(0x64, 0xB5, 0xF6));
	const FLinearColor GSsCoil	= FLinearColor::FromSRGBColor(FColor(0xBD, 0xBD, 0xBD));

	/**
	 * Kettenfarbe ueber den goldenen Schnitt im Farbton.
	 * Eine feste Palette hat immer eine Kettenzahl, ab der sie sich wiederholt; das hier
	 * bleibt bis weit ueber 20 Ketten hinaus unterscheidbar, ohne Tabelle.
	 */
	FLinearColor ChainColor(int32 ChainIndex)
	{
		constexpr float GoldenRatioConjugate = 0.6180339887498949f;
		const float Hue = FMath::Frac(static_cast<float>(ChainIndex) * GoldenRatioConjugate) * 360.f;
		return FLinearColor(Hue, 0.62f, 0.92f).HSVToLinearRGB();
	}

	/** Kyte-Doolittle-Hydropathie, indiziert ueber den Einbuchstaben-Code. */
	float KyteDoolittle(uint8 OneLetter)
	{
		switch (OneLetter)
		{
		case 'I': return  4.5f;
		case 'V': return  4.2f;
		case 'L': return  3.8f;
		case 'F': return  2.8f;
		case 'C': return  2.5f;
		case 'M': return  1.9f;
		case 'A': return  1.8f;
		case 'G': return -0.4f;
		case 'T': return -0.7f;
		case 'S': return -0.8f;
		case 'W': return -0.9f;
		case 'Y': return -1.3f;
		case 'P': return -1.6f;
		case 'H': return -3.2f;
		case 'E': return -3.5f;
		case 'Q': return -3.5f;
		case 'D': return -3.5f;
		case 'N': return -3.5f;
		case 'K': return -3.9f;
		case 'R': return -4.5f;
		default:  return  0.0f;
		}
	}

	/** Ab dieser Atomzahl lohnt der Thread-Overhead. Darunter ist die Schleife schneller allein. */
	constexpr int32 GParallelThreshold = 8192;
}

namespace MolecularForge
{
	const FString& GetAlphaFoldAttribution()
	{
		static const FString Text = TEXT("Strukturdaten aus der AlphaFold Protein Structure Database ")
			TEXT("(DeepMind / EMBL-EBI), lizenziert unter CC-BY-4.0.");
		return Text;
	}

	const FString& GetPdbAttribution()
	{
		static const FString Text = TEXT("Strukturdaten aus der RCSB Protein Data Bank (rcsb.org).");
		return Text;
	}
}

FVector UMolecularStructure::GetAtomPosition(int32 AtomIndex) const
{
	return AtomPositions.IsValidIndex(AtomIndex) ? FVector(AtomPositions[AtomIndex]) : FVector::ZeroVector;
}

float UMolecularStructure::GetAtomVdWRadius(int32 AtomIndex) const
{
	if (!AtomElements.IsValidIndex(AtomIndex))
	{
		return 0.f;
	}
	return MolecularForge::GetElement(AtomElements[AtomIndex]).VdWRadius;
}

FString UMolecularStructure::GetAtomElementSymbol(int32 AtomIndex) const
{
	if (!AtomElements.IsValidIndex(AtomIndex))
	{
		return FString();
	}
	return MolecularForge::GetElement(AtomElements[AtomIndex]).Symbol;
}

FLinearColor UMolecularStructure::GetAtomColor(int32 AtomIndex, EMolColorScheme Scheme, FLinearColor UniformColor) const
{
	if (!AtomPositions.IsValidIndex(AtomIndex))
	{
		return FLinearColor::White;
	}

	switch (Scheme)
	{
	case EMolColorScheme::Uniform:
		return UniformColor;

	case EMolColorScheme::Element:
		return MolecularForge::GetElement(AtomElements[AtomIndex]).GetCpkLinearColor();

	case EMolColorScheme::Chain:
	{
		const int32 ResidueIndex = AtomResidueIndices[AtomIndex];
		if (!Residues.IsValidIndex(ResidueIndex))
		{
			return FLinearColor::Gray;
		}
		return ChainColor(Residues[ResidueIndex].ChainIndex);
	}

	case EMolColorScheme::SecondaryStructure:
	{
		const int32 ResidueIndex = AtomResidueIndices[AtomIndex];
		if (!Residues.IsValidIndex(ResidueIndex))
		{
			return GSsCoil;
		}
		switch (Residues[ResidueIndex].SecondaryStructure)
		{
		case EMolSecondaryStructure::Helix:	return GSsHelix;
		case EMolSecondaryStructure::Sheet:	return GSsSheet;
		case EMolSecondaryStructure::Turn:	return GSsTurn;
		default:							return GSsCoil;
		}
	}

	case EMolColorScheme::BFactor:
	{
		const float Value = AtomBFactors[AtomIndex];

		// Bei AlphaFold ist das Feld ein Konfidenzwert, kein Temperaturfaktor. Die Skala
		// ist dort fest definiert und wird bewusst nicht auf die Datei normiert — sonst
		// saehe eine durchweg gute Vorhersage genauso aus wie eine durchweg schlechte.
		if (Meta.bBFactorIsPLDDT)
		{
			if (Value > 90.f) { return GPlddtVeryHigh; }
			if (Value > 70.f) { return GPlddtConfident; }
			if (Value > 50.f) { return GPlddtLow; }
			return GPlddtVeryLow;
		}

		// Echte B-Faktoren haben keine feste Spanne, also auf die Datei normieren:
		// blau = starr, rot = beweglich.
		const float Range = MaxBFactor - MinBFactor;
		const float Alpha = Range > KINDA_SMALL_NUMBER ? FMath::Clamp((Value - MinBFactor) / Range, 0.f, 1.f) : 0.5f;
		return FLinearColor(FLinearColor(240.f - Alpha * 240.f, 0.8f, 0.95f).HSVToLinearRGB());
	}

	case EMolColorScheme::Hydrophobicity:
	{
		const int32 ResidueIndex = AtomResidueIndices[AtomIndex];
		if (!Residues.IsValidIndex(ResidueIndex))
		{
			return FLinearColor::Gray;
		}
		const float Hydropathy = KyteDoolittle(Residues[ResidueIndex].OneLetterCode);
		const float Alpha = FMath::Clamp((Hydropathy + 4.5f) / 9.f, 0.f, 1.f);
		return FMath::Lerp(
			FLinearColor::FromSRGBColor(FColor(0x21, 0x96, 0xF3)),	// hydrophil
			FLinearColor::FromSRGBColor(FColor(0xFF, 0x98, 0x00)),	// hydrophob
			Alpha);
	}

	default:
		return FLinearColor::White;
	}
}

FBox UMolecularStructure::GetBoundsAngstrom() const
{
	if (!CachedBounds.IsValid)
	{
		return FBox(ForceInit);
	}
	return FBox(FVector(CachedBounds.Min), FVector(CachedBounds.Max));
}

FString UMolecularStructure::GetChainSequence(int32 ChainIndex) const
{
	if (!Chains.IsValidIndex(ChainIndex))
	{
		return FString();
	}

	const FMolChain& Chain = Chains[ChainIndex];
	FString Sequence;
	Sequence.Reserve(Chain.NumResidues);

	for (int32 i = Chain.FirstResidue; i < Chain.FirstResidue + Chain.NumResidues; ++i)
	{
		if (Residues.IsValidIndex(i))
		{
			Sequence.AppendChar(static_cast<TCHAR>(Residues[i].OneLetterCode));
		}
	}
	return Sequence;
}

FString UMolecularStructure::GetSummary() const
{
	const FString Label = Meta.Identifier.IsEmpty() ? TEXT("<ohne Kennung>") : Meta.Identifier;
	return FString::Printf(TEXT("%s — %d Atome, %d Residuen, %d Ketten, %d Bindungen"),
		*Label, GetNumAtoms(), GetNumResidues(), GetNumChains(), GetNumBonds());
}

void UMolecularStructure::Reset()
{
	AtomPositions.Reset();
	AtomElements.Reset();
	AtomResidueIndices.Reset();
	AtomBFactors.Reset();
	AtomOccupancies.Reset();
	AtomFlags.Reset();
	AtomNames.Reset();
	Residues.Reset();
	Chains.Reset();
	Bonds.Reset();
	Meta = FMolStructureMeta();
	MinBFactor = 0.f;
	MaxBFactor = 0.f;
	CachedBounds = FBox3f(ForceInit);
}

void UMolecularStructure::PreallocateAtoms(int32 NumAtoms)
{
	// SetNumUninitialized, weil jeder Slot gleich darauf von genau einem Thread
	// beschrieben wird. Vornullen waere ein zweiter voller Durchlauf ueber alles.
	AtomPositions.SetNumUninitialized(NumAtoms);
	AtomElements.SetNumUninitialized(NumAtoms);
	AtomResidueIndices.SetNumUninitialized(NumAtoms);
	AtomBFactors.SetNumUninitialized(NumAtoms);
	AtomOccupancies.SetNumUninitialized(NumAtoms);
	AtomFlags.SetNumUninitialized(NumAtoms);

	// FName ist nicht trivial konstruierbar, hier muss initialisiert werden.
	AtomNames.SetNum(NumAtoms);
}

void UMolecularStructure::FinalizeAfterLoad()
{
	const int32 NumAtoms = GetNumAtoms();

	CachedBounds = FBox3f(ForceInit);
	MinBFactor = 0.f;
	MaxBFactor = 0.f;

	if (NumAtoms > 0)
	{
		// Huelle und B-Faktor-Spanne in einem Durchlauf. Bei grossen Strukturen
		// chunkweise parallel mit anschliessender Reduktion der Teilergebnisse.
		struct FPartial
		{
			FVector3f Min = FVector3f(TNumericLimits<float>::Max());
			FVector3f Max = FVector3f(TNumericLimits<float>::Lowest());
			float MinB = TNumericLimits<float>::Max();
			float MaxB = TNumericLimits<float>::Lowest();
		};

		const int32 NumChunks = (NumAtoms >= GParallelThreshold)
			? FMath::Clamp(FTaskGraphInterface::Get().GetNumWorkerThreads(), 1, 64)
			: 1;
		const int32 ChunkSize = FMath::DivideAndRoundUp(NumAtoms, NumChunks);

		TArray<FPartial> Partials;
		Partials.SetNum(NumChunks);

		ParallelFor(NumChunks, [this, &Partials, ChunkSize, NumAtoms](int32 ChunkIndex)
		{
			FPartial& Out = Partials[ChunkIndex];
			const int32 Begin = ChunkIndex * ChunkSize;
			const int32 End = FMath::Min(Begin + ChunkSize, NumAtoms);

			for (int32 i = Begin; i < End; ++i)
			{
				const FVector3f& P = AtomPositions[i];
				Out.Min = FVector3f::Min(Out.Min, P);
				Out.Max = FVector3f::Max(Out.Max, P);

				const float B = AtomBFactors[i];
				Out.MinB = FMath::Min(Out.MinB, B);
				Out.MaxB = FMath::Max(Out.MaxB, B);
			}
		}, NumChunks == 1 ? EParallelForFlags::ForceSingleThread : EParallelForFlags::None);

		FPartial Total;
		for (const FPartial& P : Partials)
		{
			Total.Min = FVector3f::Min(Total.Min, P.Min);
			Total.Max = FVector3f::Max(Total.Max, P.Max);
			Total.MinB = FMath::Min(Total.MinB, P.MinB);
			Total.MaxB = FMath::Max(Total.MaxB, P.MaxB);
		}

		CachedBounds = FBox3f(Total.Min, Total.Max);
		MinBFactor = Total.MinB;
		MaxBFactor = Total.MaxB;
	}

	// Einbuchstaben-Codes nachtragen und die Kettenart aus den Residuenklassen ableiten.
	for (FMolResidue& Residue : Residues)
	{
		Residue.OneLetterCode = MolecularForge::ResidueOneLetterCode(Residue.Name);
	}

	for (int32 ChainIndex = 0; ChainIndex < Chains.Num(); ++ChainIndex)
	{
		FMolChain& Chain = Chains[ChainIndex];

		int32 NumAmino = 0;
		int32 NumDeoxy = 0;
		int32 NumRibo = 0;
		int32 NumWater = 0;
		int32 NumOther = 0;

		for (int32 i = Chain.FirstResidue; i < Chain.FirstResidue + Chain.NumResidues; ++i)
		{
			if (!Residues.IsValidIndex(i))
			{
				continue;
			}
			switch (MolecularForge::ClassifyResidue(Residues[i].Name))
			{
			case EMolResidueClass::AminoAcid:		++NumAmino; break;
			case EMolResidueClass::DeoxyNucleotide:	++NumDeoxy; break;
			case EMolResidueClass::Nucleotide:		++NumRibo;  break;
			case EMolResidueClass::Water:			++NumWater; break;
			default:								++NumOther; break;
			}
		}

		// Ein einzelnes Ion oder ein gebundener Ligand in einer Proteinkette soll die
		// Kette nicht umklassifizieren, deshalb entscheidet die Mehrheit der Polymerreste.
		if (NumAmino > 0 && NumAmino >= NumDeoxy && NumAmino >= NumRibo)
		{
			Chain.Kind = EMolChainKind::Protein;
		}
		else if (NumDeoxy > 0 && NumDeoxy >= NumRibo)
		{
			Chain.Kind = EMolChainKind::Dna;
		}
		else if (NumRibo > 0)
		{
			Chain.Kind = EMolChainKind::Rna;
		}
		else if (NumWater > 0 && NumOther == 0)
		{
			Chain.Kind = EMolChainKind::Water;
		}
		else if (NumOther > 0)
		{
			Chain.Kind = EMolChainKind::Ligand;
		}
		else
		{
			Chain.Kind = EMolChainKind::Unknown;
		}
	}
}

void UMolecularStructure::CenterOnOrigin()
{
	const int32 NumAtoms = GetNumAtoms();
	if (NumAtoms == 0)
	{
		return;
	}

	if (!CachedBounds.IsValid)
	{
		FinalizeAfterLoad();
	}

	const FVector3f Center = CachedBounds.GetCenter();
	if (Center.IsNearlyZero())
	{
		return;
	}

	ParallelFor(NumAtoms, [this, Center](int32 Index)
	{
		AtomPositions[Index] -= Center;
	}, NumAtoms < GParallelThreshold ? EParallelForFlags::ForceSingleThread : EParallelForFlags::None);

	CachedBounds = CachedBounds.ShiftBy(-Center);
}
