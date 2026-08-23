// Copyright 2026 Silvan Teufel All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UMolecularTrajectory;

/** Steuert, wie viel einer Trajektorie geladen wird. */
struct MOLECULARFORGERUNTIME_API FMolTrajectoryLoadOptions
{
	/**
	 * Hoechstzahl geladener Bilder. 0 heisst alle.
	 * Eine Trajektorie kann leicht mehrere Gigabyte gross sein — bei 100.000 Atomen
	 * kostet jedes Bild gut ein Megabyte, und Laeufe mit zehntausenden Bildern sind
	 * in der Simulation normal.
	 */
	int32 MaxFrames = 0;

	/** Nur jedes n-te Bild uebernehmen. Halbiert bei 2 den Speicherbedarf. */
	int32 FrameStride = 1;

	/** Die Boxabmessungen mitlesen, falls die Datei sie enthaelt. */
	bool bLoadUnitCell = true;
};

/** Ergebnis eines Trajektorienladevorgangs. */
struct MOLECULARFORGERUNTIME_API FMolTrajectoryResult
{
	bool bSuccess = false;
	FString Error;

	int32 NumFramesLoaded = 0;

	/** Wie viele Bilder die Datei laut Kopfdaten enthaelt. Kann von den geladenen abweichen. */
	int32 NumFramesInFile = 0;

	int32 NumAtoms = 0;
	double ParseSeconds = 0.0;
};

namespace MolecularForge
{
	/**
	 * Liest eine Trajektorie im DCD-Format (CHARMM, NAMD, X-PLOR).
	 *
	 * Warum DCD und nicht XTC: XTC ist das verbreitetere Format, komprimiert aber mit einem
	 * eigenen Ganzzahlverfahren, dessen Umsetzung mehrere hundert Zeilen kniffliger
	 * Bitschieberei braucht. Ein Fehler darin erzeugt keine Fehlermeldung, sondern
	 * Koordinaten, die plausibel aussehen und falsch sind — bei einer Trajektorie faellt
	 * das niemandem auf. DCD ist dagegen ein schlichtes Binaerformat mit Laengenmarken um
	 * jeden Block, und genau diese Marken erlauben es, beim Lesen fortlaufend zu pruefen,
	 * ob man noch richtig liegt. Siehe auch die offenen Punkte im Bauplan.
	 *
	 * Unterstuetzt beide Bytereihenfolgen; sie wird an der ersten Laengenmarke erkannt.
	 * Dateien mit festgehaltenen Atomen (NAMNF > 0) werden mit Begruendung abgelehnt
	 * statt halb richtig gelesen — dort enthalten die Folgebilder nur die beweglichen
	 * Atome, und wer das uebersieht, bekommt eine durcheinandergewuerfelte Struktur.
	 */
	MOLECULARFORGERUNTIME_API FMolTrajectoryResult ParseDcd(
		TArrayView<const uint8> Bytes,
		const FMolTrajectoryLoadOptions& Options,
		UMolecularTrajectory& OutTrajectory);

	/** Bequemer Weg ueber eine Datei auf der Platte. */
	MOLECULARFORGERUNTIME_API FMolTrajectoryResult ParseDcdFile(
		const FString& FilePath,
		const FMolTrajectoryLoadOptions& Options,
		UMolecularTrajectory& OutTrajectory);
}
