// Copyright 2026 Simulated Flow All Rights Reserved.

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

	// ---- Auswahl ----

	/**
	 * Waehlt Atome ueber einen Ausdruck aus, etwa `chain A and resi 10-40`
	 * oder `within 5 of resn ATP`.
	 *
	 * @param OutAtomIndices	Die gefundenen Atomindizes.
	 * @param OutError			Leer bei Erfolg, sonst was nicht verstanden wurde.
	 * @return					False bei fehlerhaftem Ausdruck.
	 */
	UFUNCTION(BlueprintCallable, Category = "MolecularForge|Auswahl")
	static bool SelectAtoms(
		const UMolecularStructure* Structure,
		const FString& Expression,
		TArray<int32>& OutAtomIndices,
		FString& OutError);

	// ---- Messung ----
	// Alle Laengen in Angstroem, alle Winkel in Grad — so, wie in der Strukturbiologie
	// gerechnet wird. Umgerechnet wird erst bei der Darstellung.

	/** Abstand zweier Atome. Negativ bei ungueltigen Indizes. */
	UFUNCTION(BlueprintPure, Category = "MolecularForge|Messung")
	static float MeasureDistance(const UMolecularStructure* Structure, int32 AtomA, int32 AtomB);

	/** Bindungswinkel A-B-C mit dem Scheitel bei B. */
	UFUNCTION(BlueprintPure, Category = "MolecularForge|Messung")
	static float MeasureAngle(const UMolecularStructure* Structure, int32 AtomA, int32 AtomB, int32 AtomC);

	/** Torsionswinkel A-B-C-D, im Bereich -180 bis 180. */
	UFUNCTION(BlueprintPure, Category = "MolecularForge|Messung")
	static float MeasureDihedral(
		const UMolecularStructure* Structure, int32 AtomA, int32 AtomB, int32 AtomC, int32 AtomD);

	/** Geometrischer Mittelpunkt der angegebenen Atome. Leere Liste heisst alle Atome. */
	UFUNCTION(BlueprintPure, Category = "MolecularForge|Messung")
	static FVector GetSelectionCentroid(const UMolecularStructure* Structure, const TArray<int32>& AtomIndices);

	/** Massenschwerpunkt der angegebenen Atome. Leere Liste heisst alle Atome. */
	UFUNCTION(BlueprintPure, Category = "MolecularForge|Messung")
	static FVector GetSelectionCenterOfMass(const UMolecularStructure* Structure, const TArray<int32>& AtomIndices);

	/**
	 * Traegheitsradius der Auswahl. Ein Mass fuer die Ausdehnung — ein kompakt gefaltetes
	 * Protein hat einen deutlich kleineren als dasselbe Protein entfaltet.
	 */
	UFUNCTION(BlueprintPure, Category = "MolecularForge|Messung")
	static float GetSelectionRadiusOfGyration(const UMolecularStructure* Structure, const TArray<int32>& AtomIndices);

	/** Gesamtmasse der Auswahl in atomaren Masseneinheiten. */
	UFUNCTION(BlueprintPure, Category = "MolecularForge|Messung")
	static float GetSelectionMass(const UMolecularStructure* Structure, const TArray<int32>& AtomIndices);

	/** Achsenparallele Huelle der Auswahl in Angstroem. */
	UFUNCTION(BlueprintPure, Category = "MolecularForge|Messung")
	static FBox GetSelectionBounds(const UMolecularStructure* Structure, const TArray<int32>& AtomIndices);
};
