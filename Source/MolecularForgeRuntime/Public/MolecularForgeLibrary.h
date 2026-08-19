// Copyright Simulated Flow. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MolecularForgeTypes.h"
#include "MolecularForgeLibrary.generated.h"

class UMolecularStructure;

/** Einstiegspunkte fuer Blueprint. Die eigentliche Arbeit steckt in den Parsern. */
UCLASS()
class MOLECULARFORGERUNTIME_API UMolecularForgeLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Laedt eine Strukturdatei von der Platte. Das Format (PDB oder mmCIF) wird aus
	 * Dateiendung und Inhalt selbst bestimmt.
	 *
	 * Laeuft synchron im aufrufenden Thread. Fuer kleine bis mittlere Strukturen
	 * (bis etwa 50.000 Atome) ist das im Spielthread unbedenklich; darueber gehoert
	 * der Aufruf in einen Hintergrundtask, weil das Parsen dann zweistellige
	 * Millisekunden kostet und ein Frame nicht so lange stehen sollte.
	 *
	 * @param OutErrorMessage	Leer bei Erfolg, sonst der Grund.
	 * @return					Die geladene Struktur, oder nullptr bei Fehler.
	 */
	UFUNCTION(BlueprintCallable, Category = "MolecularForge", meta = (AdvancedDisplay = "Options"))
	static UMolecularStructure* LoadStructureFromFile(
		UObject* Outer,
		const FString& FilePath,
		FMolLoadOptions Options,
		FString& OutErrorMessage);

	/** Wie LoadStructureFromFile, aber der Dateiinhalt liegt bereits als Text vor. */
	UFUNCTION(BlueprintCallable, Category = "MolecularForge", meta = (AdvancedDisplay = "Options"))
	static UMolecularStructure* LoadStructureFromText(
		UObject* Outer,
		const FString& Content,
		FMolLoadOptions Options,
		FString& OutErrorMessage);

	/** Attributionstext einer Struktur, oder leer wenn die Quelle keine verlangt. */
	UFUNCTION(BlueprintPure, Category = "MolecularForge")
	static FString GetRequiredAttribution(const UMolecularStructure* Structure);
};
