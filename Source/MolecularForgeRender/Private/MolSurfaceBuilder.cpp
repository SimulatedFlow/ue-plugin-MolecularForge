// Copyright 2026 Simulated Flow All Rights Reserved.

#include "MolSurfaceBuilder.h"
#include "MolecularStructure.h"
#include "MolElementTable.h"
#include "Async/ParallelFor.h"

namespace
{
	/**
	 * Bis zu welchem Vielfachen des Atomradius die Gauss-Glocke beruecksichtigt wird.
	 * Bei Blobbiness -2.3 ist ihr Beitrag jenseits des doppelten Radius kleiner als ein
	 * Promille und damit unter jeder Aufloesung, die das Gitter hergibt.
	 */
	constexpr float GInfluenceFactor = 2.5f;

	constexpr int32 GParallelThreshold = 4096;

	/** Ecken eines Gitterwuerfels, als Versatz in Zellkoordinaten. */
	const FIntVector GCubeCorners[8] =
	{
		FIntVector(0, 0, 0),	// 0
		FIntVector(1, 0, 0),	// 1
		FIntVector(1, 1, 0),	// 2
		FIntVector(0, 1, 0),	// 3
		FIntVector(0, 0, 1),	// 4
		FIntVector(1, 0, 1),	// 5
		FIntVector(1, 1, 1),	// 6
		FIntVector(0, 1, 1)		// 7
	};

	/**
	 * Zerlegung des Wuerfels in sechs Tetraeder entlang der Raumdiagonalen 0-6.
	 *
	 * Warum Tetraeder und nicht Marching Cubes: die Wuerfelvariante braucht eine Tabelle
	 * mit 256 Faellen, und eine einzige falsche Zeile darin erzeugt Loecher, die man erst
	 * im fertigen Bild bemerkt. Die Tetraedervariante kommt ohne Tabelle aus — vier Ecken
	 * lassen nur drei Faelle zu — und ist per Konstruktion dicht, weil zwei benachbarte
	 * Wuerfel dieselbe Diagonale teilen. Der Preis sind etwa doppelt so viele Dreiecke.
	 */
	const int32 GTetrahedra[6][4] =
	{
		{ 0, 6, 1, 2 },
		{ 0, 6, 2, 3 },
		{ 0, 6, 3, 7 },
		{ 0, 6, 7, 4 },
		{ 0, 6, 4, 5 },
		{ 0, 6, 5, 1 }
	};

	/** Beschleunigungsgitter ueber den Atomen, damit das Dichtefeld nicht alle abfragen muss. */
	struct FAtomLookupGrid
	{
		FVector3f Min = FVector3f::ZeroVector;
		float CellSize = 1.f;
		FIntVector Dim = FIntVector(1, 1, 1);
		TArray<int32> CellStart;
		TArray<int32> CellAtoms;

		FIntVector CellOf(const FVector3f& Position) const
		{
			const FVector3f Local = (Position - Min) / CellSize;
			return FIntVector(
				FMath::Clamp(FMath::FloorToInt(Local.X), 0, Dim.X - 1),
				FMath::Clamp(FMath::FloorToInt(Local.Y), 0, Dim.Y - 1),
				FMath::Clamp(FMath::FloorToInt(Local.Z), 0, Dim.Z - 1));
		}

		int32 IndexOf(const FIntVector& Cell) const
		{
			return (Cell.Z * Dim.Y + Cell.Y) * Dim.X + Cell.X;
		}
	};

	void BuildAtomLookupGrid(const TArray<FVector3f>& Positions, const FBox3f& Bounds,
		float CellSize, FAtomLookupGrid& OutGrid)
	{
		OutGrid.Min = Bounds.Min;
		OutGrid.CellSize = FMath::Max(CellSize, 0.1f);

		const FVector3f Extent = Bounds.Max - Bounds.Min;
		OutGrid.Dim = FIntVector(
			FMath::Max(1, FMath::CeilToInt(Extent.X / OutGrid.CellSize) + 1),
			FMath::Max(1, FMath::CeilToInt(Extent.Y / OutGrid.CellSize) + 1),
			FMath::Max(1, FMath::CeilToInt(Extent.Z / OutGrid.CellSize) + 1));

		const int32 NumCells = OutGrid.Dim.X * OutGrid.Dim.Y * OutGrid.Dim.Z;
		OutGrid.CellStart.SetNumZeroed(NumCells + 1);

		TArray<int32> AtomCell;
		AtomCell.SetNumUninitialized(Positions.Num());

		for (int32 i = 0; i < Positions.Num(); ++i)
		{
			AtomCell[i] = OutGrid.IndexOf(OutGrid.CellOf(Positions[i]));
			++OutGrid.CellStart[AtomCell[i] + 1];
		}
		for (int32 c = 0; c < NumCells; ++c)
		{
			OutGrid.CellStart[c + 1] += OutGrid.CellStart[c];
		}

		OutGrid.CellAtoms.SetNumUninitialized(Positions.Num());
		TArray<int32> Cursor = OutGrid.CellStart;
		for (int32 i = 0; i < Positions.Num(); ++i)
		{
			OutGrid.CellAtoms[Cursor[AtomCell[i]]++] = i;
		}
	}
}

