// Copyright Silvan Teufel. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MolecularForgeTypes.h"

class UMolecularStructure;

/** Ergebnis eines Ladevorgangs. Auch im Fehlerfall aussagekraeftig, damit das UI etwas zeigen kann. */
struct MOLECULARFORGERUNTIME_API FMolParseResult
{
	bool bSuccess = false;

	/** Menschenlesbarer Fehlertext, leer bei Erfolg. */
	FString Error;

	/** Atome, die in die Struktur uebernommen wurden. */
	int32 NumAtomsParsed = 0;

	/** Atome, die durch die Ladeoptionen herausgefallen sind (Wasser, Wasserstoff, AltLoc). */
	int32 NumAtomsDiscarded = 0;

	/** Zeilen, die wie ein Atom aussahen, aber nicht lesbar waren. */
	int32 NumMalformedLines = 0;

	double ParseSeconds = 0.0;
};

namespace MolecularForge
{
	/**
	 * Liest eine Datei im PDB-Format.
	 *
	 * Der Ablauf ist dreistufig, und die Aufteilung ist der Grund, warum das schnell ist:
	 *   1. Sequenzieller Klassifizierungslauf ueber alle Zeilen. Der schaut nur auf die
	 *      ersten sechs Zeichen, sammelt Kopfdaten und merkt sich die Zeilenindizes der
	 *      ATOM-/HETATM-Records. Billig, weil keine Zahl geparst wird.
	 *   2. `ParallelFor` ueber genau diese Indizes. Hier liegt die eigentliche Arbeit —
	 *      drei Fliesskommazahlen pro Zeile, bei einem Ribosom sind das ueber 400.000 Zeilen.
	 *      Jeder Thread schreibt in seinen eigenen, vorab reservierten Slot, es gibt keine
	 *      geteilten Schreibziele und damit keine Sperre.
	 *   3. Sequenzieller Verdichtungslauf: Ladeoptionen anwenden, Residuen und Ketten
	 *      gruppieren, Sekundaerstruktur eintragen. Das ist von Natur aus seriell, weil
	 *      Gruppierung von der Reihenfolge abhaengt — kostet aber nur noch Integer-Arbeit.
	 *
	 * Bei Ensembles mit mehreren Modellen (NMR) wird derzeit Modell 1 geladen; die
	 * Gesamtzahl steht danach in Meta.NumModelsInFile.
	 */
	MOLECULARFORGERUNTIME_API FMolParseResult ParsePdb(
		FStringView PdbText,
		const FMolLoadOptions& Options,
		UMolecularStructure& OutStructure);

	/** Bequemer Weg ueber eine Datei auf der Platte. */
	MOLECULARFORGERUNTIME_API FMolParseResult ParsePdbFile(
		const FString& FilePath,
		const FMolLoadOptions& Options,
		UMolecularStructure& OutStructure);
}
