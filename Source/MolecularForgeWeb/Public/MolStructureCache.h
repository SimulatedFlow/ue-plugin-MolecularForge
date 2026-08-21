// Copyright 2026 Simulated Flow All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MolStructureIdentifiers.h"

namespace MolecularForge
{
	/**
	 * Lokaler Zwischenspeicher fuer heruntergeladene Strukturdateien.
	 *
	 * Der Cache ist keine Bequemlichkeit, sondern Teil des anstaendigen Umgangs mit den
	 * Quellen. RCSB und EMBL-EBI stellen ihre Daten kostenlos und ohne Schluessel bereit;
	 * eine Anwendung, die bei jedem Levelstart dieselbe Datei erneut zieht, missbraucht das.
	 * Strukturen sind nach ihrer Freigabe ausserdem unveraenderlich, ein Wiederholen des
	 * Downloads brächte also ohnehin nichts Neues.
	 *
	 * Ablage: `<Projekt>/Saved/MolecularForge/StructureCache/<quelle>-<kennung>.<endung>`.
	 * Der Dateiname wird ausschliesslich aus geprueften Kennungen gebaut — es gibt keinen
	 * Pfad, den ein Aufrufer frei bestimmen koennte.
	 */
	MOLECULARFORGEWEB_API FString GetCacheDirectory();

	/** Vollstaendiger Cache-Pfad. Leer, wenn die Kennung ungueltig ist. */
	MOLECULARFORGEWEB_API FString GetCacheFilePath(EMolFetchSource Source, const FString& Identifier);

	/**
	 * True, wenn eine brauchbare Datei im Cache liegt.
	 *
	 * @param MaxAgeDays	0 heisst "laeuft nie ab" — der Normalfall, weil freigegebene
	 *						Strukturen sich nicht mehr aendern. Groesser als 0 ist fuer
	 *						AlphaFold sinnvoll, wo neue Modellversionen erscheinen.
	 */
	MOLECULARFORGEWEB_API bool HasCachedStructure(EMolFetchSource Source, const FString& Identifier, int32 MaxAgeDays = 0);

	/** Liest die zwischengespeicherte Datei. Gibt false zurueck, wenn sie fehlt oder zu alt ist. */
	MOLECULARFORGEWEB_API bool ReadCachedStructure(EMolFetchSource Source, const FString& Identifier, FString& OutContent, int32 MaxAgeDays = 0);

	/** Schreibt eine heruntergeladene Datei in den Cache. */
	MOLECULARFORGEWEB_API bool WriteCachedStructure(EMolFetchSource Source, const FString& Identifier, const FString& Content);

	/** Entfernt einen einzelnen Eintrag. True, wenn danach nichts mehr da ist. */
	MOLECULARFORGEWEB_API bool RemoveCachedStructure(EMolFetchSource Source, const FString& Identifier);

	/** Leert den gesamten Cache. Gibt die Zahl der geloeschten Dateien zurueck. */
	MOLECULARFORGEWEB_API int32 ClearStructureCache();

	/** Belegter Platz in Byte. Fuer eine Anzeige im Editor. */
	MOLECULARFORGEWEB_API int64 GetStructureCacheSize();
}
