// Copyright Simulated Flow. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MolecularForgeTypes.h"
#include "MolNiagaraArrays.generated.h"

class UMolecularStructure;

/** Was an Niagara uebergeben wird und in welcher Form. */
USTRUCT(BlueprintType)
struct MOLECULARFORGENIAGARA_API FMolNiagaraOptions
{
	GENERATED_BODY()

	/** Umrechnung Angstroem -> Unreal-Einheiten. Muss zu den Ladeoptionen passen. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MolecularForge", meta = (ClampMin = "0.01"))
	float UnitsPerAngstrom = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MolecularForge")
	EMolColorScheme ColorScheme = EMolColorScheme::Element;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MolecularForge")
	FLinearColor UniformColor = FLinearColor::White;

	/**
	 * Welche Atome uebergeben werden.
	 * Space-Filling nimmt alles, Rueckgrat und CA-Spur duennen aus — bei einem grossen
	 * Komplex ist der Unterschied zwischen 150.000 und 3.000 Partikeln entscheidend
	 * dafuer, ob der Effekt in Echtzeit laeuft.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MolecularForge")
	EMolRepresentation AtomSubset = EMolRepresentation::SpaceFilling;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MolecularForge")
	bool bShowWater = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MolecularForge")
	bool bShowHydrogen = false;

	/** Zusaetzlich die Bindungen als Start-/Endpunktpaare uebergeben. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MolecularForge")
	bool bIncludeBonds = false;

	/** Zusaetzlicher Faktor auf den Van-der-Waals-Radius. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MolecularForge", meta = (ClampMin = "0.01"))
	float RadiusScale = 1.f;

	/**
	 * Obergrenze fuer die Partikelzahl. 0 heisst unbegrenzt.
	 * Beim Kuerzen wird gleichmaessig ueber die Struktur ausgeduennt und nicht vorne
	 * abgeschnitten — sonst fehlte einfach das halbe Molekuel.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MolecularForge", meta = (ClampMin = "0"))
	int32 MaxAtoms = 0;
};

/**
 * Die fertig aufbereiteten Daten, wie Niagara sie erwartet.
 * Positionen bereits in Unreal-Einheiten, Farben linear.
 */
struct FMolNiagaraArrays
{
	TArray<FVector> Positions;
	TArray<FLinearColor> Colors;
	TArray<float> Radii;

	/** Nur belegt, wenn bIncludeBonds gesetzt war. Beide Arrays sind gleich lang. */
	TArray<FVector> BondStarts;
	TArray<FVector> BondEnds;

	/** Wie viele Atome die Struktur vor dem Ausduennen beigesteuert haette. */
	int32 NumAtomsBeforeLimit = 0;

	int32 Num() const { return Positions.Num(); }
	bool IsEmpty() const { return Positions.IsEmpty(); }

	void Reset()
	{
		Positions.Reset();
		Colors.Reset();
		Radii.Reset();
		BondStarts.Reset();
		BondEnds.Reset();
		NumAtomsBeforeLimit = 0;
	}
};

namespace MolecularForge
{
	/**
	 * Bereitet die Atomdaten fuer Niagara auf.
	 *
	 * Bewusst ohne jeden Niagara-Bezug: was hier passiert — filtern, umrechnen, einfaerben,
	 * ausduennen — ist reine Datenarbeit und laesst sich so ohne laufendes Partikelsystem
	 * pruefen. Die Uebergabe an Niagara ist danach ein Dreizeiler.
	 */
	MOLECULARFORGENIAGARA_API void BuildNiagaraArrays(
		const UMolecularStructure& Structure,
		const FMolNiagaraOptions& Options,
		FMolNiagaraArrays& OutArrays);

	/**
	 * Namen der Parameter, unter denen die Daten im Niagara-System ankommen.
	 * Stehen an einer Stelle, damit Doku, Beispielsystem und Code nicht auseinanderlaufen.
	 */
	namespace NiagaraParameterNames
	{
		MOLECULARFORGENIAGARA_API extern const FName AtomPositions;
		MOLECULARFORGENIAGARA_API extern const FName AtomColors;
		MOLECULARFORGENIAGARA_API extern const FName AtomRadii;
		MOLECULARFORGENIAGARA_API extern const FName AtomCount;
		MOLECULARFORGENIAGARA_API extern const FName BondStarts;
		MOLECULARFORGENIAGARA_API extern const FName BondEnds;
		MOLECULARFORGENIAGARA_API extern const FName BondCount;
	}
}
