// Copyright 2026 Silvan Teufel All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MolecularForgeTypes.h"
#include "MolRibbonBuilder.h"

class UMolecularStructure;

/** Masse und Feinheit der Oberflaeche. Alle Laengen in Angstroem. */
struct FMolSurfaceOptions
{
	float UnitsPerAngstrom = 10.f;

	/**
	 * Kantenlaenge einer Gitterzelle. Kleiner heisst feiner und deutlich teurer —
	 * der Aufwand waechst mit der dritten Potenz.
	 */
	float VoxelSizeAngstrom = 0.6f;

	/**
	 * Aufschlag auf den Van-der-Waals-Radius.
	 * 0 ergibt die Van-der-Waals-Oberflaeche. Etwa 1,4 (der Radius eines Wassermolekuels)
	 * ergibt naeherungsweise die loesungsmittelzugaengliche Oberflaeche.
	 */
	float RadiusInflationAngstrom = 0.f;

	/**
	 * Schaerfe der Gauss-Glocken. Muss negativ sein; betragsgroesser heisst kantiger
	 * und naeher an den Einzelkugeln, betragskleiner heisst weicher verschmolzen.
	 */
	float Blobbiness = -2.3f;

	/** Schwellwert, an dem die Oberflaeche liegt. */
	float IsoValue = 0.5f;

	EMolColorScheme ColorScheme = EMolColorScheme::Element;
	FLinearColor UniformColor = FLinearColor::White;

	bool bShowWater = false;
	bool bShowHydrogen = false;

	/**
	 * Obergrenze fuer die Zellenzahl. Wird sie ueberschritten, vergroebert sich das Gitter
	 * selbsttaetig. Ohne diese Bremse braeuchte ein ausgedehnter Komplex bei feiner
	 * Aufloesung mehrere Gigabyte allein fuer das Dichtefeld.
	 */
	int64 MaxVoxels = 40 * 1000 * 1000;
};

namespace MolecularForge
{
	/**
	 * Erzeugt eine Gauss-Oberflaeche ueber der Struktur.
	 *
	 * Jedes Atom legt eine Gauss-Glocke ins Feld, die Oberflaeche ist die Flaeche gleicher
	 * Dichte. Das ergibt die weich verschmolzene Huelle, die man aus Abbildungen kennt, und
	 * ist der Weg, den auch die etablierten Echtzeitwerkzeuge gehen.
	 *
	 * Was das ausdruecklich *nicht* ist: die solvent-excluded surface nach Connolly. Die
	 * verlangt ein anderes Verfahren mit abrollender Sonde und hat an Engstellen sattelfoermige
	 * Flaechen, die eine Dichtefeld-Naeherung nicht hergibt. Fuer Anschauung, Silhouette und
	 * Bildwirkung ist der Unterschied nicht zu sehen; wer Oberflaechen *ausmessen* will,
	 * braucht ein anderes Werkzeug, und dafuer ist eine Spiel-Engine ohnehin der falsche Ort.
	 *
	 * Aufwandsverteilung: das Fuellen des Dichtefeldes ist der teure Teil und laeuft parallel.
	 * Es ist bewusst als Sammeln je Zelle gebaut und nicht als Streuen je Atom — beim Sammeln
	 * schreibt jeder Thread nur in seine eigenen Zellen, und es braucht weder Sperren noch
	 * atomare Operationen. Das Durchlaufen des Gitters danach ist sequenziell, weil die
	 * Eckpunkte zwischen Nachbarzellen geteilt werden; das ist der billigere Teil.
	 *
	 * @param OutError	Optional. Bekommt bei Misserfolg einen Satz zur Anzeige.
	 * @return			False, wenn keine Oberflaeche entstehen konnte.
	 */
	MOLECULARFORGERENDER_API bool BuildGaussianSurface(
		const UMolecularStructure& Structure,
		const FMolSurfaceOptions& Options,
		FMolMeshData& OutMesh,
		FString* OutError = nullptr);

	/**
	 * Abstand, in dem die Oberflaeche um ein einzelnes Atom mit Radius R zu liegen kommt.
	 *
	 * Analytisch aus der Dichteformel: exp(B*(d²/R²-1)) = Iso liefert d = R * sqrt(1 + ln(Iso)/B).
	 * Steht hier, weil sich damit die gesamte Kette aus Dichtefeld, Gitterdurchlauf und
	 * Kanteninterpolation gegen einen bekannten Wert pruefen laesst statt gegen Augenmass.
	 */
	MOLECULARFORGERENDER_API float GetSurfaceRadiusForAtom(float AtomRadius, float Blobbiness, float IsoValue);
}
