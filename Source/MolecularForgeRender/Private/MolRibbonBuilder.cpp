// Copyright 2026 Simulated Flow All Rights Reserved.

#include "MolRibbonBuilder.h"
#include "MolecularStructure.h"

namespace
{
	/** Masse eines Querschnitts an einem Punkt des Bandes. */
	struct FMolProfile
	{
		float HalfWidth = 0.25f;
		float HalfThickness = 0.25f;

		/**
		 * 2 ergibt eine Ellipse, groessere Werte ein zunehmend rechteckiges Profil.
		 * Schleifen sollen rund sein, Baender sollen Kanten haben.
		 */
		float Squareness = 2.f;
	};

	/**
	 * Punkt auf einer Superellipse.
	 * Bei Squareness 2 ist das exakt eine Ellipse; groessere Werte druecken die Form zum
	 * Rechteck. Damit deckt eine einzige Formel Rundprofil und Flachband ab, und — wichtiger —
	 * beide lassen sich ineinander ueberblenden, ohne die Punktzahl zu aendern.
	 */
	FVector2f ProfilePoint(float Angle, const FMolProfile& Profile)
	{
		const float C = FMath::Cos(Angle);
		const float S = FMath::Sin(Angle);
		const float Exponent = 2.f / FMath::Max(Profile.Squareness, 2.f);

		const float X = FMath::Sign(C) * FMath::Pow(FMath::Abs(C), Exponent) * Profile.HalfWidth;
		const float Y = FMath::Sign(S) * FMath::Pow(FMath::Abs(S), Exponent) * Profile.HalfThickness;

		return FVector2f(X, Y);
	}

	FMolProfile ProfileForSecondaryStructure(EMolSecondaryStructure Kind, const FMolRibbonOptions& Options)
	{
		FMolProfile Profile;

		switch (Kind)
		{
		case EMolSecondaryStructure::Helix:
			Profile.HalfWidth = Options.HelixHalfWidth;
			Profile.HalfThickness = Options.HelixHalfThickness;
			Profile.Squareness = 5.f;
			break;

		case EMolSecondaryStructure::Sheet:
			Profile.HalfWidth = Options.SheetHalfWidth;
			Profile.HalfThickness = Options.SheetHalfThickness;
			Profile.Squareness = 6.f;
			break;

		case EMolSecondaryStructure::Turn:
			Profile.HalfWidth = Options.TurnRadius;
			Profile.HalfThickness = Options.TurnRadius;
			Profile.Squareness = 2.f;
			break;

		case EMolSecondaryStructure::Coil:
		default:
			Profile.HalfWidth = Options.CoilRadius;
			Profile.HalfThickness = Options.CoilRadius;
			Profile.Squareness = 2.f;
			break;
		}

		return Profile;
	}

	/**
	 * Glaettet die Profilmasse entlang des Abschnitts.
	 *
	 * Ohne das saesse an jeder Grenze zwischen zwei Sekundaerstrukturen eine Stufe im Mesh:
	 * ein Rundprofil von 0,25 A traefe unvermittelt auf ein Band von 1,8 A Breite. Der
	 * Uebergang ist ohnehin keine scharfe Linie — wo eine Helix endet, ist eine Auslegung
	 * und keine Messung.
	 */
	void SmoothProfiles(TArray<FMolProfile>& Profiles, int32 Radius)
	{
		if (Radius <= 0 || Profiles.Num() < 3)
		{
			return;
		}

		const TArray<FMolProfile> Source = Profiles;
		const int32 Num = Source.Num();

		for (int32 i = 0; i < Num; ++i)
		{
			float SumWidth = 0.f;
			float SumThickness = 0.f;
			float SumSquareness = 0.f;
			int32 Count = 0;

			for (int32 Offset = -Radius; Offset <= Radius; ++Offset)
			{
				const int32 Index = FMath::Clamp(i + Offset, 0, Num - 1);
				SumWidth += Source[Index].HalfWidth;
				SumThickness += Source[Index].HalfThickness;
				SumSquareness += Source[Index].Squareness;
				++Count;
			}

			Profiles[i].HalfWidth = SumWidth / Count;
			Profiles[i].HalfThickness = SumThickness / Count;
			Profiles[i].Squareness = SumSquareness / Count;
		}
	}

