// Copyright Silvan Teufel. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "MolecularForgeTypes.h"
#include "MolecularAtomsComponent.generated.h"

class UMolecularStructure;

/**
 * Stellt die Atome einer Struktur als Instanzen dar.
 *
 * Zur Umsetzung: Instanced Static Meshes sind hier der Zwischenschritt, nicht das Ziel.
 * Sie erlauben es, in Phase 1 ohne eigenes Material und ohne eigenen Renderpfad ein
 * korrektes Bild zu erzeugen — man sieht das Molekuel, die Farben stimmen, die Radien
 * stimmen. Ab etwa 50.000 Atomen wird die Dreiecksmenge aber zum Flaschenhals, weil
 * jede Kugel echte Geometrie ist.
 *
 * Phase 2 ersetzt das Kugelmesh durch ein Impostor-Material: ein Quad je Atom, in dem
 * der Pixelshader die Kugel analytisch schneidet und per Pixel Depth Offset die richtige
 * Tiefe schreibt. Das Bild ist dabei nicht schlechter, sondern besser — die Silhouette
 * ist exakt statt facettiert — und die Dreiecksmenge faellt um zwei Groessenordnungen.
 * Die Schnittstelle dieser Komponente bleibt dabei unveraendert.
 *
 * Farbe und Radius gehen bereits jetzt als Per-Instance Custom Data hinaus (0..2 = RGB,
 * 3 = Radius in Angstroem), damit das Impostor-Material spaeter nur noch andocken muss.
 */
UCLASS(ClassGroup = (MolecularForge), meta = (BlueprintSpawnableComponent),
	HideCategories = (Instances, Physics, Collision))
class MOLECULARFORGERENDER_API UMolecularAtomsComponent : public UInstancedStaticMeshComponent
{
	GENERATED_BODY()

public:
	UMolecularAtomsComponent();

	/** Die darzustellende Struktur. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MolecularForge")
	TObjectPtr<UMolecularStructure> Structure;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MolecularForge")
	EMolRepresentation Representation = EMolRepresentation::SpaceFilling;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MolecularForge")
	EMolColorScheme ColorScheme = EMolColorScheme::Element;

	/** Nur wirksam bei ColorScheme == Uniform. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MolecularForge")
	FLinearColor UniformColor = FLinearColor(0.35f, 0.7f, 1.f);

	/**
	 * Zusaetzlicher Faktor auf den Atomradius.
	 * Space-Filling nutzt den vollen Van-der-Waals-Radius (Faktor 1), Ball-and-Stick
	 * verkleinert intern auf ein Viertel, damit die Staebe sichtbar bleiben.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MolecularForge", meta = (ClampMin = "0.01", UIMax = "2.0"))
	float RadiusScale = 1.f;

	/** Umrechnung Angstroem -> Unreal-Einheiten. Muss zu den Ladeoptionen passen. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MolecularForge", meta = (ClampMin = "0.01"))
	float UnitsPerAngstrom = 10.f;

	/** Wassermolekuele mitzeichnen, sofern sie ueberhaupt geladen wurden. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MolecularForge")
	bool bShowWater = false;

	/** Wasserstoffatome mitzeichnen. Verdoppelt bei NMR-Strukturen die Instanzzahl. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MolecularForge")
	bool bShowHydrogen = false;

	/** Setzt die Struktur und baut die Instanzen neu auf. */
	UFUNCTION(BlueprintCallable, Category = "MolecularForge")
	void SetStructure(UMolecularStructure* InStructure);

	/**
	 * Baut die Instanzen aus der aktuellen Struktur und den aktuellen Einstellungen neu auf.
	 * Nach jeder Aenderung an Darstellung, Faerbung oder Filtern noetig.
	 */
	UFUNCTION(BlueprintCallable, Category = "MolecularForge")
	void RebuildInstances();

	/** Zahl der zuletzt erzeugten Instanzen. Nach Filtern kleiner als die Atomzahl. */
	UFUNCTION(BlueprintPure, Category = "MolecularForge")
	int32 GetNumVisibleAtoms() const { return NumVisibleAtoms; }

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

protected:
	virtual void OnRegister() override;

private:
	/** Entscheidet anhand von Flags und Filtern, ob ein Atom gezeichnet wird. */
	bool ShouldDrawAtom(int32 AtomIndex) const;

	/** Radiusfaktor der aktuellen Darstellungsart. */
	float GetRepresentationRadiusFactor() const;

	UPROPERTY(Transient)
	int32 NumVisibleAtoms = 0;
};
