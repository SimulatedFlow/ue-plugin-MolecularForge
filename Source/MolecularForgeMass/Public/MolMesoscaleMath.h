// Copyright Simulated Flow. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MolMesoscaleMath.generated.h"

/** Was mit einem Molekuel geschieht, das den erlaubten Raum verlaesst. */
UENUM(BlueprintType)
enum class EMolBoundaryMode : uint8
{
	/** An der Wand abprallen. Erhaelt die Teilchenzahl und sieht nach Behaelter aus. */
	Reflect		UMETA(DisplayName = "Reflektieren"),
	/**
	 * Auf der Gegenseite wieder hereinkommen. Das ist es, was Simulationen tun —
	 * ein kleiner Ausschnitt soll sich verhalten wie ein Teil von etwas Grossem.
	 * Im Bild sieht es allerdings nach Zauberei aus, wenn ein Molekuel verschwindet
	 * und anderswo auftaucht.
	 */
	Wrap		UMETA(DisplayName = "Umlaufen"),
	/** Nichts tun — die Wolke darf auseinanderlaufen. */
	None		UMETA(DisplayName = "Ohne Grenze")
};

/** Darstellungsstufe eines Molekuels in der mesoskopischen Ansicht. */
UENUM(BlueprintType)
enum class EMolMesoDetail : uint8
{
	/** Nah: alle Atome. */
	Full		UMETA(DisplayName = "Volle Atome"),
	/** Mittel: nur das Rueckgrat. */
	Backbone	UMETA(DisplayName = "Rueckgrat"),
	/** Fern: eine einzelne Kugel in der Groesse des Molekuels. */
	Blob		UMETA(DisplayName = "Kugel"),
	/** Zu weit weg, um noch etwas beizutragen. */
	Hidden		UMETA(DisplayName = "Ausgeblendet")
};

namespace MolecularForge
{
	/**
	 * Ein Diffusionsschritt nach der brownschen Bewegung.
	 *
	 * Die mittlere quadratische Auslenkung je Achse ist 2*D*dt — das ist die
	 * Einstein-Relation und kein frei gewaehlter Faktor. Deshalb wird die
	 * Standardabweichung als sqrt(2*D*dt) angesetzt und nicht als irgendein Wert,
	 * der "gut aussieht": mit der richtigen Beziehung laesst sich ein
	 * Diffusionskoeffizient aus der Literatur einsetzen und das Ergebnis stimmt.
	 *
	 * @param Stream				Zufallsquelle. Mit festem Startwert wird der Lauf wiederholbar.
	 * @param DiffusionCoefficient	D in Angstroem^2 je Sekunde.
	 * @param DeltaSeconds			Vergangene Zeit.
	 * @return						Auslenkung in Angstroem.
	 */
	MOLECULARFORGEMASS_API FVector3f ComputeBrownianStep(
		FRandomStream& Stream, float DiffusionCoefficient, float DeltaSeconds);

	/**
	 * Haelt eine Position im erlaubten Raum.
	 *
	 * Beim Reflektieren wird auch die Geschwindigkeit gespiegelt — ohne das klebte das
	 * Molekuel an der Wand, weil es im naechsten Bild sofort wieder hinausdrueckte.
	 *
	 * @param InOutPosition	Wird bei Bedarf zurechtgerueckt.
	 * @param InOutVelocity	Wird beim Reflektieren mitgespiegelt.
	 * @return				True, wenn etwas veraendert wurde.
	 */
	MOLECULARFORGEMASS_API bool ConfineToBounds(
		const FBox3f& Bounds, EMolBoundaryMode Mode,
		FVector3f& InOutPosition, FVector3f& InOutVelocity);

	/**
	 * Waehlt die Darstellungsstufe nach dem Abstand zur Kamera.
	 *
	 * Die Schwellen werden quadriert verglichen, damit im heissen Pfad keine Wurzel
	 * gezogen werden muss — bei zehntausenden Molekuelen je Bild zaehlt das.
	 */
	MOLECULARFORGEMASS_API EMolMesoDetail ComputeDetailLevel(
		float DistanceSquared,
		float FullDetailDistance,
		float BackboneDistance,
		float BlobDistance);

	/**
	 * Entscheidet, ob zwei Molekuele in Reichweite als gebunden gelten.
	 *
	 * Bewusst rein geometrisch mit einer Wahrscheinlichkeit und ohne Energiebetrachtung:
	 * echte Bindungskinetik braucht Kraftfelder und Zeitschritte im Femtosekundenbereich.
	 * Was hier entsteht, ist ein glaubwuerdiges Bild von Andocken und Loesen — nicht
	 * mehr, und das steht auch so in der Doku.
	 *
	 * @param DistanceSquared	Quadrierter Abstand der Mittelpunkte.
	 * @param ContactRadiusSum	Summe der beiden Kontaktradien.
	 * @param BindProbability	Wahrscheinlichkeit je Sekunde, dass eine Begegnung haelt.
	 * @param DeltaSeconds		Vergangene Zeit.
	 * @param Roll				Zufallszahl aus [0,1).
	 */
	MOLECULARFORGEMASS_API bool ShouldBind(
		float DistanceSquared, float ContactRadiusSum,
		float BindProbability, float DeltaSeconds, float Roll);

	/** Gegenstueck: ob eine bestehende Bindung sich in diesem Bild loest. */
	MOLECULARFORGEMASS_API bool ShouldUnbind(float UnbindProbability, float DeltaSeconds, float Roll);
}
