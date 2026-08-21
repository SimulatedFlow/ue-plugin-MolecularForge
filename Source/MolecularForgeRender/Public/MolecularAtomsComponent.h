// Copyright 2026 Simulated Flow All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "MolecularForgeTypes.h"
#include "MolecularAtomsComponent.generated.h"

class UMaterialInterface;
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

	/**
	 * Material der Kugeln.
	 *
	 * Voreingestellt ist eines, das die Farbe aus den Per-Instance-Daten liest — ohne das
	 * waeren alle Atome gleich grau, obwohl die Farben danebenliegen. Es wird bei jedem
	 * Registrieren neu gesetzt: eine Zuweisung im Konstruktor allein ueberlebt das
	 * Speichern eines Levels nicht zuverlaessig, und dann rendert die Engine
	 * stillschweigend das Material der Grundform.
	 *
	 * Wer eigene Optik will, traegt hier einfach ein anderes ein.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MolecularForge")
	TObjectPtr<UMaterialInterface> AtomMaterial;

	/**
	 * Kugeln als Impostoren zeichnen statt als Geometrie.
	 *
	 * Ein Impostor ist ein Viereck, in dem der Pixelshader die Kugel ausrechnet. Aus 382
	 * Dreiecken je Atom werden zwei — bei 150.000 Atomen der Unterschied zwischen 57
	 * Millionen Dreiecken und 300.000. Die Silhouette wird dabei nicht schlechter,
	 * sondern besser: sie ist analytisch exakt statt facettiert.
	 *
	 * ACHTUNG: derzeit voreingestellt AUS. Das Material ist gebaut, aber die Ausrichtung
	 * des Vierecks im Vertexshader liefert noch nichts Sichtbares — siehe die offenen
	 * Punkte im Bauplan. Bis das steht, zeichnet das Plugin echte Kugelgeometrie.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MolecularForge")
	bool bUseImpostorSpheres = false;

	/** Material der Impostor-Darstellung. Noch nicht einsatzbereit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MolecularForge", meta = (AdvancedDisplay))
	TObjectPtr<UMaterialInterface> ImpostorMaterial;

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

	/**
	 * Uebernimmt nur die Positionen aus der Struktur, ohne Filter, Farben und Radien neu
	 * zu bestimmen.
	 *
	 * Fuer das Abspielen einer Trajektorie ist der Unterschied entscheidend: dort aendern
	 * sich Bild fuer Bild ausschliesslich die Koordinaten, waehrend Elemente, Auswahl und
	 * Faerbung gleich bleiben. Ein vollstaendiger Neuaufbau je Bild wuerde die immer
	 * gleiche Arbeit dreissigmal pro Sekunde wiederholen.
	 *
	 * Stimmt die gespeicherte Zuordnung nicht mehr zur Instanzzahl — etwa nach dem Laden
	 * einer Szene —, wird von selbst vollstaendig neu aufgebaut.
	 */
	UFUNCTION(BlueprintCallable, Category = "MolecularForge")
	void RefreshTransformsFromStructure();

	/** Zahl der zuletzt erzeugten Instanzen. Nach Filtern kleiner als die Atomzahl. */
	UFUNCTION(BlueprintPure, Category = "MolecularForge")
	int32 GetNumVisibleAtoms() const { return NumVisibleAtoms; }

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

protected:
	virtual void OnRegister() override;

private:
	/** Setzt Mesh und Material passend zur gewaehlten Darstellungsweise. */
	void ApplyImpostorSettings();

	/**
	 * Liefert das Impostor-Material, laedt es aber erst beim ersten Bedarf und meldet
	 * nichts, wenn es fehlt. Es gehoert nicht zum Lieferumfang.
	 */
	UMaterialInterface* ResolveImpostorMaterial();

	/** Entscheidet anhand von Flags und Filtern, ob ein Atom gezeichnet wird. */
	bool ShouldDrawAtom(int32 AtomIndex) const;

	/** Radiusfaktor der aktuellen Darstellungsart. */
	float GetRepresentationRadiusFactor() const;

	UPROPERTY(Transient)
	int32 NumVisibleAtoms = 0;

	/** Welches Atom hinter welcher Instanz steckt. Nur fuer das schnelle Aktualisieren. */
	UPROPERTY(Transient)
	TArray<int32> InstanceAtomIndices;

	/** Das Kugelmesh fuer die Darstellung ohne Impostoren. */
	UPROPERTY()
	TObjectPtr<UStaticMesh> SphereMesh;

	/** Das Viereck, auf dem die Impostoren gezeichnet werden. */
	UPROPERTY()
	TObjectPtr<UStaticMesh> QuadMesh;
};
