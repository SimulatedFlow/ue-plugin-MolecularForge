// Copyright Simulated Flow. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "MolecularStructure.h"
#include "MolSecondaryStructure.h"
#include "MolTestPeptideBuilder.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	// Der ideale Peptidbau steht in MolTestPeptideBuilder.h, seit ihn mehrere Testdateien
	// brauchen: hier fuer die Sekundaerstruktur, daneben fuer das Zurueckmessen der
	// Torsionswinkel. Zweimal dieselbe Geometrie zu pflegen waere die sicherste Art,
	// sie irgendwann auseinanderlaufen zu lassen.
	using MolTestPeptide::BuildIdealPeptide;

	constexpr float GBondNCA = MolTestPeptide::BondNCA;
	constexpr float GBondCAC = MolTestPeptide::BondCAC;
	constexpr float GBondCN = MolTestPeptide::BondCN;

	constexpr float GHelixPhi = MolTestPeptide::HelixPhi;
	constexpr float GHelixPsi = MolTestPeptide::HelixPsi;
	constexpr float GStrandPhi = MolTestPeptide::StrandPhi;
	constexpr float GStrandPsi = MolTestPeptide::StrandPsi;

	int32 CountKind(const UMolecularStructure& Structure, EMolSecondaryStructure Kind)
	{
		int32 Count = 0;
		for (const FMolResidue& Residue : Structure.Residues)
		{
			if (Residue.SecondaryStructure == Kind)
			{
				++Count;
			}
		}
		return Count;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMolIdealGeometryTest,
	"MolecularForge.Sekundaerstruktur.TestgeometrieStimmt",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMolIdealGeometryTest::RunTest(const FString& Parameters)
{
	// Erst pruefen, ob die Testgeometrie selbst stimmt. Sonst wuerde ein Fehler im
	// Kettenbau spaeter wie ein Fehler in der Sekundaerstruktur-Berechnung aussehen.
	UMolecularStructure* Helix = NewObject<UMolecularStructure>();
	BuildIdealPeptide(16, GHelixPhi, GHelixPsi, {}, *Helix);

	TestEqual(TEXT("16 Residuen gebaut"), Helix->GetNumResidues(), 16);
	TestEqual(TEXT("64 Rueckgratatome"), Helix->GetNumAtoms(), 64);

	auto AtomOf = [Helix](int32 Residue, int32 Offset)
	{
		return FVector(Helix->AtomPositions[Residue * 4 + Offset]);
	};

	// Bindungslaengen im gebauten Rueckgrat.
	TestTrue(TEXT("N-CA-Bindungslaenge"),
		FMath::IsNearlyEqual((float)FVector::Dist(AtomOf(3, 0), AtomOf(3, 1)), GBondNCA, 0.01f));
	TestTrue(TEXT("CA-C-Bindungslaenge"),
		FMath::IsNearlyEqual((float)FVector::Dist(AtomOf(3, 1), AtomOf(3, 2)), GBondCAC, 0.01f));
	TestTrue(TEXT("Peptidbindung C-N"),
		FMath::IsNearlyEqual((float)FVector::Dist(AtomOf(3, 2), AtomOf(4, 0)), GBondCN, 0.01f));

	// Der Kern der Alpha-Helix: der Carbonyl-Sauerstoff von Residuum i liegt rund 3 A
	// vom Amid-Stickstoff von i+4 entfernt. Das ist die Wasserstoffbruecke, auf der
	// die ganze Erkennung beruht — stimmt dieser Abstand nicht, ist die Geometrie falsch.
	const double ONDistance = FVector::Dist(AtomOf(4, 3), AtomOf(8, 0));
	TestTrue(FString::Printf(TEXT("O(i) zu N(i+4) betraegt %.2f A, erwartet 2.8 bis 3.2"), ONDistance),
		ONDistance > 2.8 && ONDistance < 3.2);

	// Der gestreckte Strang darf diese Naehe gerade nicht haben.
	UMolecularStructure* Strand = NewObject<UMolecularStructure>();
	BuildIdealPeptide(16, GStrandPhi, GStrandPsi, {}, *Strand);

	const double StrandDistance = FVector::Dist(
		FVector(Strand->AtomPositions[4 * 4 + 3]), FVector(Strand->AtomPositions[8 * 4 + 0]));
	TestTrue(FString::Printf(TEXT("Im Strang betraegt O(i) zu N(i+4) %.2f A, erwartet ueber 8"), StrandDistance),
		StrandDistance > 8.0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMolHelixDetectionTest,
	"MolecularForge.Sekundaerstruktur.HelixWirdErkannt",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMolHelixDetectionTest::RunTest(const FString& Parameters)
{
	UMolecularStructure* Helix = NewObject<UMolecularStructure>();
	BuildIdealPeptide(16, GHelixPhi, GHelixPsi, {}, *Helix);

	MolecularForge::ComputeSecondaryStructure(*Helix);

	const int32 NumHelix = CountKind(*Helix, EMolSecondaryStructure::Helix);

	// Die aeussersten Residuen koennen keine vollstaendigen Bruecken bilden, deshalb
	// wird nicht die ganze Kette erwartet — aber der Kern muss sitzen.
	TestTrue(FString::Printf(TEXT("Mindestens 10 von 16 Residuen als Helix erkannt, waren %d"), NumHelix),
		NumHelix >= 10);

	for (int32 r = 5; r <= 10; ++r)
	{
		TestEqual(FString::Printf(TEXT("Residuum %d im Kern ist Helix"), r),
			Helix->Residues[r].SecondaryStructure, EMolSecondaryStructure::Helix);
	}

	TestEqual(TEXT("Kein Faltblatt in einer Helix"), CountKind(*Helix, EMolSecondaryStructure::Sheet), 0);

	// Gegenprobe: der gestreckte Strang darf keine Helix ergeben. Ein Verfahren, das
	// ueberall Helix findet, wuerde den Test oben sonst bestehen, ohne etwas zu koennen.
	UMolecularStructure* Strand = NewObject<UMolecularStructure>();
	BuildIdealPeptide(16, GStrandPhi, GStrandPsi, {}, *Strand);

	MolecularForge::ComputeSecondaryStructure(*Strand);

	TestEqual(TEXT("Einzelner gestreckter Strang ergibt keine Helix"),
		CountKind(*Strand, EMolSecondaryStructure::Helix), 0);
	TestEqual(TEXT("Ein einzelner Strang allein ergibt auch kein Faltblatt"),
		CountKind(*Strand, EMolSecondaryStructure::Sheet), 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMolProlineBreaksHelixTest,
	"MolecularForge.Sekundaerstruktur.ProlinBrichtDieHelix",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMolProlineBreaksHelixTest::RunTest(const FString& Parameters)
{
	// Prolin hat am Stickstoff keinen Wasserstoff und kann deshalb keine Bruecke stiften.
	// Die Geometrie bleibt hier unveraendert ideal — wenn die Erkennung trotzdem weniger
	// Helix findet, greift genau diese Regel und nichts anderes.
	//
	// Die Kette ist bewusst kurz. In einer langen Helix ueberlappen sich die Definitionen
	// so stark, dass ein einzelner fehlender Turn von den Nachbarn ueberbrueckt wird — dort
	// bliebe die Zuordnung unveraendert, und das ist auch richtig so: ein Prolin mitten in
	// einer langen Helix zerstoert sie nicht, es verbiegt sie nur. Sichtbar wird der Ausfall
	// erst, wenn die Kette kurz genug ist, dass es keine Nachbarn zum Ueberbruecken gibt.
	constexpr int32 NumResidues = 10;
	constexpr int32 ProlinePosition = 5;

	TArray<FName> WithProline;
	for (int32 i = 0; i < NumResidues; ++i)
	{
		WithProline.Add(i == ProlinePosition ? FName("PRO") : FName("ALA"));
	}

	UMolecularStructure* PureHelix = NewObject<UMolecularStructure>();
	BuildIdealPeptide(NumResidues, GHelixPhi, GHelixPsi, {}, *PureHelix);
	MolecularForge::ComputeSecondaryStructure(*PureHelix);

	UMolecularStructure* Broken = NewObject<UMolecularStructure>();
	BuildIdealPeptide(NumResidues, GHelixPhi, GHelixPsi, WithProline, *Broken);
	MolecularForge::ComputeSecondaryStructure(*Broken);

	const int32 PureCount = CountKind(*PureHelix, EMolSecondaryStructure::Helix);
	const int32 BrokenCount = CountKind(*Broken, EMolSecondaryStructure::Helix);

	TestTrue(FString::Printf(TEXT("Die reine Helix wird ueberhaupt erkannt (%d Residuen)"), PureCount),
		PureCount >= 6);
	TestTrue(FString::Printf(TEXT("Prolin verkuerzt die Helix (rein %d, mit Prolin %d)"),
		PureCount, BrokenCount), BrokenCount < PureCount);

	// Das Prolin selbst darf weiter Teil der Helix sein — es kann nur nichts spenden.
	// Was verschwinden muss, ist die Helix-Zuordnung am Anfang der Kette, wo die Bruecke
	// zum Prolin die einzige war, die den 4-Turn getragen hat.
	//
	// Geprueft wird "nicht mehr Helix" und nicht "jetzt Coil": eine ideale Helix erfuellt
	// nebenbei auch die Bedingungen fuer 3- und 5-Turns, sodass das Residuum als Turn
	// zurueckbleibt. Das ist richtig und soll den Test nicht scheitern lassen.
	TestNotEqual(TEXT("Residuum 1 ist ohne die Bruecke zum Prolin keine Helix mehr"),
		Broken->Residues[1].SecondaryStructure, EMolSecondaryStructure::Helix);
	TestEqual(TEXT("In der reinen Helix ist Residuum 1 dagegen Helix"),
		PureHelix->Residues[1].SecondaryStructure, EMolSecondaryStructure::Helix);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMolSecondaryStructurePolicyTest,
	"MolecularForge.Sekundaerstruktur.Ladeoption",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMolSecondaryStructurePolicyTest::RunTest(const FString& Parameters)
{
	using namespace MolecularForge;

	auto MakeHelix = []()
	{
		UMolecularStructure* Structure = NewObject<UMolecularStructure>();
		BuildIdealPeptide(16, GHelixPhi, GHelixPsi, {}, *Structure);
		return Structure;
	};

	{
		// Nur aus der Datei: es wird nicht gerechnet, also bleibt alles Coil.
		UMolecularStructure* Structure = MakeHelix();
		FMolLoadOptions Options;
		Options.SecondaryStructureSource = EMolSecondaryStructureSource::FromFile;
		ApplySecondaryStructurePolicy(*Structure, Options, false);

		TestEqual(TEXT("FromFile rechnet nicht"), CountKind(*Structure, EMolSecondaryStructure::Helix), 0);
	}
	{
		UMolecularStructure* Structure = MakeHelix();
		FMolLoadOptions Options;
		Options.SecondaryStructureSource = EMolSecondaryStructureSource::Compute;
		ApplySecondaryStructurePolicy(*Structure, Options, true);

		TestTrue(TEXT("Compute rechnet auch, wenn die Datei etwas hergab"),
			CountKind(*Structure, EMolSecondaryStructure::Helix) > 0);
	}
	{
		// Der Normalfall fuer AlphaFold: die Datei hatte nichts, also wird gerechnet.
		UMolecularStructure* Structure = MakeHelix();
		FMolLoadOptions Options;
		Options.SecondaryStructureSource = EMolSecondaryStructureSource::FromFileElseCompute;
		ApplySecondaryStructurePolicy(*Structure, Options, false);

		TestTrue(TEXT("Ohne Dateiangabe wird gerechnet"),
			CountKind(*Structure, EMolSecondaryStructure::Helix) > 0);
	}
	{
		// Der Normalfall fuer Kristallstrukturen: die Angabe des Autors bleibt stehen.
		UMolecularStructure* Structure = MakeHelix();
		FMolLoadOptions Options;
		Options.SecondaryStructureSource = EMolSecondaryStructureSource::FromFileElseCompute;
		ApplySecondaryStructurePolicy(*Structure, Options, true);

		TestEqual(TEXT("Mit Dateiangabe wird nicht ueberschrieben"),
			CountKind(*Structure, EMolSecondaryStructure::Helix), 0);
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