namespace MolecularForge
{
	float GetSurfaceRadiusForAtom(float AtomRadius, float Blobbiness, float IsoValue)
	{
		if (Blobbiness >= 0.f || IsoValue <= 0.f)
		{
			return AtomRadius;
		}

		const float Factor = 1.f + FMath::Loge(IsoValue) / Blobbiness;
		return Factor > 0.f ? AtomRadius * FMath::Sqrt(Factor) : 0.f;
	}

	bool BuildGaussianSurface(const UMolecularStructure& Structure,
		const FMolSurfaceOptions& Options, FMolMeshData& OutMesh, FString* OutError)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(MolecularForge_BuildGaussianSurface);

		OutMesh.Reset();

		auto Fail = [OutError](const TCHAR* Message)
		{
			if (OutError)
			{
				*OutError = Message;
			}
			return false;
		};

		if (Options.Blobbiness >= 0.f)
		{
			return Fail(TEXT("Blobbiness muss negativ sein."));
		}
		if (Options.IsoValue <= 0.f)
		{
			return Fail(TEXT("Der Schwellwert muss groesser als null sein."));
		}

		// ---- Beitragende Atome einsammeln ----

		TArray<FVector3f> Positions;
		TArray<float> Radii;
		TArray<int32> SourceAtom;

		Positions.Reserve(Structure.GetNumAtoms());
		Radii.Reserve(Structure.GetNumAtoms());
		SourceAtom.Reserve(Structure.GetNumAtoms());

		float MaxRadius = 0.f;

		for (int32 a = 0; a < Structure.GetNumAtoms(); ++a)
		{
			const uint8 Flags = Structure.AtomFlags[a];
			if (!Options.bShowWater && (Flags & MolAtom_Water) != 0)
			{
				continue;
			}
			if (!Options.bShowHydrogen && Structure.AtomElements[a] == 1)
			{
				continue;
			}

			const float Radius =
				GetElement(Structure.AtomElements[a]).VdWRadius + Options.RadiusInflationAngstrom;

			if (Radius <= 0.f)
			{
				continue;
			}

			Positions.Add(Structure.AtomPositions[a]);
			Radii.Add(Radius);
			SourceAtom.Add(a);
			MaxRadius = FMath::Max(MaxRadius, Radius);
		}

		if (Positions.IsEmpty())
		{
			return Fail(TEXT("Kein Atom traegt zur Oberflaeche bei."));
		}

		const float Influence = MaxRadius * GInfluenceFactor;

		// ---- Gitter aufspannen ----

		FBox3f AtomBounds(ForceInit);
		for (const FVector3f& Position : Positions)
		{
			AtomBounds += Position;
		}

		const FBox3f FieldBounds = AtomBounds.ExpandBy(Influence);
		const FVector3f Extent = FieldBounds.Max - FieldBounds.Min;

		float VoxelSize = FMath::Max(Options.VoxelSizeAngstrom, 0.05f);
		FIntVector Dim;

		auto ComputeDim = [&Extent](float Size)
		{
			return FIntVector(
				FMath::Max(2, FMath::CeilToInt(Extent.X / Size) + 1),
				FMath::Max(2, FMath::CeilToInt(Extent.Y / Size) + 1),
				FMath::Max(2, FMath::CeilToInt(Extent.Z / Size) + 1));
		};

		Dim = ComputeDim(VoxelSize);

		// Notbremse gegen ausufernden Speicherbedarf. Lieber groebere Zellen als ein
		// Absturz — und der Anwender erfaehrt es aus dem Log.
		bool bCoarsened = false;
		while (static_cast<int64>(Dim.X) * Dim.Y * Dim.Z > Options.MaxVoxels)
		{
			VoxelSize *= 1.5f;
			Dim = ComputeDim(VoxelSize);
			bCoarsened = true;
		}

		if (bCoarsened)
		{
			UE_LOG(LogMolecularForge, Warning,
				TEXT("Oberflaechengitter vergroebert auf %.2f A, sonst waeren es mehr als %lld Zellen gewesen."),
				VoxelSize, Options.MaxVoxels);
		}

