// Copyright Silvan Teufel. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MolecularForgeTypes.h"
#include "MolPdbParser.h"

class UMolecularStructure;

namespace MolecularForge
{
	/**
	 * Liest eine Datei im mmCIF-Format (PDBx/mmCIF).
	 *
	 * mmCIF hat PDB als Archivformat abgeloest, und zwar aus einem Grund, der hier zaehlt:
	 * PDB kann Strukturen mit mehr als 99.999 Atomen oder mehr als 62 Ketten gar nicht
	 * darstellen — die Spalten sind zu schmal. Genau die grossen Komplexe, die als
	 * Schaustueck taugen (Ribosomen, Kapside, Nukleosomen), gibt es deshalb nur als mmCIF.
	 *
	 * Der Aufbau folgt derselben Dreiteilung wie der PDB-Leser, nur ist die erste Stufe
	 * hier ein Tokenizer statt eines Spaltenlesers:
	 *   1. Sequenzieller Lauf durch den Token-Strom. Kopfdaten und kleine Kategorien werden
	 *      direkt mitgenommen; von der grossen `atom_site`-Schleife merkt sich der Lauf nur
	 *      die Zeilenanfaenge als Offsets. Es wird dabei keine Zahl konvertiert.
	 *   2. `ParallelFor` ueber die Zeilenanfaenge. Jeder Thread setzt einen eigenen Tokenizer
	 *      auf seinen Offset und liest genau eine Zeile — kein geteilter Zustand.
	 *   3. Derselbe Verdichtungslauf wie beim PDB-Leser (gemeinsamer Assembler).
	 *
	 * Warum Offsets statt fertiger Token: eine grosse Struktur hat 21 Spalten mal 400.000
	 * Zeilen. Alle Token als Views zu behalten kostet ueber hundert Megabyte; die
	 * Zeilenanfaenge kosten vier Byte pro Zeile, und das erneute Zerlegen einer einzelnen
	 * Zeile im Thread ist billiger als der Speicher, den man sonst bewegt.
	 *
	 * Unterstuetzt werden Anfuehrungszeichen, mehrzeilige Semikolon-Textfelder, Kommentare
	 * und die Nullwerte `.` und `?`. Bei Ensembles wird Modell 1 geladen.
	 */
	MOLECULARFORGERUNTIME_API FMolParseResult ParseCif(
		FStringView CifText,
		const FMolLoadOptions& Options,
		UMolecularStructure& OutStructure);

	/** Bequemer Weg ueber eine Datei auf der Platte. */
	MOLECULARFORGERUNTIME_API FMolParseResult ParseCifFile(
		const FString& FilePath,
		const FMolLoadOptions& Options,
		UMolecularStructure& OutStructure);
}
