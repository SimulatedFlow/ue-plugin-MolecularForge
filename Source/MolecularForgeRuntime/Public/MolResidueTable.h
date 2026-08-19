// Copyright Simulated Flow. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** Grobe Einordnung eines Residuums anhand seines Namens. */
enum class EMolResidueClass : uint8
{
	Other,
	AminoAcid,
	DeoxyNucleotide,
	Nucleotide,
	Water
};

namespace MolecularForge
{
	/**
	 * Ordnet einen Residuennamen ("ALA", "DG", "HOH", "ATP") einer Klasse zu.
	 * Erkennt auch die gaengigen Varianten aus Kraftfeld-Dateien (HSD/HSE fuer Histidin,
	 * CYX fuer verbruecktes Cystein) und Selenomethionin (MSE), das formal ein HETATM ist,
	 * aber Teil der Polymerkette.
	 */
	MOLECULARFORGERUNTIME_API EMolResidueClass ClassifyResidue(FName ResidueName);

	/** Einbuchstaben-Code fuer die Sequenzanzeige. 'X' wenn kein Standardresiduum. */
	MOLECULARFORGERUNTIME_API uint8 ResidueOneLetterCode(FName ResidueName);

	/**
	 * True, wenn dieser Atomname zum Polymer-Rueckgrat gehoert.
	 * Protein: N, CA, C, O. Nukleinsaeure: P, O5', C5', C4', C3', O3'.
	 * Erwartet den getrimmten Namen.
	 */
	MOLECULARFORGERUNTIME_API bool IsBackboneAtomName(FName AtomName, EMolResidueClass ResidueClass);

	/**
	 * True fuer das Atom, an dem Ribbon- und Backbone-Darstellungen aufgehaengt werden:
	 * CA bei Proteinen, C1' (ersatzweise P) bei Nukleinsaeuren.
	 */
	MOLECULARFORGERUNTIME_API bool IsAnchorAtomName(FName AtomName, EMolResidueClass ResidueClass);
}