		const int64 NumPoints = static_cast<int64>(Dim.X) * Dim.Y * Dim.Z;

		FAtomLookupGrid Lookup;
		BuildAtomLookupGrid(Positions, AtomBounds, Influence, Lookup);

		// ---- Dichtefeld fuellen ----
		// Sammeln je Gitterpunkt statt Streuen je Atom: so schreibt jeder Thread nur in
		// seine eigenen Punkte, ganz ohne Sperren.

		TArray<float> Density;
		Density.SetNumUninitialized(static_cast<int32>(NumPoints));

		const float Blobbiness = Options.Blobbiness;
		const float InfluenceSq = Influence * Influence;

		ParallelFor(Dim.Z, [&](int32 z)
		{
			for (int32 y = 0; y < Dim.Y; ++y)
			{
				for (int32 x = 0; x < Dim.X; ++x)
				{
					const FVector3f Point = FieldBounds.Min + FVector3f(x, y, z) * VoxelSize;
					const FIntVector Cell = Lookup.CellOf(Point);

					float Sum = 0.f;

					for (int32 dz = -1; dz <= 1; ++dz)
					{
						const int32 CellZ = Cell.Z + dz;
						if (CellZ < 0 || CellZ >= Lookup.Dim.Z) { continue; }

						for (int32 dy = -1; dy <= 1; ++dy)
						{
							const int32 CellY = Cell.Y + dy;
							if (CellY < 0 || CellY >= Lookup.Dim.Y) { continue; }

							for (int32 dx = -1; dx <= 1; ++dx)
							{
								const int32 CellX = Cell.X + dx;
								if (CellX < 0 || CellX >= Lookup.Dim.X) { continue; }

								const int32 Neighbour = Lookup.IndexOf(FIntVector(CellX, CellY, CellZ));
								for (int32 s = Lookup.CellStart[Neighbour]; s < Lookup.CellStart[Neighbour + 1]; ++s)
								{
									const int32 i = Lookup.CellAtoms[s];
									const float DistSq = FVector3f::DistSquared(Point, Positions[i]);
									if (DistSq > InfluenceSq)
									{
										continue;
									}

									const float Ratio = DistSq / (Radii[i] * Radii[i]);
									Sum += FMath::Exp(Blobbiness * (Ratio - 1.f));
								}
							}
						}
					}

					Density[(z * Dim.Y + y) * Dim.X + x] = Sum;
				}
			}
		}, Dim.Z < 4 ? EParallelForFlags::ForceSingleThread : EParallelForFlags::None);

		// ---- Gitter durchlaufen ----

		auto PointIndex = [&Dim](int32 x, int32 y, int32 z)
		{
			return (z * Dim.Y + y) * Dim.X + x;
		};

		auto PositionOf = [&](const FIntVector& P)
		{
			return FieldBounds.Min + FVector3f(P.X, P.Y, P.Z) * VoxelSize;
		};

		// Gradient per zentraler Differenz. Er zeigt in Richtung wachsender Dichte, also
		// nach innen — die Oberflaechennormale ist seine Gegenrichtung. Aus dem Feld
		// abgeleitete Normalen sind glatter als aus den Dreiecken gemittelte und kosten
		// nichts extra, weil das Feld ohnehin dasteht.
		auto GradientAt = [&](const FIntVector& P)
		{
			auto Sample = [&](int32 x, int32 y, int32 z)
			{
				return Density[PointIndex(
					FMath::Clamp(x, 0, Dim.X - 1),
					FMath::Clamp(y, 0, Dim.Y - 1),
					FMath::Clamp(z, 0, Dim.Z - 1))];
			};

			return FVector3f(
				Sample(P.X + 1, P.Y, P.Z) - Sample(P.X - 1, P.Y, P.Z),
				Sample(P.X, P.Y + 1, P.Z) - Sample(P.X, P.Y - 1, P.Z),
				Sample(P.X, P.Y, P.Z + 1) - Sample(P.X, P.Y, P.Z - 1));
		};

		// Geteilte Kantenpunkte, damit die Flaeche dicht bleibt und nicht in lauter
		// Einzeldreiecke zerfaellt.
		TMap<uint64, int32> EdgeVertices;
		EdgeVertices.Reserve(1024);

		const float Scale = Options.UnitsPerAngstrom;
		const float Iso = Options.IsoValue;

