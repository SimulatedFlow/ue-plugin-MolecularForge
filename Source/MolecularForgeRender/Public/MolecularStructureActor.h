// Copyright Simulated Flow. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MolecularForgeTypes.h"
#include "MolecularStructureActor.generated.h"

class UMolecularAtomsComponent;
class UMolecularStructure;

/**
 * Fertiger Akteur zum Platzieren einer Struktur in der Welt.
 *
 * Gedacht als kuerzester Weg vom Dateipfad zum Bild: Actor ins Level ziehen, Pfad
 * eintragen, "Jetzt laden" druecken. Wer mehr Kontrolle braucht, nimmt die
 * UMolecularAtomsComponent direkt.
 */
UCLASS(Blueprintable, ClassGroup = (MolecularForge))
class MOLECULARFORGERENDER_API AMolecularStructureActor : public AActor
{
	GENERATED_BODY()

public:
	AMolecularStructureActor();

	/**
	 * Pfad zu einer Strukturdatei (PDB oder mmCIF), absolut oder relativ zum
	 * Projektverzeichnis. Das Format wird selbst erkannt.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MolecularForge")
	FString StructureFilePath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MolecularForge")
	FMolLoadOptions LoadOptions;

	/** Beim Spielstart automatisch laden. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MolecularForge")
	bool bLoadOnBeginPlay = true;

	/** Laedt die Datei aus StructureFilePath und uebergibt sie an die Darstellungskomponente. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "MolecularForge")
	void LoadNow();

	/** Die zuletzt geladene Struktur, oder nullptr. */
	UFUNCTION(BlueprintPure, Category = "MolecularForge")
	UMolecularStructure* GetStructure() const { return LoadedStructure; }

	UFUNCTION(BlueprintPure, Category = "MolecularForge")
	UMolecularAtomsComponent* GetAtomsComponent() const { return AtomsComponent; }

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MolecularForge")
	TObjectPtr<UMolecularAtomsComponent> AtomsComponent;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "MolecularForge")
	TObjectPtr<UMolecularStructure> LoadedStructure;
};
