// Copyright Simulated Flow. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UMolecularStructure;

/** Ergebnis einer Auswahl. Auch im Fehlerfall aussagekraeftig. */
struct MOLECULARFORGERUNTIME_API FMolSelectionResult
{
	bool bSuccess = false;

	/** Menschenlesbarer Fehlertext, leer bei Erfolg. */
	FString Error;

	/** Zeichenposition im Ausdruck, an der es klemmte. INDEX_NONE wenn nicht zuordenbar. */
	int32 ErrorPosition = INDEX_NONE;

	/** Ein Bit je Atom der Struktur. */
	TBitArray<> Mask;

	int32 NumSelected = 0;
};

namespace MolecularForge
{
	/**
	 * Waehlt Atome ueber einen Ausdruck aus.
	 *
	 * Die Syntax lehnt sich an PyMOL an, und zwar mit Absicht: wer aus der Strukturbiologie
	 * kommt, hat sie im Kopf. Eine eigene, vielleicht schoenere Schreibweise waere fuer
	 * genau die Zielgruppe eine zusaetzliche Huerde.
	 *
	 * Klassen:
	 *   `all`, `none`, `protein`, `nucleic`, `dna`, `rna`, `water`, `hetero`, `ligand`,
	 *   `backbone`, `sidechain`
	 *
	 * Mit Argument:
	 *   `chain A+B`          Ketten
	 *   `resi 1-50+60`       Residuennummern, Bereiche mit Bindestrich
	 *   `resn ALA+GLY`       Residuennamen
	 *   `name CA+CB`         Atomnamen
	 *   `element C+N`        Elementsymbole
	 *   `ss H+S`             Sekundaerstruktur (H Helix, S Faltblatt, T Turn, C Coil)
	 *   `b > 50`             B-Faktor bzw. pLDDT, auch `<`
	 *   `within 5 of <Ausdruck>`   alles im Umkreis
	 *
	 * Verknuepfung: `and`, `or`, `not` sowie Klammern. Beispiel:
	 *   `(chain A and resi 10-40) or (within 4 of resn ATP)`
	 *
	 * Gross- und Kleinschreibung spielt keine Rolle.
	 */
	MOLECULARFORGERUNTIME_API FMolSelectionResult SelectAtoms(
		const UMolecularStructure& Structure,
		const FString& Expression);

	/** Wandelt eine Maske in eine Indexliste. */
	MOLECULARFORGERUNTIME_API void MaskToIndices(const TBitArray<>& Mask, TArray<int32>& OutIndices);
}
