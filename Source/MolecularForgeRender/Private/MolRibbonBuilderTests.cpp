// Copyright 2026 Simulated Flow All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "MolRibbonBuilder.h"
#include "MolBackboneSpline.h"
#include "MolTestChainBuilder.h"
#include "MolecularStructure.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/** Damit die Zahlen im Test in Angstroem bleiben und nicht umgerechnet werden muessen. */
	FMolRibbonOptions MakeTestRibbonOptions()
	{
		FMolRibbonOptions Options;
		Options.UnitsPerAngstrom = 1.f;
		Options.RingResolution = 12;
		return Options;
	}

	/**
	 * Schluessel fuer positionsbasiertes Verschweissen.
	 *
	 * Deckel und Mantel teilen sich zwar die Randpositionen, aber nicht die Indizes — sie
	 * brauchen eigene Vertices, damit die Kante scharf bleibt. Fuer die Pruefung auf
	 * Geschlossenheit muss deshalb ueber die Position zusammengefuehrt werden, nicht ueber
	 * den Index.
	 */
	uint64 QuantizePosition(const FVector3f& Position)
	{
		const int64 X = FMath::RoundToInt64(Position.X * 1000.0);
		const int64 Y = FMath::RoundToInt64(Position.Y * 1000.0);
		const int64 Z = FMath::RoundToInt64(Position.Z * 1000.0);
		return HashCombine(HashCombine(GetTypeHash(X), GetTypeHash(Y)), GetTypeHash(Z));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMolRibbonIntegrityTest,
	"MolecularForge.Band.MeshIstBrauchbar",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMolRibbonIntegrityTest::RunTest(const FString& Parameters)
{
	constexpr int32 NumResidues = 14;

	TArray<EMolSecondaryStructure> SS;
	for (int32 i = 0; i < NumResidues; ++i)
	{
		SS.Add(i < 8 ? EMolSecondaryStructure::Helix : EMolSecondaryStructure::Coil);
	}

	UMolecularStructure* Structure =
		MolTest::BuildChain(MolTest::MakeStraightAnchors(NumResidues), {}, SS);

	TArray<FMolBackboneSegment> Segments;
	MolecularForge::BuildBackboneSegments(*Structure, FMolBackboneOptions(), Segments);
	TestEqual(TEXT("Ein Abschnitt"), Segments.Num(), 1);

	FMolMeshData Mesh;
	MolecularForge::BuildRibbonMesh(*Structure, Segments, MakeTestRibbonOptions(), Mesh);

	TestFalse(TEXT("Es entsteht ein Mesh"), Mesh.IsEmpty());
	TestTrue(TEXT("Dreiecke sind vollstaendig"), Mesh.Triangles.Num() % 3 == 0);

	// Alle Attributarrays muessen gleich lang sein, sonst liest die Komponente daneben.
	TestEqual(TEXT("Normalen je Vertex"), Mesh.Normals.Num(), Mesh.NumVertices());
	TestEqual(TEXT("UVs je Vertex"), Mesh.UVs.Num(), Mesh.NumVertices());
	TestEqual(TEXT("Farben je Vertex"), Mesh.Colors.Num(), Mesh.NumVertices());

	for (int32 Index : Mesh.Triangles)
	{
		if (Index < 0 || Index >= Mesh.NumVertices())
		{
			AddError(FString::Printf(TEXT("Dreiecksindex %d liegt ausserhalb von %d Vertices."),
				Index, Mesh.NumVertices()));
			return false;
		}
	}

	for (int32 v = 0; v < Mesh.NumVertices(); ++v)
	{
		if (Mesh.Positions[v].ContainsNaN() || Mesh.Normals[v].ContainsNaN())
		{
			AddError(FString::Printf(TEXT("Vertex %d enthaelt NaN."), v));
			return false;
		}
		if (!FMath::IsNearlyEqual(Mesh.Normals[v].Size(), 1.f, 0.01f))
		{
			AddError(FString::Printf(TEXT("Normale an Vertex %d ist nicht normiert (%.4f)."),
				v, Mesh.Normals[v].Size()));
			return false;
		}
	}

	// Entartete Dreiecke deuten auf einen Fehler in der Ringverbindung hin.
	int32 DegenerateCount = 0;
	for (int32 t = 0; t + 2 < Mesh.Triangles.Num(); t += 3)
	{
		const FVector3f& A = Mesh.Positions[Mesh.Triangles[t]];
		const FVector3f& B = Mesh.Positions[Mesh.Triangles[t + 1]];
		const FVector3f& C = Mesh.Positions[Mesh.Triangles[t + 2]];

		if (FVector3f::CrossProduct(B - A, C - A).SizeSquared() < UE_SMALL_NUMBER)
		{
			++DegenerateCount;
		}
	}
	TestEqual(TEXT("Keine entarteten Dreiecke"), DegenerateCount, 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMolRibbonClosedTest,
	"MolecularForge.Band.MeshIstGeschlossen",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMolRibbonClosedTest::RunTest(const FString& Parameters)
{
	// Ein geschlossenes, einheitlich orientiertes Mesh hat jede gerichtete Kante genau
	// einmal. Diese Pruefung faengt so ziemlich jeden Fehler in der Ringverbindung und in
	// den Deckeln ab — ein vergessener Ringschluss oder eine verdrehte Umlaufrichtung
	// faellt sofort auf, waehrend man es im Bild uebersehen wuerde.
	UMolecularStructure* Structure =
		MolTest::BuildChain(MolTest::MakeStraightAnchors(10), {});

	TArray<FMolBackboneSegment> Segments;
	MolecularForge::BuildBackboneSegments(*Structure, FMolBackboneOptions(), Segments);

	FMolRibbonOptions Options = MakeTestRibbonOptions();
	Options.bGenerateCaps = true;

	FMolMeshData Mesh;
	MolecularForge::BuildRibbonMesh(*Structure, Segments, Options, Mesh);

	TestFalse(TEXT("Es entsteht ein Mesh"), Mesh.IsEmpty());

	// Positionsbasiert verschweissen — Deckel und Mantel teilen Positionen, nicht Indizes.
	TMap<uint64, int32> Welded;
	TArray<int32> Representative;
	Representative.SetNumUninitialized(Mesh.NumVertices());

	for (int32 v = 0; v < Mesh.NumVertices(); ++v)
	{
		const uint64 Key = QuantizePosition(Mesh.Positions[v]);
		if (const int32* Existing = Welded.Find(Key))
		{
			Representative[v] = *Existing;
		}
		else
		{
			Welded.Add(Key, v);
			Representative[v] = v;
		}
	}

	TMap<uint64, int32> DirectedEdges;
	auto EdgeKey = [](int32 A, int32 B) { return (static_cast<uint64>(A) << 32) | static_cast<uint32>(B); };

	for (int32 t = 0; t + 2 < Mesh.Triangles.Num(); t += 3)
	{
		const int32 I0 = Representative[Mesh.Triangles[t]];
		const int32 I1 = Representative[Mesh.Triangles[t + 1]];
		const int32 I2 = Representative[Mesh.Triangles[t + 2]];

		++DirectedEdges.FindOrAdd(EdgeKey(I0, I1));
		++DirectedEdges.FindOrAdd(EdgeKey(I1, I2));
		++DirectedEdges.FindOrAdd(EdgeKey(I2, I0));
	}

	int32 UnpairedEdges = 0;
	int32 DuplicateEdges = 0;

	for (const TPair<uint64, int32>& Edge : DirectedEdges)
	{
		if (Edge.Value != 1)
		{
			++DuplicateEdges;
			continue;
		}

		const int32 A = static_cast<int32>(Edge.Key >> 32);
		const int32 B = static_cast<int32>(Edge.Key & 0xFFFFFFFF);

		if (!DirectedEdges.Contains(EdgeKey(B, A)))
		{
			++UnpairedEdges;
		}
	}

	TestEqual(TEXT("Keine gerichtete Kante kommt doppelt vor"), DuplicateEdges, 0);
	TestEqual(TEXT("Jede Kante hat ihre Gegenrichtung — das Mesh ist geschlossen"), UnpairedEdges, 0);

	// Ohne Deckel muss es dagegen offen sein. Sonst wuerde der Test oben auch dann
	// bestehen, wenn die Deckel gar nichts beitragen.
	{
		FMolRibbonOptions Open = MakeTestRibbonOptions();
		Open.bGenerateCaps = false;

		FMolMeshData OpenMesh;
		MolecularForge::BuildRibbonMesh(*Structure, Segments, Open, OpenMesh);

		TestTrue(TEXT("Ohne Deckel hat das Mesh weniger Dreiecke"),
			OpenMesh.NumTriangles() < Mesh.NumTriangles());
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMolRibbonNormalsOutwardTest,
	"MolecularForge.Band.NormalenZeigenNachAussen",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMolRibbonNormalsOutwardTest::RunTest(const FString& Parameters)
{
	// Prueft die Umlaufrichtung der Dreiecke. Ist sie verdreht, zeigen alle Normalen nach
	// innen und das Band waere im Spiel unsichtbar oder schwarz — ein Fehler, den man erst
	// im Editor bemerken wuerde, und dort auch erst beim genauen Hinsehen.
	constexpr int32 NumResidues = 10;
	constexpr int32 Ring = 12;

	UMolecularStructure* Structure =
		MolTest::BuildChain(MolTest::MakeStraightAnchors(NumResidues), {});

	TArray<FMolBackboneSegment> Segments;
	MolecularForge::BuildBackboneSegments(*Structure, FMolBackboneOptions(), Segments);
	if (Segments.Num() != 1)
	{
		AddError(TEXT("Erwartet wurde genau ein Abschnitt."));
		return false;
	}

	FMolRibbonOptions Options = MakeTestRibbonOptions();
	Options.RingResolution = Ring;
	Options.bGenerateCaps = false;

	FMolMeshData Mesh;
	MolecularForge::BuildRibbonMesh(*Structure, Segments, Options, Mesh);

	const TArray<FMolBackbonePoint>& Points = Segments[0].Points;
	TestEqual(TEXT("Vertexzahl entspricht Punkten mal Ringaufloesung"),
		Mesh.NumVertices(), Points.Num() * Ring);

	int32 InwardCount = 0;
	for (int32 v = 0; v < Mesh.NumVertices(); ++v)
	{
		const int32 PointIndex = v / Ring;
		if (!Points.IsValidIndex(PointIndex))
		{
			break;
		}

		const FVector3f Outward = Mesh.Positions[v] - Points[PointIndex].Position;
		if (Outward.IsNearlyZero())
		{
			continue;
		}

		if (FVector3f::DotProduct(Mesh.Normals[v], Outward.GetSafeNormal()) <= 0.f)
		{
			++InwardCount;
		}
	}

	TestEqual(FString::Printf(TEXT("Alle Normalen zeigen von der Mittellinie weg (%d taten es nicht)"),
		InwardCount), InwardCount, 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMolRibbonArrowHeadTest,
	"MolecularForge.Band.FaltblattBekommtEinePfeilspitze",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMolRibbonArrowHeadTest::RunTest(const FString& Parameters)
{
	// Ein Faltblatt ohne Pfeilspitze ist von einer Helix nicht zu unterscheiden. Die Spitze
	// zeigt ausserdem die Laufrichtung der Kette — die halbe Aussage einer Cartoon-Abbildung
	// haengt daran.
	constexpr int32 NumResidues = 20;
	constexpr int32 Ring = 12;
	constexpr int32 SheetEnd = 14;

	TArray<EMolSecondaryStructure> SS;
	for (int32 i = 0; i < NumResidues; ++i)
	{
		SS.Add(i < SheetEnd ? EMolSecondaryStructure::Sheet : EMolSecondaryStructure::Coil);
	}

	UMolecularStructure* Structure =
		MolTest::BuildChain(MolTest::MakeStraightAnchors(NumResidues), {}, SS);

	TArray<FMolBackboneSegment> Segments;
	MolecularForge::BuildBackboneSegments(*Structure, FMolBackboneOptions(), Segments);
	if (Segments.Num() != 1)
	{
		AddError(TEXT("Erwartet wurde genau ein Abschnitt."));
		return false;
	}

	FMolRibbonOptions Options = MakeTestRibbonOptions();
	Options.RingResolution = Ring;
	Options.bGenerateCaps = false;

	FMolMeshData Mesh;
	MolecularForge::BuildRibbonMesh(*Structure, Segments, Options, Mesh);

	const TArray<FMolBackbonePoint>& Points = Segments[0].Points;

	// Breite eines Querschnitts: groesster Abstand seiner Ringvertices zur Mittellinie.
	auto RingWidthAt = [&Mesh, &Points, Ring](int32 PointIndex)
	{
		float MaxDistance = 0.f;
		for (int32 k = 0; k < Ring; ++k)
		{
			const int32 v = PointIndex * Ring + k;
			if (Mesh.Positions.IsValidIndex(v))
			{
				MaxDistance = FMath::Max(MaxDistance,
					FVector3f::Dist(Mesh.Positions[v], Points[PointIndex].Position));
			}
		}
		return MaxDistance;
	};

	// Letzter Punkt, der noch zum Faltblatt gehoert.
	int32 LastSheetPoint = INDEX_NONE;
	float WidestSheetPoint = 0.f;

	for (int32 p = 0; p < Points.Num(); ++p)
	{
		if (Points[p].SecondaryStructure == EMolSecondaryStructure::Sheet)
		{
			LastSheetPoint = p;
			WidestSheetPoint = FMath::Max(WidestSheetPoint, RingWidthAt(p));
		}
	}

	TestTrue(TEXT("Es gibt Faltblatt-Punkte"), LastSheetPoint != INDEX_NONE);
	if (LastSheetPoint == INDEX_NONE)
	{
		return false;
	}

	// An der Basis der Spitze muss das Band deutlich breiter sein als der Faltblattkoerper.
	TestTrue(FString::Printf(TEXT("Die Pfeilbasis ist breiter als der Koerper (%.2f A)"), WidestSheetPoint),
		WidestSheetPoint > Options.SheetHalfWidth * 1.3f);

	// Und ganz am Ende muss es spitz zulaufen.
	const float TipWidth = RingWidthAt(LastSheetPoint);
	TestTrue(FString::Printf(TEXT("Die Spitze laeuft zusammen (%.3f A, Basis %.2f A)"),
		TipWidth, WidestSheetPoint), TipWidth < WidestSheetPoint * 0.25f);

	// Gegenprobe: eine reine Helix darf nirgends so breit werden wie eine Pfeilbasis.
	{
		TArray<EMolSecondaryStructure> AllHelix;
		AllHelix.Init(EMolSecondaryStructure::Helix, NumResidues);

		UMolecularStructure* HelixOnly =
			MolTest::BuildChain(MolTest::MakeStraightAnchors(NumResidues), {}, AllHelix);

		TArray<FMolBackboneSegment> HelixSegments;
		MolecularForge::BuildBackboneSegments(*HelixOnly, FMolBackboneOptions(), HelixSegments);

		FMolMeshData HelixMesh;
		MolecularForge::BuildRibbonMesh(*HelixOnly, HelixSegments, Options, HelixMesh);

		float WidestHelix = 0.f;
		const TArray<FMolBackbonePoint>& HelixPoints = HelixSegments[0].Points;
		for (int32 p = 0; p < HelixPoints.Num(); ++p)
		{
			for (int32 k = 0; k < Ring; ++k)
			{
				const int32 v = p * Ring + k;
				if (HelixMesh.Positions.IsValidIndex(v))
				{
					WidestHelix = FMath::Max(WidestHelix,
						FVector3f::Dist(HelixMesh.Positions[v], HelixPoints[p].Position));
				}
			}
		}

		TestTrue(FString::Printf(TEXT("Eine Helix bekommt keine Spitze (breiteste Stelle %.2f A)"), WidestHelix),
			WidestHelix < Options.ArrowHalfWidth * 0.9f);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMolRibbonEmptyInputTest,
	"MolecularForge.Band.LeereEingabeErgibtLeeresMesh",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMolRibbonEmptyInputTest::RunTest(const FString& Parameters)
{
	UMolecularStructure* Structure = NewObject<UMolecularStructure>();

	FMolMeshData Mesh;
	MolecularForge::BuildRibbonMesh(*Structure, {}, MakeTestRibbonOptions(), Mesh);

	TestTrue(TEXT("Ohne Abschnitte entsteht kein Mesh"), Mesh.IsEmpty());
	TestEqual(TEXT("Keine Vertices"), Mesh.NumVertices(), 0);

	// Ein einzelner Punkt kann kein Band aufspannen und darf nicht dazu fuehren,
	// dass ein entartetes Dreieck entsteht.
	{
		FMolBackboneSegment Single;
		Single.ChainIndex = 0;
		Single.Points.AddDefaulted();

		FMolMeshData SingleMesh;
		MolecularForge::BuildRibbonMesh(*Structure, { Single }, MakeTestRibbonOptions(), SingleMesh);

		TestTrue(TEXT("Ein einzelner Punkt ergibt kein Mesh"), SingleMesh.IsEmpty());
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