		auto VertexOnEdge = [&](const FIntVector& A, const FIntVector& B) -> int32
		{
			const int32 IndexA = PointIndex(A.X, A.Y, A.Z);
			const int32 IndexB = PointIndex(B.X, B.Y, B.Z);
			const uint64 Key = (static_cast<uint64>(FMath::Min(IndexA, IndexB)) << 32)
				| static_cast<uint32>(FMath::Max(IndexA, IndexB));

			if (const int32* Existing = EdgeVertices.Find(Key))
			{
				return *Existing;
			}

			const float DensityA = Density[IndexA];
			const float DensityB = Density[IndexB];
			const float Denominator = DensityB - DensityA;

			const float T = FMath::Abs(Denominator) > UE_SMALL_NUMBER
				? FMath::Clamp((Iso - DensityA) / Denominator, 0.f, 1.f)
				: 0.5f;

			const FVector3f PosA = PositionOf(A);
			const FVector3f PosB = PositionOf(B);
			const FVector3f Position = FMath::Lerp(PosA, PosB, T);

			const FVector3f Gradient = FMath::Lerp(GradientAt(A), GradientAt(B), T);
			FVector3f Normal = (-Gradient).GetSafeNormal();
			if (Normal.IsNearlyZero())
			{
				Normal = FVector3f::ZAxisVector;
			}

			const int32 NewIndex = OutMesh.Positions.Num();
			OutMesh.Positions.Add(Position * Scale);
			OutMesh.Normals.Add(Normal);
			OutMesh.UVs.Add(FVector2f(0.f, 0.f));
			OutMesh.Colors.Add(FColor::White);

			EdgeVertices.Add(Key, NewIndex);
			return NewIndex;
		};

		/**
		 * Haengt ein Dreieck an und richtet es dabei am Feld aus.
		 *
		 * Die Umlaufrichtung aus der Lage der Ecken im Tetraeder herzuleiten ist moeglich,
		 * aber fehleranfaellig: sie haengt davon ab, auf welcher Seite die einzelne Ecke
		 * liegt *und* wie der Tetraeder in der Zerlegung orientiert ist. Ein Vorzeichenfehler
		 * darin erzeugt eine Flaeche, deren Dreiecke teils richtig und teils verdreht sind —
		 * das Mesh ist dann nicht mehr dicht, und im Bild sieht man Loecher, die je nach
		 * Blickwinkel auftauchen und verschwinden.
		 *
		 * Der Dichtegradient beantwortet die Frage dagegen unmittelbar und unabhaengig von
		 * der Zerlegung: er zeigt nach innen, die Normale also nach aussen. Danach wird
		 * ausgerichtet.
		 */
		auto AddOrientedTriangle = [&OutMesh](int32 V0, int32 V1, int32 V2)
		{
			const FVector3f& P0 = OutMesh.Positions[V0];
			const FVector3f& P1 = OutMesh.Positions[V1];
			const FVector3f& P2 = OutMesh.Positions[V2];

			const FVector3f FaceNormal = FVector3f::CrossProduct(P1 - P0, P2 - P0);
			const FVector3f Reference =
				OutMesh.Normals[V0] + OutMesh.Normals[V1] + OutMesh.Normals[V2];

			OutMesh.Triangles.Add(V0);

			// Bei entarteten Dreiecken gibt es nichts auszurichten; sie bleiben stehen,
			// damit die Kantenbilanz der Flaeche aufgeht.
			if (FVector3f::DotProduct(FaceNormal, Reference) < 0.f)
			{
				OutMesh.Triangles.Add(V2);
				OutMesh.Triangles.Add(V1);
			}
			else
			{
				OutMesh.Triangles.Add(V1);
				OutMesh.Triangles.Add(V2);
			}
		};

