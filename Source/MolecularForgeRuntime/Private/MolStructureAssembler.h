// Copyright 2026 Silvan Teufel All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MolecularForgeTypes.h"

class UMolecularStructure;

/**
 * Zwischenform eines Atoms, wie es aus einer Zeile bzw. Datenzeile faellt.
 * Bewusst flach und ohne Zeiger, damit `ParallelFor` ohne weitere Ruecksicht
 * hineinschreiben kann. Wird nach dem Verdichten wieder verworfen.
 */
struct FMolRawAtom
{
	FVector3f Position = FVector3f::ZeroVector;
	float Occupancy = 1.f;
	float BFactor = 0.f;
	FName Name;
	FName ResidueName;
	int32 ResidueSeq = 0;
	uint8 Element = 0;
	FName ChainId;
	uint8 InsertionCode = ' ';
	uint8 AltLoc = ' ';
	bool bHetatm = false;
	bool bValid = false;
};

/** Ein Sekundaerstruktur-Bereich, wie ihn HELIX/SHEET bzw. struct_conf/struct_sheet_range liefern. */
struct FMolSecondaryRange
{
	FName ChainId;
	int32 FirstSeq = 0;
	int32 LastSeq = 0;
	EMolSecondaryStructure Kind = EMolSecondaryStructure::Coil;
};

/** Ergebniszahlen des Verdichtungslaufs. */
struct FMolAssembleStats
{
	int32 NumAtomsKept = 0;
	int32 NumAtomsDiscarded = 0;
};

namespace MolecularForge
{
	/**
	 * Verdichtet Rohatome zu einer fertigen Struktur: Ladeoptionen anwenden, Residuen und
	 * Ketten gruppieren, Atomflags setzen, Sekundaerstruktur eintragen.
	 *
	 * Dieser Schritt ist von Natur aus seriell — Gruppierung haengt an der Reihenfolge, in
	 * der die Atome in der Datei stehen. Das ist kein Problem, weil hier nur noch
	 * Integer-Arbeit anfaellt; die teure Zahlenkonvertierung ist vorher schon parallel
	 * erledigt worden.
	 *
	 * Er ist absichtlich formatunabhaengig: PDB und mmCIF unterscheiden sich darin, wie ein
	 * Atom aus dem Text faellt, aber nicht darin, was danach damit zu geschehen hat. Diese
	 * Trennung ist auch der Grund, warum ein dritter Leser (etwa MMTF oder BinaryCIF) spaeter
	 * nur noch Rohatome liefern muss.
	 *
	 * @param RawAtoms			Rohatome in Dateireihenfolge.
	 * @param ChainBreakBefore	Optional, gleiche Laenge wie RawAtoms. Ein Wert != 0 erzwingt
	 *							vor diesem Atom einen Kettenbruch (PDB-TER-Record). Darf leer sein.
	 * @param SecondaryRanges	Optional, wird nach der Gruppierung auf die Residuen angewandt.
	 */
	void AssembleStructure(
		const TArray<FMolRawAtom>& RawAtoms,
		const TArray<uint8>& ChainBreakBefore,
		const TArray<FMolSecondaryRange>& SecondaryRanges,
		const FMolLoadOptions& Options,
		UMolecularStructure& OutStructure,
		FMolAssembleStats& OutStats);
}
