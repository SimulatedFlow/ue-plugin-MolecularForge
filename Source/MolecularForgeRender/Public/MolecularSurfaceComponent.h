// Copyright Simulated Flow. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ProceduralMeshComponent.h"
#include "MolecularForgeTypes.h"
#include "MolecularSurfaceComponent.generated.h"

class UMolecularStructure;

/**
 * Zeichnet die Molekueloberflaeche als geschlossene Huelle.
 *
 * Die Oberflaeche beantwortet eine andere Frage als Kugeln oder Band: nicht "wo sitzt
 * welches Atom" und nicht "wie ist die Kette gefaltet", sondern "wie sieht dieses Molekuel
 * von aussen aus und wo sind seine Taschen". Fuer Bindungsstellen und fuer alles, was
 * beeindrucken soll, ist sie die richtige Wahl.
 *
 * Der Aufbau ist der teuerste im ganzen Plugin — ein Dichtefeld ueber dem gesamten
 * Molekuelvolumen. Er passiert deshalb nur auf Anforderung und nicht bei jeder Aenderung
 * einer Nebensache.
 */
UCLASS(ClassGroup = (MolecularForge), meta = (BlueprintSpawnableComponent),
	HideCategories = (Physics, Collision))
class MOLECULARFORGERENDER_API UMolecularSurfaceComponent : public UProceduralMeshComponent
{
	GENERATED_BODY()

public:
	UMolecularSurfaceComponent(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MolecularForge")
	TObjectPtr<UMolecularStructure> Structure;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MolecularForge")
	EMolColorScheme ColorScheme = EMolColorScheme::Element;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MolecularForge")
	FLinearColor UniformColor = FLinearColor(0.85f, 0.85f, 0.9f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MolecularForge", meta = (ClampMin = "0.01"))
	float UnitsPerAngstrom = 10.f;

	/**
	 * Zellkante des Dichtegitters in Angstroem.
	 * Der Aufwand waechst mit der dritten Potenz — 0,3 statt 0,6 kostet das Achtfache.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MolecularForge", meta = (ClampMin = "0.1", UIMax = "2.0"))
	float VoxelSizeAngstrom = 0.6f;

	/** 0 ergibt die Van-der-Waals-Oberflaeche, etwa 1,4 die loesungsmittelzugaengliche. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MolecularForge", meta = (ClampMin = "0.0", UIMax = "3.0"))
	float RadiusInflationAngstrom = 0.f;

	/** Muss negativ sein. Betragsgroesser heisst kantiger, betragskleiner weicher verschmolzen. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MolecularForge", meta = (UIMin = "-6.0", UIMax = "-0.5"))
	float Blobbiness = -2.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MolecularForge")
	bool bShowWater = false;

	UFUNCTION(BlueprintCallable, Category = "MolecularForge")
	void SetStructure(UMolecularStructure* InStructure);

	/** Erzeugt die Oberflaeche neu. Teuer — nur aufrufen, wenn sich wirklich etwas geaendert hat. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "MolecularForge")
	void RebuildMesh();

	UFUNCTION(BlueprintPure, Category = "MolecularForge")
	int32 GetNumTriangles() const { return LastTriangleCount; }

	/** Leer bei Erfolg, sonst der Grund des letzten Fehlschlags. */
	UFUNCTION(BlueprintPure, Category = "MolecularForge")
	FString GetLastError() const { return LastError; }

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

protected:
	virtual void OnRegister() override;

private:
	UPROPERTY(Transient)
	int32 LastTriangleCount = 0;

	UPROPERTY(Transient)
	FString LastError;
};
