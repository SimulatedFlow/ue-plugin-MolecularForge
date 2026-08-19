// Copyright Simulated Flow. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTypes.h"
// FMassEntityHandle liegt in 5.8 hier und nicht in MassEntityTypes.h.
#include "Mass/EntityHandle.h"
#include "MolMesoscaleMath.h"
#include "MolMesoscaleFragments.generated.h"

/** Markiert eine Mass-Entity als ganzes Molekuel in der mesoskopischen Ansicht. */
USTRUCT()
struct MOLECULARFORGEMASS_API FMolMoleculeTag : public FMassTag
{
	GENERATED_BODY()
};

/**
 * Zustand eines einzelnen Molekuels.
 *
 * Hier ist eine Entity *ein ganzes Molekuel* und nicht ein Atom — das ist die
 * Architekturentscheidung aus dem Bauplan. Ein Atom in einer starren Struktur hat kein
 * Verhalten und gehoert in ein flaches Array; ein Molekuel, das diffundiert, bindet und
 * je nach Abstand anders gezeichnet wird, hat genau die Eigenschaften, fuer die Mass
 * gebaut wurde.
 */
USTRUCT()
struct MOLECULARFORGEMASS_API FMolMoleculeFragment : public FMassFragment
{
	GENERATED_BODY()

	/**
	 * Welche Molekuelart. Zeigt in die Artenliste des Spawners und nicht auf ein UObject —
	 * ein Zeiger je Entity waere bei zehntausenden Molekuelen sowohl Speicherverschwendung
	 * als auch ein Hindernis fuer die parallele Verarbeitung.
	 */
	UPROPERTY()
	int32 SpeciesIndex = 0;

	/** Kontaktradius in Angstroem — zugleich die Groesse der Kugeldarstellung. */
	UPROPERTY()
	float ContactRadius = 12.f;

	/** Woran dieses Molekuel gebunden ist. Ungesetzt heisst frei. */
	UPROPERTY()
	FMassEntityHandle BoundTo;

	/**
	 * Eigene Zufallsquelle je Entity.
	 *
	 * Notwendig, weil die Diffusion parallel ueber die Chunks laeuft: eine geteilte
	 * Quelle waere entweder ein Engpass oder wuerde je nach Abarbeitungsreihenfolge
	 * andere Zahlen liefern. So bleibt der Lauf bei gleichem Startwert wiederholbar,
	 * unabhaengig davon, wie die Arbeit auf die Threads faellt.
	 */
	UPROPERTY()
	FRandomStream RandomStream;

	UPROPERTY()
	EMolMesoDetail Detail = EMolMesoDetail::Full;
};

/**
 * Gemeinsame Parameter aller Molekuele einer Konfiguration.
 * Als Const-Shared-Fragment existiert das genau einmal und nicht je Entity.
 */
USTRUCT()
struct MOLECULARFORGEMASS_API FMolMesoscaleParameters : public FMassConstSharedFragment
{
	GENERATED_BODY()

	/**
	 * Diffusionskoeffizient in Angstroem^2 je Sekunde.
	 *
	 * Der Standardwert entspricht der Groessenordnung eines mittleren globulaeren
	 * Proteins in Wasser. Er ist bewusst in der Einheit der Fachliteratur angegeben,
	 * damit sich ein gemessener Wert unmittelbar einsetzen laesst.
	 */
	UPROPERTY(EditAnywhere, Category = "Diffusion", meta = (ClampMin = "0.0"))
	float DiffusionCoefficient = 1000000.f;

	/** Diffusion eines gebundenen Molekuels, als Anteil des freien Werts. */
	UPROPERTY(EditAnywhere, Category = "Diffusion", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BoundDiffusionScale = 0.1f;

	UPROPERTY(EditAnywhere, Category = "Grenzen")
	EMolBoundaryMode BoundaryMode = EMolBoundaryMode::Reflect;

	/** Mittelpunkt des erlaubten Raums in Unreal-Einheiten. */
	UPROPERTY(EditAnywhere, Category = "Grenzen")
	FVector BoundsCenter = FVector::ZeroVector;

	/** Halbe Kantenlaengen des erlaubten Raums in Unreal-Einheiten. */
	UPROPERTY(EditAnywhere, Category = "Grenzen")
	FVector BoundsExtent = FVector(5000.0, 5000.0, 5000.0);

	/** Umrechnung Angstroem -> Unreal-Einheiten. Muss zum Rest der Szene passen. */
	UPROPERTY(EditAnywhere, Category = "Diffusion", meta = (ClampMin = "0.01"))
	float UnitsPerAngstrom = 10.f;

	// ---- Bindung ----

	UPROPERTY(EditAnywhere, Category = "Bindung")
	bool bEnableBinding = true;

	/** Wahrscheinlichkeit je Sekunde, dass eine Begegnung in Reichweite haelt. */
	UPROPERTY(EditAnywhere, Category = "Bindung", meta = (ClampMin = "0.0"))
	float BindProbabilityPerSecond = 2.f;

	/** Wahrscheinlichkeit je Sekunde, dass sich eine bestehende Bindung wieder loest. */
	UPROPERTY(EditAnywhere, Category = "Bindung", meta = (ClampMin = "0.0"))
	float UnbindProbabilityPerSecond = 0.3f;

	// ---- Darstellungsstufen ----
	// Abstaende in Unreal-Einheiten.

	UPROPERTY(EditAnywhere, Category = "Detailstufen", meta = (ClampMin = "0.0"))
	float FullDetailDistance = 2000.f;

	UPROPERTY(EditAnywhere, Category = "Detailstufen", meta = (ClampMin = "0.0"))
	float BackboneDistance = 8000.f;

	UPROPERTY(EditAnywhere, Category = "Detailstufen", meta = (ClampMin = "0.0"))
	float BlobDistance = 40000.f;

	/** Kantenlaenge der Zellen fuer die Nachbarschaftssuche. 0 = automatisch. */
	UPROPERTY(EditAnywhere, Category = "Leistung", meta = (ClampMin = "0.0"))
	float BindingGridCellSize = 0.f;

	FBox3f GetBounds() const
	{
		const FVector3f Center(BoundsCenter);
		const FVector3f Extent(BoundsExtent);
		return FBox3f(Center - Extent, Center + Extent);
	}
};
