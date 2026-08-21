// Copyright 2026 Simulated Flow All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MolecularForgeTypes.h"
#include "MolStructureIdentifiers.h"
#include "MolStructureFetcher.generated.h"

class UMolecularStructure;

/** Was geholt werden soll und wie. */
USTRUCT(BlueprintType)
struct MOLECULARFORGEWEB_API FMolFetchOptions
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MolecularForge")
	EMolFetchSource Source = EMolFetchSource::RcsbPdb;

	/** PDB-Code (z.B. "1CRN") oder UniProt-Accession (z.B. "P69905"), je nach Quelle. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MolecularForge")
	FString Identifier;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MolecularForge")
	FMolLoadOptions LoadOptions;

	/** Aus dem lokalen Zwischenspeicher bedienen, wenn dort etwas liegt. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MolecularForge")
	bool bUseCache = true;

	/**
	 * Hoechstalter des Cache-Eintrags in Tagen. 0 heisst "laeuft nie ab".
	 * Fuer RCSB ist 0 richtig — freigegebene Strukturen aendern sich nicht mehr.
	 * Fuer AlphaFold ist ein Wert sinnvoll, weil dort neue Modellversionen erscheinen.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MolecularForge", meta = (ClampMin = "0"))
	int32 CacheMaxAgeDays = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MolecularForge", meta = (ClampMin = "1.0"))
	float TimeoutSeconds = 30.f;
};

/** Wie der Abruf ausgegangen ist. Auch im Fehlerfall aussagekraeftig. */
USTRUCT(BlueprintType)
struct MOLECULARFORGEWEB_API FMolFetchResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "MolecularForge")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadOnly, Category = "MolecularForge")
	FString Error;

	/** True, wenn nichts heruntergeladen werden musste. */
	UPROPERTY(BlueprintReadOnly, Category = "MolecularForge")
	bool bFromCache = false;

	/** Tatsaechlich abgerufene Adresse. Bei AlphaFold die aufgeloeste Datei-URL, nicht die API-Adresse. */
	UPROPERTY(BlueprintReadOnly, Category = "MolecularForge")
	FString ResolvedUrl;

	UPROPERTY(BlueprintReadOnly, Category = "MolecularForge")
	int32 HttpStatusCode = 0;

	UPROPERTY(BlueprintReadOnly, Category = "MolecularForge")
	float ElapsedSeconds = 0.f;
};

/** Wird auf dem Spielthread aufgerufen. Bei Misserfolg ist die Struktur nullptr. */
DECLARE_DELEGATE_TwoParams(FOnMolStructureFetched, UMolecularStructure*, const FMolFetchResult&);

namespace MolecularForge
{
	/**
	 * Holt eine Struktur aus dem Netz oder aus dem lokalen Zwischenspeicher.
	 *
	 * Muss vom Spielthread aus aufgerufen werden; der Abschluss-Delegat wird ebenfalls
	 * dort ausgefuehrt. Dazwischen laeuft nichts auf dem Spielthread: der Download geht
	 * ueber das HTTP-Modul, und das Zerlegen der Datei — bei einem grossen Komplex
	 * dreistellige Millisekunden — wird auf einen Hintergrundtask ausgelagert. Ein
	 * Levelstart, der ein Ribosom laedt, ruckelt dadurch nicht.
	 *
	 * AlphaFold braucht zwei Schritte: erst die API nach dem Eintrag fragen, dann die
	 * Datei holen, auf die sie verweist. Die Datei-URL enthaelt eine Versionsnummer, die
	 * sich mit jeder neuen Modellgeneration aendert — sie zu raten waere nur so lange
	 * richtig, bis EMBL-EBI das naechste Mal nachliefert.
	 *
	 * @param Outer	Besitzer der erzeugten Struktur. Darf nullptr sein, dann haengt sie
	 *				am transienten Paket.
	 */
	MOLECULARFORGEWEB_API void FetchStructure(
		UObject* Outer,
		const FMolFetchOptions& Options,
		FOnMolStructureFetched OnComplete);
}