	/**
	 * Setzt die Pfeilspitzen auf die Enden der Faltblatt-Abschnitte.
	 *
	 * Erst nach dem Glaetten, damit der Ansatz scharf bleibt: der Pfeil springt am Anfang
	 * auf volle Breite und laeuft dann spitz zu. Genau so sieht ihn jeder, der schon einmal
	 * eine Strukturabbildung gesehen hat.
	 */
	void ApplyArrowHeads(const TArray<FMolBackbonePoint>& Points,
		TArray<FMolProfile>& Profiles, const FMolRibbonOptions& Options)
	{
		const int32 Num = Points.Num();
		const int32 ArrowLength = FMath::Max(2, Options.ArrowLengthInPoints);

		for (int32 i = 0; i < Num; ++i)
		{
			if (Points[i].SecondaryStructure != EMolSecondaryStructure::Sheet)
			{
				continue;
			}

			// Ende eines Faltblatt-Abschnitts?
			const bool bIsRunEnd = (i + 1 >= Num)
				|| Points[i + 1].SecondaryStructure != EMolSecondaryStructure::Sheet;

			if (!bIsRunEnd)
			{
				continue;
			}

			const int32 Start = FMath::Max(0, i - ArrowLength + 1);
			for (int32 k = Start; k <= i; ++k)
			{
				// Nur ueber Punkte laufen, die wirklich zum Faltblatt gehoeren — sonst
				// bekaeme eine kurze Schleife davor ebenfalls eine Pfeilform.
				if (Points[k].SecondaryStructure != EMolSecondaryStructure::Sheet)
				{
					continue;
				}

				const float Alpha = static_cast<float>(i - k) / static_cast<float>(ArrowLength - 1);
				// Alpha ist 1 am Beginn der Spitze und 0 an ihrem Ende.
				Profiles[k].HalfWidth = FMath::Lerp(0.05f, Options.ArrowHalfWidth, Alpha);
				Profiles[k].Squareness = 6.f;
			}
		}
	}

	/** Farbe eines Bandpunktes ueber sein Ankeratom. */
	FColor ColorForPoint(const UMolecularStructure& Structure,
		const FMolBackbonePoint& Point, const FMolRibbonOptions& Options)
	{
		if (!Structure.AtomPositions.IsValidIndex(Point.AnchorAtomIndex))
		{
			return FColor::White;
		}

		const FLinearColor Color =
			Structure.GetAtomColor(Point.AnchorAtomIndex, Options.ColorScheme, Options.UniformColor);

		return Color.ToFColor(/*bSRGB=*/false);
	}
}

namespace MolecularForge
{
	void BuildRibbonMesh(const UMolecularStructure& Structure,
		const TArray<FMolBackboneSegment>& Segments,
		const FMolRibbonOptions& Options,
		FMolMeshData& OutMesh)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(MolecularForge_BuildRibbonMesh);

		OutMesh.Reset();

		const int32 Ring = FMath::Clamp(Options.RingResolution, 3, 64);
		const float Scale = Options.UnitsPerAngstrom;

