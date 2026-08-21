// Copyright 2026 Simulated Flow All Rights Reserved.

#include "MolMesoscaleMath.h"

namespace
{
	/**
	 * Standardnormalverteilte Zufallszahl nach Box-Muller.
	 *
	 * FRandomStream liefert nur Gleichverteilung. Eine Summe mehrerer gleichverteilter
	 * Zahlen waere naeherungsweise normalverteilt, haette aber abgeschnittene Auslaeufer —
	 * gerade die seltenen weiten Spruenge fehlten dann, und die sind bei Diffusion das,
	 * was den Stoff im Raum verteilt. Box-Muller ist exakt und kostet eine Wurzel.
	 */
	float DrawStandardNormal(FRandomStream& Stream)
	{
		// Die Null muss ausgeschlossen bleiben, sonst gibt der Logarithmus unendlich.
		const float U1 = FMath::Max(Stream.GetFraction(), UE_SMALL_NUMBER);
		const float U2 = Stream.GetFraction();

		return FMath::Sqrt(-2.f * FMath::Loge(U1)) * FMath::Cos(2.f * PI * U2);
	}
}

namespace MolecularForge
{
	FVector3f ComputeBrownianStep(FRandomStream& Stream, float DiffusionCoefficient, float DeltaSeconds)
	{
		if (DiffusionCoefficient <= 0.f || DeltaSeconds <= 0.f)
		{
			return FVector3f::ZeroVector;
		}

		// Einstein-Relation: mittlere quadratische Auslenkung je Achse ist 2*D*dt.
		const float StandardDeviation = FMath::Sqrt(2.f * DiffusionCoefficient * DeltaSeconds);

		return FVector3f(
			DrawStandardNormal(Stream) * StandardDeviation,
			DrawStandardNormal(Stream) * StandardDeviation,
			DrawStandardNormal(Stream) * StandardDeviation);
	}

	bool ConfineToBounds(const FBox3f& Bounds, EMolBoundaryMode Mode,
		FVector3f& InOutPosition, FVector3f& InOutVelocity)
	{
		if (Mode == EMolBoundaryMode::None || !Bounds.IsValid)
		{
			return false;
		}

		const FVector3f Size = Bounds.Max - Bounds.Min;
		if (Size.GetMin() <= UE_SMALL_NUMBER)
		{
			return false;
		}

		bool bChanged = false;

		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			const float Min = Bounds.Min[Axis];
			const float Max = Bounds.Max[Axis];
			const float Extent = Size[Axis];

			float& Value = InOutPosition[Axis];

			if (Value >= Min && Value <= Max)
			{
				continue;
			}

			bChanged = true;

			if (Mode == EMolBoundaryMode::Wrap)
			{
				// Modulo statt einfachem Verschieben: ein grosser Zeitschritt kann ein
				// Molekuel mehr als eine Boxlaenge weit tragen, und dann reichte ein
				// einzelnes Addieren nicht.
				Value = Min + FMath::Fmod(FMath::Fmod(Value - Min, Extent) + Extent, Extent);
				continue;
			}

			// Reflektieren: an der ueberschrittenen Wand spiegeln, notfalls mehrfach.
			// Die Schleife ist gedeckelt, damit ein unsinniger Wert — etwa nach einem
			// Sprung ins Unendliche — die Bildberechnung nicht anhaelt.
			for (int32 Guard = 0; Guard < 8 && (Value < Min || Value > Max); ++Guard)
			{
				if (Value < Min)
				{
					Value = Min + (Min - Value);
					InOutVelocity[Axis] = FMath::Abs(InOutVelocity[Axis]);
				}
				else
				{
					Value = Max - (Value - Max);
					InOutVelocity[Axis] = -FMath::Abs(InOutVelocity[Axis]);
				}
			}

			Value = FMath::Clamp(Value, Min, Max);
		}

		return bChanged;
	}

	EMolMesoDetail ComputeDetailLevel(float DistanceSquared,
		float FullDetailDistance, float BackboneDistance, float BlobDistance)
	{
		// Die Schwellen muessen aufsteigend sein, sonst waere eine Stufe unerreichbar.
		// Statt das dem Aufrufer zu ueberlassen, wird hier sortiert weitergerechnet.
		const float Full = FMath::Max(FullDetailDistance, 0.f);
		const float Backbone = FMath::Max(BackboneDistance, Full);
		const float Blob = FMath::Max(BlobDistance, Backbone);

		if (DistanceSquared <= Full * Full)		{ return EMolMesoDetail::Full; }
		if (DistanceSquared <= Backbone * Backbone)	{ return EMolMesoDetail::Backbone; }
		if (DistanceSquared <= Blob * Blob)		{ return EMolMesoDetail::Blob; }
		return EMolMesoDetail::Hidden;
	}

	bool ShouldBind(float DistanceSquared, float ContactRadiusSum,
		float BindProbability, float DeltaSeconds, float Roll)
	{
		if (ContactRadiusSum <= 0.f || BindProbability <= 0.f || DeltaSeconds <= 0.f)
		{
			return false;
		}

		if (DistanceSquared > ContactRadiusSum * ContactRadiusSum)
		{
			return false;
		}

		// Die Wahrscheinlichkeit gilt je Sekunde, gefragt ist sie fuer diesen Zeitschritt.
		// Der Weg ueber die Gegenwahrscheinlichkeit haelt das Ergebnis auch bei grossen
		// Schritten unter eins — schlichtes Multiplizieren mit dt ergaebe bei dt > 1/p
		// Werte ueber eins, und dann bindet alles sofort.
		const float StepProbability = 1.f - FMath::Exp(-BindProbability * DeltaSeconds);
		return Roll < StepProbability;
	}

	bool ShouldUnbind(float UnbindProbability, float DeltaSeconds, float Roll)
	{
		if (UnbindProbability <= 0.f || DeltaSeconds <= 0.f)
		{
			return false;
		}

		const float StepProbability = 1.f - FMath::Exp(-UnbindProbability * DeltaSeconds);
		return Roll < StepProbability;
	}
}
