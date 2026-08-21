// Copyright 2026 Simulated Flow All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UMolecularStructure;

namespace MolecularForge
{
	/**
	 * Messungen an einer Struktur. Alle Laengen in Angstroem, alle Winkel in Grad.
	 *
	 * Die Einheit ist Absicht: in der Strukturbiologie wird in Angstroem gerechnet, und ein
	 * Werkzeug, das Bindungslaengen in Unreal-Einheiten ausgibt, waere fuer die Zielgruppe
	 * unbrauchbar. Umgerechnet wird erst bei der Darstellung.
	 */

	/** Abstand zweier Atome. Negativ, wenn ein Index nicht stimmt. */
	MOLECULARFORGERUNTIME_API float MeasureDistance(const UMolecularStructure& Structure, int32 AtomA, int32 AtomB);

	/** Bindungswinkel A-B-C mit dem Scheitel bei B, in Grad. Negativ bei ungueltigen Indizes. */
	MOLECULARFORGERUNTIME_API float MeasureAngle(
		const UMolecularStructure& Structure, int32 AtomA, int32 AtomB, int32 AtomC);

	/**
	 * Torsionswinkel A-B-C-D in Grad, im Bereich -180 bis 180.
	 *
	 * Das ist die Groesse, die die Faltung eines Proteins beschreibt: phi, psi und die
	 * Seitenkettenwinkel sind alle Torsionen. Das Vorzeichen folgt der IUPAC-Regel —
	 * von B nach C geblickt ist eine Drehung im Uhrzeigersinn positiv.
	 */
	MOLECULARFORGERUNTIME_API float MeasureDihedral(
		const UMolecularStructure& Structure, int32 AtomA, int32 AtomB, int32 AtomC, int32 AtomD);

	/** Geometrischer Mittelpunkt der ausgewaehlten Atome. */
	MOLECULARFORGERUNTIME_API FVector ComputeCentroid(
		const UMolecularStructure& Structure, const TBitArray<>& Mask);

	/**
	 * Massenschwerpunkt der ausgewaehlten Atome.
	 * Unterscheidet sich vom geometrischen Mittelpunkt vor allem dort, wo schwere Atome
	 * sitzen — bei einem Eisen-Schwefel-Zentrum liegen die beiden merklich auseinander.
	 */
	MOLECULARFORGERUNTIME_API FVector ComputeCenterOfMass(
		const UMolecularStructure& Structure, const TBitArray<>& Mask);

	/**
	 * Traegheitsradius der Auswahl in Angstroem.
	 * Ein Mass fuer die Ausdehnung: ein kompakt gefaltetes Protein hat einen deutlich
	 * kleineren als dasselbe Protein entfaltet. Damit laesst sich eine Faltungssimulation
	 * in einer einzigen Zahl verfolgen.
	 */
	MOLECULARFORGERUNTIME_API float ComputeRadiusOfGyration(
		const UMolecularStructure& Structure, const TBitArray<>& Mask);

	/** Achsenparallele Huelle der Auswahl in Angstroem. */
	MOLECULARFORGERUNTIME_API FBox ComputeSelectionBounds(
		const UMolecularStructure& Structure, const TBitArray<>& Mask);

	/** Gesamtmasse der Auswahl in atomaren Masseneinheiten. */
	MOLECULARFORGERUNTIME_API float ComputeTotalMass(
		const UMolecularStructure& Structure, const TBitArray<>& Mask);

	/**
	 * Mittlere quadratische Abweichung zweier Koordinatensaetze, in Angstroem.
	 *
	 * Vergleicht die Positionen so, wie sie dastehen — es wird *nicht* vorher optimal
	 * uebereinandergelegt. Fuer den Vergleich zweier Bilder derselben Trajektorie ist das
	 * richtig, weil sie ohnehin im selben Bezugssystem liegen. Wer zwei getrennt bestimmte
	 * Strukturen vergleicht, muss sie erst zur Deckung bringen, sonst misst er im
	 * Wesentlichen den Abstand ihrer Schwerpunkte.
	 *
	 * @return False, wenn die Saetze verschieden lang oder leer sind.
	 */
	MOLECULARFORGERUNTIME_API bool ComputeRmsd(
		TArrayView<const FVector3f> A, TArrayView<const FVector3f> B, float& OutRmsd);

	/** Wie ComputeRmsd, aber nur ueber die ausgewaehlten Atome. */
	MOLECULARFORGERUNTIME_API bool ComputeRmsdMasked(
		TArrayView<const FVector3f> A, TArrayView<const FVector3f> B,
		const TBitArray<>& Mask, float& OutRmsd);
}
