// Copyright 2026 Simulated Flow All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MolStructureIdentifiers.generated.h"

/** Woher eine Struktur geholt werden soll. */
UENUM(BlueprintType)
enum class EMolFetchSource : uint8
{
	/** RCSB Protein Data Bank — experimentell bestimmte Strukturen, Kennung ist der PDB-Code. */
	RcsbPdb			UMETA(DisplayName = "RCSB PDB"),
	/** AlphaFold-Datenbank — vorhergesagte Strukturen, Kennung ist die UniProt-Accession. */
	AlphaFoldDb		UMETA(DisplayName = "AlphaFold DB")
};

namespace MolecularForge
{
	/**
	 * Prueft einen PDB-Code.
	 *
	 * Gueltig sind die klassische vierstellige Form (Ziffer 1-9, dann drei alphanumerische
	 * Zeichen, z.B. "1CRN") und die erweiterte Form "pdb_" plus acht alphanumerische Zeichen,
	 * die eingefuehrt wurde, weil der vierstellige Raum absehbar voll ist.
	 *
	 * Die Pruefung ist nicht nur Bequemlichkeit. Die Kennung geht ungefiltert in eine URL
	 * und in einen Cache-Dateinamen; ohne sie liesse sich mit "../" aus dem Cache-Verzeichnis
	 * herausschreiben oder mit einem eingebauten "?" die Ziel-URL umbiegen. Deshalb wird
	 * hier gegen eine Positivliste geprueft und nicht nachtraeglich etwas herausgefiltert.
	 */
	MOLECULARFORGEWEB_API bool IsValidPdbIdentifier(const FString& Identifier);

	/**
	 * Prueft eine UniProt-Accession gegen das offizielle Muster
	 * (z.B. "P69905", "A0A023GPI8"). Sechs oder zehn Zeichen, feste Stellenbelegung.
	 */
	MOLECULARFORGEWEB_API bool IsValidUniProtAccession(const FString& Accession);

	/** Prueft die Kennung passend zur Quelle. */
	MOLECULARFORGEWEB_API bool IsValidIdentifier(EMolFetchSource Source, const FString& Identifier);

	/**
	 * Bringt eine Kennung in die kanonische Schreibweise: PDB-Codes gross, UniProt-
	 * Accessions gross. Ohne das landen "1crn" und "1CRN" als zwei Eintraege im Cache.
	 */
	MOLECULARFORGEWEB_API FString NormalizeIdentifier(EMolFetchSource Source, const FString& Identifier);

	/**
	 * Download-URL fuer die Strukturdatei.
	 *
	 * Bei RCSB wird mmCIF geholt und nicht PDB: grosse Komplexe gibt es im PDB-Format gar
	 * nicht mehr, weil dessen Spalten fuer mehr als 99.999 Atome zu schmal sind.
	 * Bei AlphaFold liefert diese Funktion die API-Adresse, denn die eigentliche Datei-URL
	 * enthaelt eine Versionsnummer, die sich aendert und deshalb nicht geraten werden darf.
	 *
	 * Gibt bei ungueltiger Kennung eine leere Zeichenkette zurueck.
	 */
	MOLECULARFORGEWEB_API FString BuildRequestUrl(EMolFetchSource Source, const FString& Identifier);

	/** Dateiendung, unter der die Antwort dieser Quelle im Cache landet. */
	MOLECULARFORGEWEB_API FString GetCacheFileExtension(EMolFetchSource Source);

	/** Kurzname der Quelle fuer Cache-Dateinamen und Logausgaben. */
	MOLECULARFORGEWEB_API FString GetSourceSlug(EMolFetchSource Source);
}
