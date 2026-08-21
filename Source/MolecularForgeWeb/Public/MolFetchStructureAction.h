// Copyright 2026 Simulated Flow All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "MolStructureFetcher.h"
#include "MolFetchStructureAction.generated.h"

class UMolecularStructure;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMolFetchStructurePin,
	UMolecularStructure*, Structure, FMolFetchResult, Result);

/**
 * Latenter Blueprint-Knoten zum Holen einer Struktur.
 *
 * Der Knoten haelt sich selbst am Leben, solange die Anfrage laeuft, und gibt sich erst
 * frei, wenn einer der beiden Ausgaenge gefeuert hat. Ohne das koennte die
 * Speicherbereinigung mitten im Download zuschlagen.
 */
UCLASS()
class MOLECULARFORGEWEB_API UMolFetchStructureAction : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	/** Die Struktur wurde geladen und ist gebrauchsfertig. */
	UPROPERTY(BlueprintAssignable)
	FMolFetchStructurePin OnSuccess;

	/** Der Abruf ist gescheitert. `Result.Error` sagt warum, in einem Satz fuer die Anzeige. */
	UPROPERTY(BlueprintAssignable)
	FMolFetchStructurePin OnFailure;

	/**
	 * Holt eine Struktur aus dem Netz oder aus dem lokalen Zwischenspeicher.
	 *
	 * @param Source		RCSB fuer experimentelle Strukturen, AlphaFold fuer Vorhersagen.
	 * @param Identifier	PDB-Code (z.B. "1CRN") oder UniProt-Accession (z.B. "P69905").
	 */
	UFUNCTION(BlueprintCallable, Category = "MolecularForge",
		meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject",
			DisplayName = "Struktur holen", AdvancedDisplay = "LoadOptions,bUseCache"))
	static UMolFetchStructureAction* FetchStructure(
		UObject* WorldContextObject,
		EMolFetchSource Source,
		const FString& Identifier,
		FMolLoadOptions LoadOptions,
		bool bUseCache = true);

	virtual void Activate() override;

private:
	void HandleCompleted(UMolecularStructure* Structure, const FMolFetchResult& Result);

	UPROPERTY()
	TObjectPtr<UObject> ContextObject;

	FMolFetchOptions Options;
};