		for (const FMolBackboneSegment& Segment : Segments)
		{
			const TArray<FMolBackbonePoint>& Points = Segment.Points;
			if (Points.Num() < 2)
			{
				continue;
			}

			// ---- Profile bestimmen ----

			TArray<FMolProfile> Profiles;
			Profiles.Reserve(Points.Num());
			for (const FMolBackbonePoint& Point : Points)
			{
				Profiles.Add(ProfileForSecondaryStructure(Point.SecondaryStructure, Options));
			}

			SmoothProfiles(Profiles, 2);
			ApplyArrowHeads(Points, Profiles, Options);

			// ---- Mantelflaeche ----

			const int32 FirstVertex = OutMesh.Positions.Num();

			for (int32 p = 0; p < Points.Num(); ++p)
			{
				const FMolBackbonePoint& Point = Points[p];
				const FColor Color = ColorForPoint(Structure, Point, Options);

				for (int32 k = 0; k < Ring; ++k)
				{
					const float Angle = (2.f * PI * k) / Ring;
					const FVector2f Local = ProfilePoint(Angle, Profiles[p]);

					const FVector3f Position =
						(Point.Position + Point.Right * Local.X + Point.Up * Local.Y) * Scale;

					OutMesh.Positions.Add(Position);
					OutMesh.Normals.Add(FVector3f::ZeroVector);
					OutMesh.UVs.Add(FVector2f(static_cast<float>(k) / Ring, Point.Alpha));
					OutMesh.Colors.Add(Color);
				}
			}

			for (int32 p = 0; p + 1 < Points.Num(); ++p)
			{
				const int32 RowA = FirstVertex + p * Ring;
				const int32 RowB = FirstVertex + (p + 1) * Ring;

				for (int32 k = 0; k < Ring; ++k)
				{
					const int32 Next = (k + 1) % Ring;

					const int32 A = RowA + k;
					const int32 B = RowA + Next;
					const int32 C = RowB + k;
					const int32 D = RowB + Next;

					// Reihenfolge so gewaehlt, dass die Normale nach aussen zeigt.
					// Ueberprueft wird das im Test, nicht per Augenmass — die erste
					// Fassung zeigte durchgehend nach innen, und im Bild waere das
					// erst als schwarzes oder unsichtbares Band aufgefallen.
					OutMesh.Triangles.Add(A);
					OutMesh.Triangles.Add(B);
					OutMesh.Triangles.Add(C);

					OutMesh.Triangles.Add(B);
					OutMesh.Triangles.Add(D);
					OutMesh.Triangles.Add(C);
				}
			}

			// ---- Normalen aus den Flaechen mitteln ----
			// Analytisch waere das fuer eine Superellipse muehsam; ueber die erzeugten
			// Dreiecke ist es kurz und stimmt auch dann noch, wenn sich das Profil aendert.

			{
				const int32 LastVertex = OutMesh.Positions.Num();
				const int32 FirstTriangle = OutMesh.Triangles.Num() - (Points.Num() - 1) * Ring * 6;

				for (int32 t = FirstTriangle; t < OutMesh.Triangles.Num(); t += 3)
				{
					const int32 I0 = OutMesh.Triangles[t];
					const int32 I1 = OutMesh.Triangles[t + 1];
					const int32 I2 = OutMesh.Triangles[t + 2];

					const FVector3f Edge1 = OutMesh.Positions[I1] - OutMesh.Positions[I0];
					const FVector3f Edge2 = OutMesh.Positions[I2] - OutMesh.Positions[I0];
					const FVector3f FaceNormal = FVector3f::CrossProduct(Edge1, Edge2);

					OutMesh.Normals[I0] += FaceNormal;
					OutMesh.Normals[I1] += FaceNormal;
					OutMesh.Normals[I2] += FaceNormal;
				}

				for (int32 v = FirstVertex; v < LastVertex; ++v)
				{
					OutMesh.Normals[v] = OutMesh.Normals[v].GetSafeNormal();

					if (OutMesh.Normals[v].IsNearlyZero())
					{
						// Kann bei entarteten Profilen vorkommen — dann tut es die
						// Querrichtung, die immer definiert ist.
						const int32 PointIndex = (v - FirstVertex) / Ring;
						OutMesh.Normals[v] = Points[PointIndex].Right;
					}
				}
			}

			// ---- Deckel ----
			// Eigene Vertices, damit die Kante zum Mantel scharf bleibt und die Deckelnormale
			// nicht in die Mantelnormalen einfliesst.

			if (Options.bGenerateCaps)
			{
				auto AddCap = [&](int32 PointIndex, bool bAtStart)
				{
					const FMolBackbonePoint& Point = Points[PointIndex];
					const FVector3f Normal = bAtStart ? -Point.Forward : Point.Forward;
					const FColor Color = ColorForPoint(Structure, Point, Options);

					const int32 CenterIndex = OutMesh.Positions.Num();
					OutMesh.Positions.Add(Point.Position * Scale);
					OutMesh.Normals.Add(Normal);
					OutMesh.UVs.Add(FVector2f(0.5f, 0.5f));
					OutMesh.Colors.Add(Color);

					const int32 RimStart = OutMesh.Positions.Num();
					for (int32 k = 0; k < Ring; ++k)
					{
						const float Angle = (2.f * PI * k) / Ring;
						const FVector2f Local = ProfilePoint(Angle, Profiles[PointIndex]);

						OutMesh.Positions.Add(
							(Point.Position + Point.Right * Local.X + Point.Up * Local.Y) * Scale);
						OutMesh.Normals.Add(Normal);
						OutMesh.UVs.Add(FVector2f(static_cast<float>(k) / Ring, bAtStart ? 0.f : 1.f));
						OutMesh.Colors.Add(Color);
					}

					for (int32 k = 0; k < Ring; ++k)
					{
						const int32 Next = (k + 1) % Ring;

						// Am Anfang zeigt der Deckel entgegen der Laufrichtung, am Ende mit
						// ihr — deshalb muss die Umlaufrichtung sich unterscheiden.
						if (bAtStart)
						{
							OutMesh.Triangles.Add(CenterIndex);
							OutMesh.Triangles.Add(RimStart + Next);
							OutMesh.Triangles.Add(RimStart + k);
						}
						else
						{
							OutMesh.Triangles.Add(CenterIndex);
							OutMesh.Triangles.Add(RimStart + k);
							OutMesh.Triangles.Add(RimStart + Next);
						}
					}
				};

				AddCap(0, /*bAtStart=*/true);
				AddCap(Points.Num() - 1, /*bAtStart=*/false);
			}
		}

		UE_LOG(LogMolecularForge, Verbose, TEXT("Band erzeugt: %d Vertices, %d Dreiecke aus %d Abschnitten."),
			OutMesh.NumVertices(), OutMesh.NumTriangles(), Segments.Num());
	}
}
