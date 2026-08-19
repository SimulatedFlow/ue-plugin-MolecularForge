// Copyright Simulated Flow. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UMolecularStructure;

namespace MolecularForge
{
	/**
	 * Leitet kovalente Bindungen aus den Atomabstaenden ab und schreibt sie nach
	 * `UMolecularStructure::Bonds`. Vorhandene Bindungen werden ersetzt.
	 *
	 * Warum abstandsbasiert und nicht ueber Residuen-Templates: Templates sind fuer die
	 * 20 Standardaminosaeuren schneller und exakter, decken aber genau die Faelle nicht ab,
	 * die im Bild auffallen — Liganden, Kofaktoren, Metallzentren, modifizierte Residuen.
	 * Und ein Ligand ist meistens der Grund, warum jemand sich die Struktur ueberhaupt ansieht.
	 *
	 * Zwei Atome gelten als gebunden, wenn ihr Abstand kleiner ist als die Summe ihrer
	 * kovalenten Radien plus einer Toleranz von 0,45 A. Der Nachbarschaftstest laeuft ueber
	 * ein Uniform-Grid: Zellkante etwas groesser als die groesste moegliche Bindungslaenge,
	 * dann reicht je Atom der Blick in die 27 umliegenden Zellen. Das macht aus O(n^2) ein
	 * O(n) — bei 150.000 Atomen der Unterschied zwischen Minuten und Millisekunden.
	 *
	 * Der Paarungslauf ist auf Threads verteilt, jeder Thread sammelt in seine eigene Liste;
	 * zusammengehaengt wird erst am Ende. Damit gibt es keine geteilten Schreibziele.
	 *
	 * Bewusst nicht gebunden werden einatomige Residuen — freie Ionen (Na+, Zn2+, Cl-)
	 * liegen oft im kovalenten Abstand zu ihrer Koordinationsumgebung, sind aber eben
	 * nicht kovalent gebunden. Sie wuerden das Bild mit falschen Staeben zuziehen.
	 */
	MOLECULARFORGERUNTIME_API void BuildBondsByDistance(UMolecularStructure& Structure);
}
