// Copyright 2026 Silvan Teufel All Rights Reserved.

#include "MolecularForgeLibrary.h"
#include "MolecularStructure.h"
#include "MolStructureIO.h"
#include "MolSelection.h"
#include "MolMeasurement.h"
// GetTransientPackage() liefert UPackage*. Ohne diese Kopfdatei ist UPackage nur
// vorwaertsdeklariert, und der bedingte Ausdruck darunter findet keinen gemeinsamen
// Typ mit UObject* — der Fehler liest sich dann irrefuehrend als Konvertierungsproblem.
#include "UObject/Package.h"

namespace
{
	/**
	 * Wandelt eine Indexliste in eine Bitmaske.
	 *
	 * Eine leere Liste bedeutet "alle Atome" und nicht "keines". Das ist bewusst so:
	 * in Blueprint bleibt ein Eingang gern unbelegt, und wer den Schwerpunkt eines
	 * ganzen Molekuels will, soll dafuer nicht erst alle Indizes auflisten muessen.
	 */
	TBitArray<> IndicesToMask(const UMolecularStructure& Structure, const TArray<int32>& Indices)
	{
		if (Indices.IsEmpty())
		{
			return TBitArray<>(true, Structure.GetNumAtoms());
		}

		TBitArray<> Mask(false, Structure.GetNumAtoms());
		for (int32 Index : Indices)
		{
			if (Index >= 0 && Index < Mask.Num())
			{
				Mask[Index] = true;
			}
		}
		return Mask;
	}
	UMolecularStructure* FinishLoad(UMolecularStructure* Structure,
		const FMolParseResult& Result, FString& OutErrorMessage)
	{
		if (!Result.bSuccess)
		{
			OutErrorMessage = Result.Error;
			UE_LOG(LogMolecularForge, Warning, TEXT("Laden fehlgeschlagen: %s"), *Result.Error);
			return nullptr;
		}

		OutErrorMessage.Empty();
		return Structure;
	}
}

UMolecularStructure* UMolecularForgeLibrary::LoadStructureFromFile(
	UObject* Outer, const FString& FilePath, FMolLoadOptions Options, FString& OutErrorMessage)
{
	UObject* Owner = Outer ? Outer : GetTransientPackage();
	UMolecularStructure* Structure = NewObject<UMolecularStructure>(Owner);

	const FMolParseResult Result = MolecularForge::ParseStructureFile(FilePath, Options, *Structure);
	return FinishLoad(Structure, Result, OutErrorMessage);
}

UMolecularStructure* UMolecularForgeLibrary::LoadStructureFromText(
	UObject* Outer, const FString& Content, FMolLoadOptions Options, FString& OutErrorMessage)
{
	UObject* Owner = Outer ? Outer : GetTransientPackage();
	UMolecularStructure* Structure = NewObject<UMolecularStructure>(Owner);

	const FMolParseResult Result = MolecularForge::ParseStructureText(Content, Options, *Structure);
	return FinishLoad(Structure, Result, OutErrorMessage);
}

FString UMolecularForgeLibrary::GetRequiredAttribution(const UMolecularStructure* Structure)
{
	return Structure ? Structure->Meta.Attribution : FString();
}

bool UMolecularForgeLibrary::SelectAtoms(const UMolecularStructure* Structure,
	const FString& Expression, TArray<int32>& OutAtomIndices, FString& OutError)
{
	OutAtomIndices.Reset();

	if (!Structure)
	{
		OutError = TEXT("Es ist keine Struktur angegeben.");
		return false;
	}

	const FMolSelectionResult Result = MolecularForge::SelectAtoms(*Structure, Expression);
	if (!Result.bSuccess)
	{
		OutError = Result.Error;
		return false;
	}

	MolecularForge::MaskToIndices(Result.Mask, OutAtomIndices);
	OutError.Empty();
	return true;
}

float UMolecularForgeLibrary::MeasureDistance(const UMolecularStructure* Structure, int32 AtomA, int32 AtomB)
{
	return Structure ? MolecularForge::MeasureDistance(*Structure, AtomA, AtomB) : -1.f;
}

float UMolecularForgeLibrary::MeasureAngle(const UMolecularStructure* Structure,
	int32 AtomA, int32 AtomB, int32 AtomC)
{
	return Structure ? MolecularForge::MeasureAngle(*Structure, AtomA, AtomB, AtomC) : -1.f;
}

float UMolecularForgeLibrary::MeasureDihedral(const UMolecularStructure* Structure,
	int32 AtomA, int32 AtomB, int32 AtomC, int32 AtomD)
{
	return Structure ? MolecularForge::MeasureDihedral(*Structure, AtomA, AtomB, AtomC, AtomD) : 0.f;
}

FVector UMolecularForgeLibrary::GetSelectionCentroid(
	const UMolecularStructure* Structure, const TArray<int32>& AtomIndices)
{
	if (!Structure)
	{
		return FVector::ZeroVector;
	}
	return MolecularForge::ComputeCentroid(*Structure, IndicesToMask(*Structure, AtomIndices));
}

FVector UMolecularForgeLibrary::GetSelectionCenterOfMass(
	const UMolecularStructure* Structure, const TArray<int32>& AtomIndices)
{
	if (!Structure)
	{
		return FVector::ZeroVector;
	}
	return MolecularForge::ComputeCenterOfMass(*Structure, IndicesToMask(*Structure, AtomIndices));
}

float UMolecularForgeLibrary::GetSelectionRadiusOfGyration(
	const UMolecularStructure* Structure, const TArray<int32>& AtomIndices)
{
	if (!Structure)
	{
		return 0.f;
	}
	return MolecularForge::ComputeRadiusOfGyration(*Structure, IndicesToMask(*Structure, AtomIndices));
}

float UMolecularForgeLibrary::GetSelectionMass(
	const UMolecularStructure* Structure, const TArray<int32>& AtomIndices)
{
	if (!Structure)
	{
		return 0.f;
	}
	return MolecularForge::ComputeTotalMass(*Structure, IndicesToMask(*Structure, AtomIndices));
}

FBox UMolecularForgeLibrary::GetSelectionBounds(
	const UMolecularStructure* Structure, const TArray<int32>& AtomIndices)
{
	if (!Structure)
	{
		return FBox(ForceInit);
	}
	return MolecularForge::ComputeSelectionBounds(*Structure, IndicesToMask(*Structure, AtomIndices));
}
