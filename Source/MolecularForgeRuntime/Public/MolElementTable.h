// Copyright 2026 Simulated Flow All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Statische Stammdaten eines chemischen Elements.
 * Bewusst POD ohne UObject-Bezug, damit der Parser das aus mehreren Threads lesen darf.
 */
struct FMolElementInfo
{
	/** Elementsymbol in kanonischer Schreibweise, z.B. "Fe". */
	const TCHAR* Symbol;

	const TCHAR* Name;

	/** Van-der-Waals-Radius in Angstroem. Bestimmt die Kugelgroesse in der Space-Filling-Darstellung. */
	float VdWRadius;

	/** Kovalenter Radius in Angstroem (Cordero et al.). Grundlage der abstandsbasierten Bindungsableitung. */
	float CovalentRadius;

	/** CPK-/Jmol-Farbe, die in der Strukturbiologie als Standard gilt. */
	uint8 ColorR;
	uint8 ColorG;
	uint8 ColorB;

	FColor GetCpkColor() const { return FColor(ColorR, ColorG, ColorB, 255); }
	FLinearColor GetCpkLinearColor() const { return FLinearColor(FColor(ColorR, ColorG, ColorB, 255)); }
};

namespace MolecularForge
{
	/** Hoechste unterstuetzte Ordnungszahl. Index 0 ist der Unbekannt-Eintrag. */
	inline constexpr uint8 MaxAtomicNumber = 118;

	/**
	 * Stammdaten zu einer Ordnungszahl. Bei ungueltigem Wert kommt der Unbekannt-Eintrag
	 * zurueck, nie eine ungueltige Referenz — der Parser darf also blind zugreifen.
	 */
	MOLECULARFORGERUNTIME_API const FMolElementInfo& GetElement(uint8 AtomicNumber);

	/**
	 * Ordnungszahl aus einem Symbol. Gross-/Kleinschreibung und fuehrende Leerzeichen
	 * sind egal, PDB schreibt das Feld rechtsbuendig (" C", "FE").
	 * Ergebnis 0 heisst "nicht erkannt".
	 */
	MOLECULARFORGERUNTIME_API uint8 AtomicNumberFromSymbol(FStringView Symbol);

	/**
	 * Notfallpfad fuer Dateien ohne Elementspalte (kommt bei aelteren PDB-Dateien vor).
	 * Leitet das Element aus dem Atomnamen ab, z.B. " CA " -> Kohlenstoff, "CA  " -> Calcium.
	 * Die Spaltenposition ist dabei die entscheidende Information, deshalb wird der
	 * Name ungetrimmt mit voller Breite von 4 Zeichen erwartet.
	 */
	MOLECULARFORGERUNTIME_API uint8 GuessAtomicNumberFromAtomName(FStringView PaddedAtomName, bool bIsHetatm);

	/**
	 * Mittlere Atommasse in atomaren Masseneinheiten (Standardatomgewicht).
	 *
	 * Steht bewusst in einer eigenen Tabelle und nicht als Feld in FMolElementInfo: die
	 * Masse wird nur fuer Schwerpunktrechnungen gebraucht, waehrend Radien und Farben in
	 * jeder Darstellung vorkommen. Sie dort einzureihen wuerde die Struktur vergroessern,
	 * die im heissen Pfad der Bindungsableitung millionenfach gelesen wird.
	 *
	 * Bei unbekannter Ordnungszahl kommt 0 zurueck.
	 */
	MOLECULARFORGERUNTIME_API float GetAtomicMass(uint8 AtomicNumber);
}
