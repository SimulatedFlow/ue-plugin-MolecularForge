// Copyright 2026 Silvan Teufel All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "MolecularForgeTypes.h"
#include "MolecularStructure.generated.h"

/**
 * Eine geladene Molekuelstruktur.
 *
 * Die Atomdaten liegen bewusst als Struct-of-Arrays vor, nicht als Array-of-Structs.
 * Drei Gruende, die alle drei zaehlen:
 *   1. Die Mesh- und Bindungserzeugung liest fast immer nur die Positionen. Als SoA
 *      passen viermal so viele Positionen in dieselbe Cacheline.
 *   2. `AtomPositions` kann als zusammenhaengender Block direkt an Instancing-Buffer
 *      und spaeter an das Niagara Data Interface gereicht werden, ohne Umkopieren.
 *   3. Phase 4 setzt Mass-Entities auf diese Daten. Mass erwartet POD-Fragmente in
 *      Chunks — ein AoS mit UObject-Bezuegen waere dort eine Sackgasse.
 *
 * Alle Atom-Arrays haben dieselbe Laenge (`GetNumAtoms()`) und sind parallel indiziert.
 * Positionen stehen in Angstroem, so wie sie in der Datei stehen. Die Umrechnung in
 * Unreal-Einheiten passiert erst bei der Darstellung, damit Messwerte und Radien
 * durchgehend in der Einheit bleiben, in der die Fachwelt rechnet.
 */
UCLASS(BlueprintType)
class MOLECULARFORGERUNTIME_API UMolecularStructure : public UObject
{
	GENERATED_BODY()

public:
	// ---- Atomdaten (SoA, parallel indiziert) ----

	/** Position in Angstroem. */
	UPROPERTY()
	TArray<FVector3f> AtomPositions;

	/** Ordnungszahl. 0 = unbekannt. */
	UPROPERTY()
	TArray<uint8> AtomElements;

	/** Index in `Residues`. */
	UPROPERTY()
	TArray<int32> AtomResidueIndices;

	/** B-Faktor in A^2, bei AlphaFold-Dateien stattdessen pLDDT in 0..100. */
	UPROPERTY()
	TArray<float> AtomBFactors;

	/** Besetzung 0..1. Werte unter 1 bedeuten alternative Konformationen. */
	UPROPERTY()
	TArray<float> AtomOccupancies;

	/** Bitmaske aus EMolAtomFlags. */
	UPROPERTY()
	TArray<uint8> AtomFlags;

	/** Atomname aus der Datei, getrimmt ("CA", "OG1", "C1'"). */
	UPROPERTY()
	TArray<FName> AtomNames;

	// ---- Aggregatdaten ----

	UPROPERTY()
	TArray<FMolResidue> Residues;

	UPROPERTY()
	TArray<FMolChain> Chains;

	UPROPERTY()
	TArray<FMolBond> Bonds;

	UPROPERTY(BlueprintReadOnly, Category = "MolecularForge")
	FMolStructureMeta Meta;

	/** Hoechster gefundener B-Faktor bzw. pLDDT. Fuer die Normierung der Faerbung. */
	UPROPERTY()
	float MaxBFactor = 0.f;

	/** Niedrigster gefundener B-Faktor bzw. pLDDT. */
	UPROPERTY()
	float MinBFactor = 0.f;

	// ---- Zugriff ----

	UFUNCTION(BlueprintPure, Category = "MolecularForge")
	int32 GetNumAtoms() const { return AtomPositions.Num(); }

	UFUNCTION(BlueprintPure, Category = "MolecularForge")
	int32 GetNumResidues() const { return Residues.Num(); }

	UFUNCTION(BlueprintPure, Category = "MolecularForge")
	int32 GetNumChains() const { return Chains.Num(); }

	UFUNCTION(BlueprintPure, Category = "MolecularForge")
	int32 GetNumBonds() const { return Bonds.Num(); }

	UFUNCTION(BlueprintPure, Category = "MolecularForge")
	bool IsEmpty() const { return AtomPositions.IsEmpty(); }

	/** Position in Angstroem. Ausserhalb des gueltigen Bereichs kommt der Nullvektor. */
	UFUNCTION(BlueprintPure, Category = "MolecularForge")
	FVector GetAtomPosition(int32 AtomIndex) const;

	/** Van-der-Waals-Radius des Atoms in Angstroem. */
	UFUNCTION(BlueprintPure, Category = "MolecularForge")
	float GetAtomVdWRadius(int32 AtomIndex) const;

	/** Elementsymbol, z.B. "Fe". */
	UFUNCTION(BlueprintPure, Category = "MolecularForge")
	FString GetAtomElementSymbol(int32 AtomIndex) const;

	/** Farbe des Atoms unter dem angegebenen Schema. */
	UFUNCTION(BlueprintPure, Category = "MolecularForge")
	FLinearColor GetAtomColor(int32 AtomIndex, EMolColorScheme Scheme, FLinearColor UniformColor = FLinearColor::White) const;

	/** Achsenparallele Huelle in Angstroem, um den geladenen Ursprung. */
	UFUNCTION(BlueprintPure, Category = "MolecularForge")
	FBox GetBoundsAngstrom() const;

	/** Aminosaeuresequenz einer Kette als Einbuchstaben-Code. */
	UFUNCTION(BlueprintPure, Category = "MolecularForge")
	FString GetChainSequence(int32 ChainIndex) const;

	/** Kurzbeschreibung fuer Logs und UI, z.B. "1CRN — 327 Atome, 46 Residuen, 1 Kette". */
	UFUNCTION(BlueprintPure, Category = "MolecularForge")
	FString GetSummary() const;

	/** Alle Arrays leeren und Metadaten zuruecksetzen. */
	void Reset();

	/**
	 * Vorab reservieren, damit die parallele Befuellung ohne Reallokation auskommt.
	 * Muss aufgerufen werden, bevor Threads in die Arrays schreiben.
	 */
	void PreallocateAtoms(int32 NumAtoms);

	/**
	 * Nach dem Laden aufrufen. Berechnet Huelle, B-Faktor-Spanne, Kettenarten und
	 * Einbuchstaben-Codes. Bis dahin sind diese Felder nicht gueltig.
	 */
	void FinalizeAfterLoad();

	/** Verschiebt alle Atome so, dass der geometrische Mittelpunkt im Ursprung liegt. */
	void CenterOnOrigin();

private:
	/** Cache der Huelle, gefuellt von FinalizeAfterLoad. */
	UPROPERTY()
	FBox3f CachedBounds = FBox3f(ForceInit);
};
