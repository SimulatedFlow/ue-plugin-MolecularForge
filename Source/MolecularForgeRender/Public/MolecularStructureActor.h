// Copyright 2026 Silvan Teufel All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MolecularForgeTypes.h"
#include "MolecularStructureActor.generated.h"

class UMolecularAtomsComponent;
class UMolecularBondsComponent;
class UMolecularCartoonComponent;
class UMolecularSurfaceComponent;
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

	/**
	 * Darstellungsart. Schaltet die drei Komponenten passend zu- und ab, statt sie
	 * einzeln bedienen zu muessen.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MolecularForge")
	EMolRepresentation Representation = EMolRepresentation::Cartoon;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MolecularForge")
	EMolColorScheme ColorScheme = EMolColorScheme::SecondaryStructure;

	/** Uebertraegt Darstellungsart und Faerbung auf die Komponenten und baut sie neu auf. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "MolecularForge")
	void ApplyRepresentation();

	/** Laedt die Datei aus StructureFilePath und uebergibt sie an die Darstellungskomponente. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "MolecularForge")
	void LoadNow();

	/** Die zuletzt geladene Struktur, oder nullptr. */
	UFUNCTION(BlueprintPure, Category = "MolecularForge")
	UMolecularStructure* GetStructure() const { return LoadedStructure; }

	UFUNCTION(BlueprintPure, Category = "MolecularForge")
	UMolecularAtomsComponent* GetAtomsComponent() const { return AtomsComponent; }

	UFUNCTION(BlueprintPure, Category = "MolecularForge")
	UMolecularBondsComponent* GetBondsComponent() const { return BondsComponent; }

	UFUNCTION(BlueprintPure, Category = "MolecularForge")
	UMolecularCartoonComponent* GetCartoonComponent() const { return CartoonComponent; }

	UFUNCTION(BlueprintPure, Category = "MolecularForge")
	UMolecularSurfaceComponent* GetSurfaceComponent() const { return SurfaceComponent; }

protected:
	virtual void BeginPlay() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MolecularForge")
	TObjectPtr<UMolecularAtomsComponent> AtomsComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MolecularForge")
	TObjectPtr<UMolecularBondsComponent> BondsComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MolecularForge")
	TObjectPtr<UMolecularCartoonComponent> CartoonComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MolecularForge")
	TObjectPtr<UMolecularSurfaceComponent> SurfaceComponent;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "MolecularForge")
	TObjectPtr<UMolecularStructure> LoadedStructure;
};
