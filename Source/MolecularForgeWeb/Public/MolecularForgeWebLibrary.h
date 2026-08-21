// Copyright 2026 Simulated Flow All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MolStructureIdentifiers.h"
#include "MolecularForgeWebLibrary.generated.h"

/** Zugaenge zu Kennungspruefung und Zwischenspeicher fuer Blueprint. */
UCLASS()
class MOLECULARFORGEWEB_API UMolecularForgeWebLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Prueft, ob die Kennung zur Quelle passt — vor dem Abruf, damit die Eingabemaske
	 * gleich Rueckmeldung geben kann statt erst nach einem gescheiterten Download.
	 */
	UFUNCTION(BlueprintPure, Category = "MolecularForge|Web")
	static bool IsValidStructureIdentifier(EMolFetchSource Source, const FString& Identifier);

	/** Die Adresse, die fuer diese Anfrage aufgerufen wuerde. Leer bei ungueltiger Kennung. */
	UFUNCTION(BlueprintPure, Category = "MolecularForge|Web")
	static FString GetStructureRequestUrl(EMolFetchSource Source, const FString& Identifier);

	/** True, wenn diese Struktur bereits lokal vorliegt und kein Download noetig waere. */
	UFUNCTION(BlueprintPure, Category = "MolecularForge|Web")
	static bool IsStructureCached(EMolFetchSource Source, const FString& Identifier);

	UFUNCTION(BlueprintPure, Category = "MolecularForge|Web")
	static FString GetStructureCacheDirectory();

	/** Belegter Platz des Zwischenspeichers in Megabyte. */
	UFUNCTION(BlueprintPure, Category = "MolecularForge|Web")
	static float GetStructureCacheSizeMB();

	/** Leert den Zwischenspeicher und gibt die Zahl der entfernten Dateien zurueck. */
	UFUNCTION(BlueprintCallable, Category = "MolecularForge|Web")
	static int32 ClearStructureCache();
};
