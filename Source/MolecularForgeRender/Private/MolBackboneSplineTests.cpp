// Copyright 2026 Silvan Teufel All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "MolBackboneSpline.h"
#include "MolTestChainBuilder.h"
#include "MolecularStructure.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr float GCaSpacing = MolTest::CaSpacing;

	UMolecularStructure* BuildTestChain(
		const TArray<FVector3f>& AnchorPositions,
		const TArray<FVector3f>& CarbonylDirections)
	{
		return MolTest::BuildChain(AnchorPositions, CarbonylDirections);
	}

	TArray<FVector3f> MakeStraightAnchors(int32 Count)
	{
		return MolTest::MakeStraightAnchors(Count);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMolBackboneFrameTest,
	"MolecularForge.Rueckgrat.BandKipptNichtImFaltblatt",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMolBackboneFrameTest::RunTest(const FString& Parameters)
{
	// In einem Faltblatt zeigen die Carbonylgruppen abwechselnd nach oben und unten.
	// Genau hier geht ein naiv gebautes Band kaputt: es kippt bei jedem Residuum um 180
	// Grad. Diese Kette bildet den Fall nach.
	constexpr int32 NumResidues = 12;

	TArray<FVector3f> Carbonyls;
	Carbonyls.Reserve(NumResidues);
	for (int32 i = 0; i < NumResidues; ++i)
	{
		Carbonyls.Add((i % 2 == 0) ? FVector3f::ZAxisVector : -FVector3f::ZAxisVector);
	}

	UMolecularStructure* Structure = BuildTestChain(MakeStraightAnchors(NumResidues), Carbonyls);

	TArray<FMolBackboneSegment> Segments;
	MolecularForge::BuildBackboneSegments(*Structure, FMolBackboneOptions(), Segments);

	TestEqual(TEXT("Ein durchgehender Abschnitt"), Segments.Num(), 1);
	if (Segments.Num() != 1)
	{
		return false;
	}

	const TArray<FMolBackbonePoint>& Points = Segments[0].Points;
	TestTrue(TEXT("Es entstehen Punkte"), Points.Num() > NumResidues);

	// Kein Punkt darf gegen seinen Vorgaenger zeigen. Ohne die Umdrehkorrektur waeren
	// hier bei jedem zweiten Residuum Werte um -1 zu sehen.
	float SchlechtesterUebergang = 1.f;
	int32 SchlechterIndex = INDEX_NONE;

	for (int32 i = 1; i < Points.Num(); ++i)
	{
		const float Dot = FVector3f::DotProduct(Points[i].Right, Points[i - 1].Right);
		if (Dot < SchlechtesterUebergang)
		{
			SchlechtesterUebergang = Dot;
			SchlechterIndex = i;
		}
	}

	TestTrue(FString::Printf(
		TEXT("Querrichtung dreht nirgends um (schlechtester Uebergang %.3f bei Punkt %d)"),
		SchlechtesterUebergang, SchlechterIndex), SchlechtesterUebergang > 0.f);

	// Und das Koordinatensystem muss ueberall vollstaendig und rechtwinklig sein,
	// sonst waere jedes daraus gebaute Profil verzerrt.
	for (int32 i = 0; i < Points.Num(); ++i)
	{
		const FMolBackbonePoint& P = Points[i];

		if (!FMath::IsNearlyEqual(P.Forward.Size(), 1.f, 0.01f)
			|| !FMath::IsNearlyEqual(P.Right.Size(), 1.f, 0.01f)
			|| !FMath::IsNearlyEqual(P.Up.Size(), 1.f, 0.01f))
		{
			AddError(FString::Printf(TEXT("Punkt %d hat nicht normierte Achsen."), i));
			break;
		}

		if (FMath::Abs(FVector3f::DotProduct(P.Forward, P.Right)) > 0.01f
			|| FMath::Abs(FVector3f::DotProduct(P.Forward, P.Up)) > 0.01f
			|| FMath::Abs(FVector3f::DotProduct(P.Right, P.Up)) > 0.01f)
		{
			AddError(FString::Printf(TEXT("Punkt %d hat nicht rechtwinklige Achsen."), i));
			break;
		}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMolBackboneInterpolationTest,
	"MolecularForge.Rueckgrat.KurveLaeuftDurchDieAnker",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMolBackboneInterpolationTest::RunTest(const FString& Parameters)
{
	// Catmull-Rom laeuft durch seine Stuetzpunkte, eine B-Spline nicht. Bei gemessenen
	// Atompositionen ist das der Unterschied zwischen "zeigt, wo das Atom ist" und
	// "zeigt ungefaehr dorthin". Der Test haelt diese Entscheidung fest.
	constexpr int32 NumResidues = 8;
	constexpr int32 SegmentsPerResidue = 6;

	// Eine gekruemmte Kette, damit der Test nicht auf einer Geraden trivial wird.
	TArray<FVector3f> Anchors;
	for (int32 i = 0; i < NumResidues; ++i)
	{
		const float Angle = i * 0.6f;
		Anchors.Add(FVector3f(i * GCaSpacing, FMath::Sin(Angle) * 2.f, FMath::Cos(Angle) * 2.f));
	}

	UMolecularStructure* Structure = BuildTestChain(Anchors, {});

	FMolBackboneOptions Options;
	Options.SegmentsPerResidue = SegmentsPerResidue;

	TArray<FMolBackboneSegment> Segments;
	MolecularForge::BuildBackboneSegments(*Structure, Options, Segments);

	TestEqual(TEXT("Ein Abschnitt"), Segments.Num(), 1);
	if (Segments.Num() != 1)
	{
		return false;
	}

	const TArray<FMolBackbonePoint>& Points = Segments[0].Points;

	// Sieben Spannen mal sechs Schritte, plus der Schlusspunkt.
	TestEqual(TEXT("Erwartete Punktzahl"), Points.Num(), (NumResidues - 1) * SegmentsPerResidue + 1);

	for (int32 i = 0; i < NumResidues; ++i)
	{
		const int32 PointIndex = i * SegmentsPerResidue;
		if (!Points.IsValidIndex(PointIndex))
		{
			AddError(FString::Printf(TEXT("Zu Anker %d gibt es keinen Punkt."), i));
			break;
		}

		const float Distance = FVector3f::Dist(Points[PointIndex].Position, Anchors[i]);
		if (Distance > 0.001f)
		{
			AddError(FString::Printf(
				TEXT("Die Kurve verfehlt Anker %d um %.4f A."), i, Distance));
			break;
		}
	}

	// Alpha muss von 0 bis 1 durchlaufen.
	TestTrue(TEXT("Alpha beginnt bei 0"), FMath::IsNearlyEqual(Points[0].Alpha, 0.f, 0.001f));
	TestTrue(TEXT("Alpha endet bei 1"), FMath::IsNearlyEqual(Points.Last().Alpha, 1.f, 0.001f));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMolBackboneGapTest,
	"MolecularForge.Rueckgrat.LueckeTrenntDenStrang",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMolBackboneGapTest::RunTest(const FString& Parameters)
{
	// In Kristallstrukturen fehlen bewegliche Schleifen regelmaessig. Ueber so eine
	// Luecke hinweg zu interpolieren waere erfunden — das Band zeigte einen Weg, den
	// niemand gemessen hat. Erwartet werden deshalb zwei getrennte Abschnitte.
	TArray<FVector3f> Anchors;
	for (int32 i = 0; i < 5; ++i)
	{
		Anchors.Add(FVector3f(i * GCaSpacing, 0.f, 0.f));
	}
	// 20 A Sprung — weit jenseits der 3,8 A, die zwei verbundene CA-Atome trennen.
	for (int32 i = 0; i < 5; ++i)
	{
		Anchors.Add(FVector3f(4 * GCaSpacing + 20.f + i * GCaSpacing, 0.f, 0.f));
	}

	UMolecularStructure* Structure = BuildTestChain(Anchors, {});

	TArray<FMolBackboneSegment> Segments;
	MolecularForge::BuildBackboneSegments(*Structure, FMolBackboneOptions(), Segments);

	TestEqual(TEXT("Die Luecke trennt in zwei Abschnitte"), Segments.Num(), 2);

	if (Segments.Num() == 2)
	{
		TestTrue(TEXT("Beide Abschnitte haben Punkte"),
			Segments[0].Points.Num() > 1 && Segments[1].Points.Num() > 1);

		// Kein Punkt darf in der Luecke liegen.
		const float LueckeStart = 4 * GCaSpacing;
		const float LueckeEnde = 4 * GCaSpacing + 20.f;

		for (const FMolBackboneSegment& Segment : Segments)
		{
			for (const FMolBackbonePoint& Point : Segment.Points)
			{
				if (Point.Position.X > LueckeStart + 0.1f && Point.Position.X < LueckeEnde - 0.1f)
				{
					AddError(FString::Printf(
						TEXT("Ein Punkt liegt bei X=%.2f mitten in der Luecke."), Point.Position.X));
					return false;
				}
			}
		}
	}

	// Eine Kette ohne Rueckgrat darf gar keinen Abschnitt ergeben.
	{
		UMolecularStructure* Ligand = NewObject<UMolecularStructure>();
		Ligand->PreallocateAtoms(1);
		FMolChain& Chain = Ligand->Chains.AddDefaulted_GetRef();
		Chain.Id = FName("B");
		Chain.FirstResidue = 0;
		Chain.NumResidues = 1;
		Chain.FirstAtom = 0;
		Chain.NumAtoms = 1;

		FMolResidue& Residue = Ligand->Residues.AddDefaulted_GetRef();
		Residue.Name = FName("ZN");
		Residue.ChainIndex = 0;
		Residue.FirstAtom = 0;
		Residue.NumAtoms = 1;

		Ligand->AtomPositions[0] = FVector3f::ZeroVector;
		Ligand->AtomNames[0] = FName("ZN");
		Ligand->AtomElements[0] = 30;
		Ligand->AtomResidueIndices[0] = 0;
		Ligand->AtomBFactors[0] = 0.f;
		Ligand->AtomOccupancies[0] = 1.f;
		Ligand->AtomFlags[0] = MolAtom_Hetatm;
		Ligand->FinalizeAfterLoad();

		TArray<FMolBackboneSegment> LigandSegments;
		MolecularForge::BuildBackboneSegments(*Ligand, FMolBackboneOptions(), LigandSegments);

		TestEqual(TEXT("Ein einzelnes Ion hat kein Rueckgrat"), LigandSegments.Num(), 0);
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
