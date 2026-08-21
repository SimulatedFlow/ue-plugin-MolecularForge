// Copyright 2026 Simulated Flow All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MolecularForgeTypes.h"

class UMolecularStructure;

/**
 * Ein Punkt auf dem geglaetteten Rueckgrat, mit vollstaendigem Koordinatensystem.
 * Alle Vektoren sind normiert und stehen paarweise senkrecht aufeinander.
 * Positionen in Angstroem, wie ueberall im Plugin bis zur Darstellung.
 */
struct FMolBackbonePoint
{
	FVector3f Position = FVector3f::ZeroVector;

	/** Laufrichtung der Kette. */
	FVector3f Forward = FVector3f::XAxisVector;

	/** Querrichtung — die Breite eines Bandes liegt auf dieser Achse. */
	FVector3f Right = FVector3f::YAxisVector;

	/** Flaechennormale des Bandes. */
	FVector3f Up = FVector3f::ZAxisVector;

	/** Sekundaerstruktur des naechstgelegenen Residuums. */
	EMolSecondaryStructure SecondaryStructure = EMolSecondaryStructure::Coil;

	/** Index in UMolecularStructure::Residues. */
	int32 ResidueIndex = INDEX_NONE;

	/**
	 * Ankeratom des naechstgelegenen Residuums (CA bzw. C1').
	 * Wird zum Einfaerben gebraucht: die Faerbeschemata arbeiten auf Atomen, ein
	 * Bandpunkt braucht aber eine Farbe. Ueber den Anker bekommt er dieselbe wie
	 * die Kugeldarstellung, sodass beide Darstellungen zusammenpassen.
	 */
	int32 AnchorAtomIndex = INDEX_NONE;

	/** Fortschritt entlang dieses Abschnitts, 0 bis 1. */
	float Alpha = 0.f;
};

/**
 * Ein zusammenhaengendes Stueck Rueckgrat.
 *
 * Eine Kette zerfaellt in mehrere Abschnitte, sobald in der Struktur Residuen fehlen —
 * in Kristallstrukturen sind bewegliche Schleifen regelmaessig nicht aufgeloest. Ueber
 * so eine Luecke hinweg zu interpolieren waere schlicht erfunden: das Band wuerde einen
 * Weg zeigen, den niemand gemessen hat.
 */
struct FMolBackboneSegment
{
	int32 ChainIndex = INDEX_NONE;
	TArray<FMolBackbonePoint> Points;
};

struct FMolBackboneOptions
{
	/** Stuetzpunkte je Residuum. Hoeher heisst glatter und teurer. */
	int32 SegmentsPerResidue = 6;

	/**
	 * Groesster Abstand zwischen zwei Ankeratomen, bei dem sie noch als verbunden gelten.
	 * Zwei aufeinanderfolgende CA-Atome liegen rund 3,8 A auseinander; alles deutlich
	 * darueber ist eine Luecke in der Struktur, keine lange Bindung.
	 */
	float MaxAnchorGapAngstrom = 5.f;
};

namespace MolecularForge
{
	/**
	 * Legt eine geglaettete Kurve durch die Ankeratome (CA bei Proteinen, C1' bei
	 * Nukleinsaeuren) und berechnet fuer jeden Punkt ein vollstaendiges Koordinatensystem.
	 *
	 * Die Querrichtung ist der heikle Teil. Sie darf nicht aus einem festen Hochvektor
	 * kommen, sonst dreht sich das Band an jeder Kurve mit — sie folgt stattdessen der
	 * Carbonylgruppe des Residuums, so wie es die etablierten Cartoon-Darstellungen tun.
	 * Damit liegt die Bandflaeche dort, wo auch die Peptidebene liegt.
	 *
	 * Dabei muss eine Eigenheit abgefangen werden: in einem Faltblatt zeigen die
	 * Carbonylgruppen abwechselnd nach oben und unten. Uebernaehme man sie ungeprueft,
	 * kippte das Band bei jedem Residuum um 180 Grad und saehe aus wie eine Papierschlange.
	 * Deshalb wird jede Querrichtung, die gegen ihre Vorgaengerin zeigt, umgedreht.
	 *
	 * Interpoliert wird mit Catmull-Rom: die Kurve laeuft genau durch die Ankeratome und
	 * nicht daneben — bei gemessenen Positionen ist das die richtige Wahl.
	 */
	MOLECULARFORGERENDER_API void BuildBackboneSegments(
		const UMolecularStructure& Structure,
		const FMolBackboneOptions& Options,
		TArray<FMolBackboneSegment>& OutSegments);
}
