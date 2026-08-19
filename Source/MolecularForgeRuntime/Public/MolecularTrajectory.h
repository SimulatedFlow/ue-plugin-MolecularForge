// Copyright Simulated Flow. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "MolecularForgeTypes.h"
#include "MolecularTrajectory.generated.h"

/**
 * Eine Molekuedynamik-Trajektorie: dieselben Atome ueber viele Zeitschritte hinweg.
 *
 * Die Positionen liegen in einem einzigen flachen Array, Bild fuer Bild hintereinander.
 * Das ist kein Detail, sondern der Grund, warum das Abspielen billig ist: ein Zeitschritt
 * ist ein zusammenhaengender Speicherbereich, den man am Stueck lesen und weiterreichen
 * kann. Ein Array von Arrays haette dieselben Daten ueber den Speicher verstreut.
 *
 * Was hier bewusst *nicht* passiert: es wird nichts gerechnet. Trajektorien werden von
 * GROMACS, NAMD oder AMBER erzeugt, oft ueber Tage auf Rechenclustern. Eine Spiel-Engine
 * spielt sie ab — sie erzeugt sie nicht.
 */
UCLASS(BlueprintType)
class MOLECULARFORGERUNTIME_API UMolecularTrajectory : public UObject
{
	GENERATED_BODY()

public:
	/** Atome je Zeitschritt. Muss zur Struktur passen, auf die die Trajektorie angewandt wird. */
	UPROPERTY(BlueprintReadOnly, Category = "MolecularForge")
	int32 NumAtoms = 0;

	/**
	 * Positionen in Angstroem, Bild fuer Bild hintereinander.
	 * Index eines Atoms: Frame * NumAtoms + Atom.
	 */
	UPROPERTY()
	TArray<FVector3f> Positions;

	/** Kantenlaengen der periodischen Box je Zeitschritt, falls die Datei sie mitbrachte. */
	UPROPERTY()
	TArray<FVector3f> UnitCellSizes;

	/** Zeit zwischen zwei gespeicherten Bildern in Pikosekunden. 0 wenn unbekannt. */
	UPROPERTY(BlueprintReadOnly, Category = "MolecularForge")
	float TimeStepPicoseconds = 0.f;

	/** Woher die Trajektorie stammt. Fuer Anzeige und Fehlersuche. */
	UPROPERTY(BlueprintReadOnly, Category = "MolecularForge")
	FString SourceFile;

	UFUNCTION(BlueprintPure, Category = "MolecularForge")
	int32 GetNumFrames() const { return NumAtoms > 0 ? Positions.Num() / NumAtoms : 0; }

	UFUNCTION(BlueprintPure, Category = "MolecularForge")
	bool IsEmpty() const { return GetNumFrames() == 0; }

	/** Gesamtdauer in Pikosekunden. 0, wenn die Datei keine Schrittweite nannte. */
	UFUNCTION(BlueprintPure, Category = "MolecularForge")
	float GetDurationPicoseconds() const
	{
		return TimeStepPicoseconds * FMath::Max(0, GetNumFrames() - 1);
	}

	/** Position eines Atoms in einem Bild, in Angstroem. */
	UFUNCTION(BlueprintPure, Category = "MolecularForge")
	FVector GetAtomPosition(int32 Frame, int32 Atom) const;

	/** Zusammenhaengender Blick auf ein Bild. Leer bei ungueltigem Index. */
	TArrayView<const FVector3f> GetFrame(int32 Frame) const;

	FString GetSummary() const;

	void Reset();

	/**
	 * Schreibt einen Zwischenzustand in das Zielarray.
	 *
	 * `FrameTime` wird in Bildern gezaehlt, Nachkommastellen erlaubt: 3.5 liegt genau
	 * zwischen Bild 3 und Bild 4. Zwischen den Bildern wird geradlinig interpoliert.
	 *
	 * Zur Einordnung: geradlinig ist geometrisch nicht ganz richtig, weil sich Atome
	 * auf Kreisbahnen um Bindungen bewegen und nicht auf Geraden. Bei den ueblichen
	 * Speicherabstaenden sind die Auslenkungen aber so klein, dass der Unterschied unter
	 * der Strichstaerke bleibt — und alle gaengigen Abspielprogramme machen es genauso.
	 * Wer exakte Zwischenzustaende braucht, muss sie rechnen lassen und nicht interpolieren.
	 */
	void SampleInto(float FrameTime, TArray<FVector3f>& OutPositions) const;
};