		for (int32 z = 0; z + 1 < Dim.Z; ++z)
		{
			for (int32 y = 0; y + 1 < Dim.Y; ++y)
			{
				for (int32 x = 0; x + 1 < Dim.X; ++x)
				{
					const FIntVector Base(x, y, z);

					FIntVector Corner[8];
					float CornerDensity[8];
					for (int32 c = 0; c < 8; ++c)
					{
						Corner[c] = Base + GCubeCorners[c];
						CornerDensity[c] = Density[PointIndex(Corner[c].X, Corner[c].Y, Corner[c].Z)];
					}

					for (int32 t = 0; t < 6; ++t)
					{
						const int32* Tet = GTetrahedra[t];

						int32 Inside[4];
						int32 Outside[4];
						int32 NumInside = 0;
						int32 NumOutside = 0;

						for (int32 k = 0; k < 4; ++k)
						{
							if (CornerDensity[Tet[k]] >= Iso)
							{
								Inside[NumInside++] = Tet[k];
							}
							else
							{
								Outside[NumOutside++] = Tet[k];
							}
						}

						if (NumInside == 0 || NumInside == 4)
						{
							continue;
						}

						if (NumInside == 1 || NumInside == 3)
						{
							// Eine Ecke steht allein — es entsteht ein Dreieck zwischen den
							// drei Kanten, die von ihr ausgehen.
							const bool bSingleInside = (NumInside == 1);
							const int32 Apex = bSingleInside ? Inside[0] : Outside[0];
							const int32* Others = bSingleInside ? Outside : Inside;

							const int32 V0 = VertexOnEdge(Corner[Apex], Corner[Others[0]]);
							const int32 V1 = VertexOnEdge(Corner[Apex], Corner[Others[1]]);
							const int32 V2 = VertexOnEdge(Corner[Apex], Corner[Others[2]]);

							AddOrientedTriangle(V0, V1, V2);
						}
						else
						{
							// Zwei gegen zwei: es entsteht ein Viereck.
							const int32 A = Inside[0];
							const int32 B = Inside[1];
							const int32 C = Outside[0];
							const int32 D = Outside[1];

							const int32 V0 = VertexOnEdge(Corner[A], Corner[C]);
							const int32 V1 = VertexOnEdge(Corner[B], Corner[C]);
							const int32 V2 = VertexOnEdge(Corner[B], Corner[D]);
							const int32 V3 = VertexOnEdge(Corner[A], Corner[D]);

							AddOrientedTriangle(V0, V1, V2);
							AddOrientedTriangle(V0, V2, V3);
						}
					}
				}
			}
		}

		if (OutMesh.Triangles.IsEmpty())
		{
			return Fail(TEXT("Bei diesen Einstellungen entsteht keine Oberflaeche. "
				"Meist ist der Schwellwert zu hoch oder das Gitter zu grob."));
		}

		// ---- Einfaerben ueber das naechstgelegene Atom ----
		// Erst jetzt, weil die Vertexpositionen vorher nicht feststehen. Laeuft parallel,
		// da jeder Vertex nur seinen eigenen Eintrag beschreibt.

		ParallelFor(OutMesh.Positions.Num(), [&](int32 v)
		{
			const FVector3f Point = OutMesh.Positions[v] / Scale;
			const FIntVector Cell = Lookup.CellOf(Point);

			int32 Nearest = INDEX_NONE;
			float NearestDistSq = TNumericLimits<float>::Max();

			// Der Suchradius waechst so lange, bis etwas gefunden ist. In der Praxis
			// reicht der erste Ring, weil die Oberflaeche nah an den Atomen liegt.
			for (int32 Radius = 1; Radius <= 3 && Nearest == INDEX_NONE; ++Radius)
			{
				for (int32 dz = -Radius; dz <= Radius; ++dz)
				{
					const int32 CellZ = Cell.Z + dz;
					if (CellZ < 0 || CellZ >= Lookup.Dim.Z) { continue; }

					for (int32 dy = -Radius; dy <= Radius; ++dy)
					{
						const int32 CellY = Cell.Y + dy;
						if (CellY < 0 || CellY >= Lookup.Dim.Y) { continue; }

						for (int32 dx = -Radius; dx <= Radius; ++dx)
						{
							const int32 CellX = Cell.X + dx;
							if (CellX < 0 || CellX >= Lookup.Dim.X) { continue; }

							const int32 Neighbour = Lookup.IndexOf(FIntVector(CellX, CellY, CellZ));
							for (int32 s = Lookup.CellStart[Neighbour]; s < Lookup.CellStart[Neighbour + 1]; ++s)
							{
								const int32 i = Lookup.CellAtoms[s];
								const float DistSq = FVector3f::DistSquared(Point, Positions[i]);
								if (DistSq < NearestDistSq)
								{
									NearestDistSq = DistSq;
									Nearest = i;
								}
							}
						}
					}
				}
			}

			if (Nearest != INDEX_NONE)
			{
				const FLinearColor Color = Structure.GetAtomColor(
					SourceAtom[Nearest], Options.ColorScheme, Options.UniformColor);
				OutMesh.Colors[v] = Color.ToFColor(/*bSRGB=*/false);
			}
		}, OutMesh.Positions.Num() < GParallelThreshold
			? EParallelForFlags::ForceSingleThread : EParallelForFlags::None);

		UE_LOG(LogMolecularForge, Log,
			TEXT("Oberflaeche erzeugt: %d Vertices, %d Dreiecke, Gitter %dx%dx%d bei %.2f A."),
			OutMesh.NumVertices(), OutMesh.NumTriangles(), Dim.X, Dim.Y, Dim.Z, VoxelSize);

		return true;
	}
}
