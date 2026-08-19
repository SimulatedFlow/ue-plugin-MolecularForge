// Copyright Simulated Flow. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ProceduralMeshComponent.h"
#include "MolecularForgeTypes.h"
#include "MolRibbonBuilder.h"
#include "MolecularCartoonComponent.generated.h"

class UMaterialInterface;
class UMolecularStructure;

/**
 * Zeichnet die Struktur als Cartoon-Band — Schleifen als Rundprofil, Helices als flaches
 * Band, Faltblaetter als Band mit Pfeilspitze.
 *
 * Das ist die Darstellung, die man auf Titelbildern und in Lehrbuechern sieht, und sie ist
 * dort nicht aus Geschmacksgruenden Standard: bei einem Protein mit einigen tausend Atomen
 * verdeckt die Kugeldarstellung ihre eigene Struktur. Das Band laesst den Faltungsverlauf
 * ueberhaupt erst erkennen.
 *
 * Anders als Atome und Bindungen laesst sich das Band nicht instanzieren — es ist eine
 * einzige, durchgehende Flaeche. Es wird deshalb einmal erzeugt und danach nicht mehr
 * angefasst, solange sich die Struktur nicht aendert.
 */
UCLASS(ClassGroup = (MolecularForge), meta = (BlueprintSpawnableComponent),
	HideCategories = (Physics, Collision))
class MOLECULARFORGERENDER_API UMolecularCartoonComponent : public UProceduralMeshComponent
{
	GENERATED_BODY()

public:
	// UProceduralMeshComponent bietet keinen Standardkonstruktor an, sondern nur die
	// Form mit FObjectInitializer — die muss deshalb durchgereicht werden.
	UMolecularCartoonComponent(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MolecularForge")
	TObjectPtr<UMolecularStructure> Structure;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MolecularForge")
	EMolColorScheme ColorScheme = EMolColorScheme::SecondaryStructure;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MolecularForge")
	FLinearColor UniformColor = FLinearColor(0.4f, 0.7f, 1.f);

	/** Umrechnung Angstroem -> Unreal-Einheiten. Muss zu den Ladeoptionen passen. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MolecularForge", meta = (ClampMin = "0.01"))
	float UnitsPerAngstrom = 10.f;

	/** Stuetzpunkte je Residuum entlang der Kurve. Hoeher heisst glatter und teurer. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MolecularForge", meta = (ClampMin = "1", ClampMax = "24"))
	int32 SegmentsPerResidue = 6;

	/** Stuetzpunkte je Querschnitt. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MolecularForge", meta = (ClampMin = "3", ClampMax = "32"))
	int32 RingResolution = 12;

	/** Halbe Bandbreite einer Helix in Angstroem. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MolecularForge", meta = (ClampMin = "0.05"))
	float HelixHalfWidth = 0.90f;

	/** Halbe Bandbreite eines Faltblatts in Angstroem. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MolecularForge", meta = (ClampMin = "0.05"))
	float SheetHalfWidth = 1.00f;

	/** Radius des Rundprofils fuer Schleifen in Angstroem. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MolecularForge", meta = (ClampMin = "0.02"))
	float CoilRadius = 0.25f;

	/**
	 * Material des Bandes. Voreingestellt ist eines, das die Farbe aus den Vertices liest —
	 * dort hat die Erzeugung sie hingeschrieben.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MolecularForge")
	TObjectPtr<UMaterialInterface> RibbonMaterial;

	UFUNCTION(BlueprintCallable, Category = "MolecularForge")
	void SetStructure(UMolecularStructure* InStructure);

	/** Erzeugt das Band aus der aktuellen Struktur und den aktuellen Einstellungen neu. */
	UFUNCTION(BlueprintCallable, Category = "MolecularForge")
	void RebuildMesh();

	UFUNCTION(BlueprintPure, Category = "MolecularForge")
	int32 GetNumTriangles() const { return LastTriangleCount; }

	/** Zahl der zusammenhaengenden Rueckgrat-Abschnitte. Mehr als einer heisst: Luecken. */
	UFUNCTION(BlueprintPure, Category = "MolecularForge")
	int32 GetNumSegments() const { return LastSegmentCount; }

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

protected:
	virtual void OnRegister() override;

private:
	UPROPERTY(Transient)
	int32 LastTriangleCount = 0;

	UPROPERTY(Transient)
	int32 LastSegmentCount = 0;
};
