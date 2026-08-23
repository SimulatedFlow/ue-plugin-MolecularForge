// Copyright 2026 Silvan Teufel All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "MolSurfaceBuilder.h"
#include "MolTestChainBuilder.h"
#include "MolElementTable.h"
#include "MolecularStructure.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FMolSurfaceOptions MakeTestSurfaceOptions()
	{
		FMolSurfaceOptions Options;
		// Massstab 1, damit die Zahlen im Test in Angstroem bleiben.
		Options.UnitsPerAngstrom = 1.f;
		Options.VoxelSizeAngstrom = 0.25f;
		return Options;
	}

	/** Prueft, ob das Mesh geschlossen ist — jede gerichtete Kante genau einmal. */
	bool IsClosedMesh(const FMolMeshData& Mesh, int32& OutUnpaired, int32& OutDuplicate)
	{
		TMap<uint64, int32> DirectedEdges;
		auto EdgeKey = [](int32 A, int32 B) { return (static_cast<uint64>(A) << 32) | static_cast<uint32>(B); };

		for (int32 t = 0; t + 2 < Mesh.Triangles.Num(); t += 3)
		{
			const int32 I0 = Mesh.Triangles[t];
			const int32 I1 = Mesh.Triangles[t + 1];
			const int32 I2 = Mesh.Triangles[t + 2];

			++DirectedEdges.FindOrAdd(EdgeKey(I0, I1));
			++DirectedEdges.FindOrAdd(EdgeKey(I1, I2));
			++DirectedEdges.FindOrAdd(EdgeKey(I2, I0));
		}

		OutUnpaired = 0;
		OutDuplicate = 0;

		for (const TPair<uint64, int32>& Edge : DirectedEdges)
		{
			if (Edge.Value != 1)
			{
				++OutDuplicate;
				continue;
			}
			const int32 A = static_cast<int32>(Edge.Key >> 32);
			const int32 B = static_cast<int32>(Edge.Key & 0xFFFFFFFF);
			if (!DirectedEdges.Contains(EdgeKey(B, A)))
			{
				++OutUnpaired;
			}
		}

		return OutUnpaired == 0 && OutDuplicate == 0;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMolSurfaceRadiusTest,
	"MolecularForge.Oberflaeche.EinzelatomHatDenRichtigenRadius",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMolSurfaceRadiusTest::RunTest(const FString& Parameters)
{
	using namespace MolecularForge;

	// Ein einzelnes Atom ergibt eine Kugel, deren Radius sich aus der Dichteformel
	// ausrechnen laesst. Damit ist die gesamte Kette pruefbar — Dichtefeld, Gitterdurchlauf
	// und Kanteninterpolation — und zwar gegen eine Zahl und nicht gegen Augenmass.
	// Waere die Interpolation etwa weggelassen und immer die Kantenmitte genommen worden,
	// laege der Radius sichtbar daneben, das Bild saehe aber unverdaechtig aus.
	FMolSurfaceOptions Options = MakeTestSurfaceOptions();

	const float AtomRadius = GetElement(6).VdWRadius;	// Kohlenstoff, 1,70 A
	const float Expected = GetSurfaceRadiusForAtom(AtomRadius, Options.Blobbiness, Options.IsoValue);

	TestTrue(FString::Printf(TEXT("Erwarteter Radius ist plausibel (%.3f A bei Atomradius %.2f A)"),
		Expected, AtomRadius), Expected > AtomRadius && Expected < AtomRadius * 1.5f);

	UMolecularStructure* Structure = MolTest::BuildAtomCloud({ FVector3f::ZeroVector });

	FMolMeshData Mesh;
	FString Error;
	TestTrue(TEXT("Oberflaeche entsteht"), BuildGaussianSurface(*Structure, Options, Mesh, &Error));
	TestTrue(FString::Printf(TEXT("Kein Fehler: %s"), *Error), Error.IsEmpty());
	TestTrue(TEXT("Es gibt Dreiecke"), Mesh.NumTriangles() > 0);

	// Toleranz in der Groessenordnung der Zellkante — feiner kann ein Gitterverfahren nicht.
	const float Tolerance = Options.VoxelSizeAngstrom;

	float MinDistance = TNumericLimits<float>::Max();
	float MaxDistance = 0.f;

	for (const FVector3f& Position : Mesh.Positions)
	{
		const float Distance = Position.Size();
		MinDistance = FMath::Min(MinDistance, Distance);
		MaxDistance = FMath::Max(MaxDistance, Distance);
	}

	TestTrue(FString::Printf(
		TEXT("Alle Vertices liegen bei %.3f A (gemessen %.3f bis %.3f, Toleranz %.2f)"),
		Expected, MinDistance, MaxDistance, Tolerance),
		MinDistance > Expected - Tolerance && MaxDistance < Expected + Tolerance);

	// Ein groesserer Radius muss eine groessere Kugel ergeben — sonst wuerde der Radius
	// gar nicht ausgewertet und der Test oben liefe zufaellig durch.
	{
		FMolSurfaceOptions Inflated = Options;
		Inflated.RadiusInflationAngstrom = 1.f;

		FMolMeshData InflatedMesh;
		TestTrue(TEXT("Aufgeblasene Oberflaeche entsteht"),
			BuildGaussianSurface(*Structure, Inflated, InflatedMesh, nullptr));

		float InflatedMax = 0.f;
		for (const FVector3f& Position : InflatedMesh.Positions)
		{
			InflatedMax = FMath::Max(InflatedMax, Position.Size());
		}

		TestTrue(FString::Printf(TEXT("Aufblasen vergroessert die Kugel (%.2f gegen %.2f)"),
			InflatedMax, MaxDistance), InflatedMax > MaxDistance + 0.8f);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMolSurfaceClosedTest,
	"MolecularForge.Oberflaeche.MeshIstGeschlossen",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMolSurfaceClosedTest::RunTest(const FString& Parameters)
{
	// Der Grund fuer die Tetraeder-Zerlegung war Dichtheit. Wenn sie nicht haelt, war die
	// Entscheidung umsonst — also wird sie geprueft, und zwar an einem Fall mit
	// verschmelzenden Kugeln, wo Loecher am ehesten entstehen.
	UMolecularStructure* Structure = MolTest::BuildAtomCloud({
		FVector3f(0.f, 0.f, 0.f),
		FVector3f(2.2f, 0.f, 0.f),
		FVector3f(1.1f, 1.9f, 0.f),
		FVector3f(1.1f, 0.6f, 1.8f)
	});

	FMolMeshData Mesh;
	TestTrue(TEXT("Oberflaeche entsteht"),
		MolecularForge::BuildGaussianSurface(*Structure, MakeTestSurfaceOptions(), Mesh, nullptr));

	int32 Unpaired = 0;
	int32 Duplicate = 0;
	const bool bClosed = IsClosedMesh(Mesh, Unpaired, Duplicate);

	TestTrue(FString::Printf(
		TEXT("Das Mesh ist geschlossen (%d Kanten ohne Gegenstueck, %d doppelte)"),
		Unpaired, Duplicate), bClosed);

	// Indizes und Attribute muessen zusammenpassen, sonst liest die Komponente daneben.
	TestEqual(TEXT("Normalen je Vertex"), Mesh.Normals.Num(), Mesh.NumVertices());
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

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMolSurfaceNormalsTest,
	"MolecularForge.Oberflaeche.NormalenUndUmlaufStimmen",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMolSurfaceNormalsTest::RunTest(const FString& Parameters)
{
	UMolecularStructure* Structure = MolTest::BuildAtomCloud({ FVector3f::ZeroVector });

	FMolMeshData Mesh;
	TestTrue(TEXT("Oberflaeche entsteht"),
		MolecularForge::BuildGaussianSurface(*Structure, MakeTestSurfaceOptions(), Mesh, nullptr));

	// Bei einer Kugel um den Ursprung muss jede Normale vom Ursprung wegzeigen.
	int32 InwardNormals = 0;
	for (int32 v = 0; v < Mesh.NumVertices(); ++v)
	{
		const FVector3f Outward = Mesh.Positions[v].GetSafeNormal();
		if (FVector3f::DotProduct(Mesh.Normals[v], Outward) <= 0.f)
		{
			++InwardNormals;
		}
	}
	TestEqual(FString::Printf(TEXT("Alle Normalen zeigen nach aussen (%d taten es nicht)"), InwardNormals),
		InwardNormals, 0);

	// Und die Umlaufrichtung der Dreiecke muss dazu passen — sonst waere die Flaeche
	// im Bild von aussen unsichtbar, obwohl die Normalen stimmen.
	int32 MismatchedWinding = 0;
	for (int32 t = 0; t + 2 < Mesh.Triangles.Num(); t += 3)
	{
		const FVector3f& A = Mesh.Positions[Mesh.Triangles[t]];
		const FVector3f& B = Mesh.Positions[Mesh.Triangles[t + 1]];
		const FVector3f& C = Mesh.Positions[Mesh.Triangles[t + 2]];

		const FVector3f FaceNormal = FVector3f::CrossProduct(B - A, C - A);
		if (FaceNormal.IsNearlyZero())
		{
			continue;
		}

		const FVector3f Centroid = (A + B + C) / 3.f;
		if (FVector3f::DotProduct(FaceNormal.GetSafeNormal(), Centroid.GetSafeNormal()) <= 0.f)
		{
			++MismatchedWinding;
		}
	}

	TestEqual(FString::Printf(TEXT("Umlaufrichtung passt zur Normalen (%d Dreiecke verdreht)"),
		MismatchedWinding), MismatchedWinding, 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMolSurfaceGuardsTest,
	"MolecularForge.Oberflaeche.GrenzfaelleWerdenAbgefangen",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMolSurfaceGuardsTest::RunTest(const FString& Parameters)
{
	using namespace MolecularForge;

	// Leere Struktur.
	{
		UMolecularStructure* Empty = NewObject<UMolecularStructure>();
		FMolMeshData Mesh;
		FString Error;

		TestFalse(TEXT("Leere Struktur ergibt keine Oberflaeche"),
			BuildGaussianSurface(*Empty, MakeTestSurfaceOptions(), Mesh, &Error));
		TestFalse(TEXT("Und einen Grund dafuer"), Error.IsEmpty());
		TestTrue(TEXT("Das Mesh bleibt leer"), Mesh.IsEmpty());
	}

	// Unsinnige Parameter duerfen nicht in eine Endlosschleife oder einen Absturz laufen.
	{
		UMolecularStructure* Structure = MolTest::BuildAtomCloud({ FVector3f::ZeroVector });

		FMolSurfaceOptions Options = MakeTestSurfaceOptions();
		Options.Blobbiness = 1.f;	// muss negativ sein

		FMolMeshData Mesh;
		FString Error;
		TestFalse(TEXT("Positive Blobbiness wird abgewiesen"),
			BuildGaussianSurface(*Structure, Options, Mesh, &Error));
		TestFalse(TEXT("Mit Begruendung"), Error.IsEmpty());
	}

	// Zu hoher Schwellwert: es gibt schlicht nichts zu triangulieren, und das muss als
	// erklaerter Fehlschlag herauskommen und nicht als leeres Mesh ohne Hinweis.
	{
		UMolecularStructure* Structure = MolTest::BuildAtomCloud({ FVector3f::ZeroVector });

		FMolSurfaceOptions Options = MakeTestSurfaceOptions();
		Options.IsoValue = 100.f;

		FMolMeshData Mesh;
		FString Error;
		TestFalse(TEXT("Unerreichbarer Schwellwert ergibt keine Oberflaeche"),
			BuildGaussianSurface(*Structure, Options, Mesh, &Error));
		TestTrue(TEXT("Der Hinweis nennt den Schwellwert"), Error.Contains(TEXT("Schwellwert")));
	}

	// Die Zellenbremse muss greifen, statt Speicher ohne Ende anzufordern.
	{
		UMolecularStructure* Structure = MolTest::BuildAtomCloud({
			FVector3f(0.f, 0.f, 0.f),
			FVector3f(60.f, 60.f, 60.f)
		});

		FMolSurfaceOptions Options = MakeTestSurfaceOptions();
		Options.VoxelSizeAngstrom = 0.05f;
		Options.MaxVoxels = 200000;

		FMolMeshData Mesh;
		FString Error;
		TestTrue(TEXT("Auch mit Bremse entsteht eine Oberflaeche"),
			BuildGaussianSurface(*Structure, Options, Mesh, &Error));
		TestTrue(TEXT("Und sie hat Dreiecke"), Mesh.NumTriangles() > 0);
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
