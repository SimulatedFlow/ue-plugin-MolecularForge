// Copyright Simulated Flow. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MolecularForgeTypes.h"

class UMolecularStructure;

namespace MolecularForge
{
	/**
	 * Berechnet die Sekundaerstruktur nach dem Verfahren von Kabsch und Sander (DSSP, 1983)
	 * und schreibt sie in `UMolecularStructure::Residues`. Vorhandene Zuordnungen werden ersetzt.
	 *
	 * Warum ueberhaupt rechnen, wo beide Dateiformate Sekundaerstruktur mitliefern koennen:
	 * AlphaFold-Dateien tun es nicht. Ohne eigene Berechnung bliebe ausgerechnet die Quelle
	 * mit rund 200 Millionen Strukturen durchgehend als "Coil" eingefaerbt — und damit waere
	 * die Cartoon-Darstellung dort wertlos.
	 *
	 * Das Verfahren in Kuerze:
	 *  1. Fuer jedes Residuum wird die Position des Amid-Wasserstoffs geschaetzt. Kristall-
	 *     strukturen enthalten selten Wasserstoff, aber seine Lage ist aus dem Rueckgrat
	 *     bestimmt: er sitzt am Stickstoff, entgegen der C=O-Richtung des Vorgaengers.
	 *  2. Fuer Paare in Reichweite wird die elektrostatische Energie der Wasserstoffbruecke
	 *     berechnet. Unter -0,5 kcal/mol gilt sie als vorhanden.
	 *  3. Aus dem Muster der Bruecken ergeben sich Helices (zwei aufeinanderfolgende n-Turns)
	 *     und Faltblaetter (Bruecken zwischen entfernten Residuen, parallel oder antiparallel).
	 *
	 * Was bewusst vereinfacht ist: DSSP unterscheidet acht Zustaende (H, B, E, G, I, T, S, -).
	 * Hier werden sie auf die vier zusammengefasst, die eine Darstellung braucht — 3-10- und
	 * Pi-Helix gelten als Helix, Bruecke und Faltblatt als Faltblatt. Fuer Bild und Faerbung
	 * ist das die richtige Aufloesung; wer die acht Zustaende braucht, braucht ohnehin DSSP
	 * selbst und keine Spiel-Engine.
	 *
	 * Die teure Stufe — der Energielauf ueber die Nachbarpaare — ist auf Threads verteilt.
	 * Jedes Residuum schreibt nur in seine eigene Bruecken-Liste, deshalb ohne Sperren.
	 */
	MOLECULARFORGERUNTIME_API void ComputeSecondaryStructure(UMolecularStructure& Structure);

	/**
	 * Wendet die Ladeoption an: je nachdem wird die Angabe aus der Datei behalten,
	 * verworfen und neu gerechnet, oder nur dann gerechnet, wenn die Datei nichts hergab.
	 *
	 * @param bFileHadAnnotations	Ob die Datei HELIX/SHEET bzw. struct_conf enthielt.
	 */
	MOLECULARFORGERUNTIME_API void ApplySecondaryStructurePolicy(
		UMolecularStructure& Structure,
		const FMolLoadOptions& Options,
		bool bFileHadAnnotations);
}
