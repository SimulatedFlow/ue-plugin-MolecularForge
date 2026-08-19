// Copyright Simulated Flow. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "MolecularStructure.h"
#include "MolSecondaryStructure.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	// Standardgeometrie des Proteinrueckgrats (Engh & Huber). Bindungslaengen in Angstroem,
	// Winkel in Grad. Diese Zahlen sind Lehrbuchwerte und stehen hier bewusst als Konstanten,
	// damit im Test sichtbar bleibt, woher die Testgeometrie kommt.
	constexpr float GBondNCA = 1.458f;
	constexpr float GBondCAC = 1.525f;
	constexpr float GBondCN  = 1.329f;
	constexpr float GBondCO  = 1.231f;

	constexpr float GAngleNCAC = 111.2f;
	constexpr float GAngleCACN = 116.2f;
	constexpr float GAngleCNCA = 121.7f;
	constexpr float GAngleCACO = 120.8f;

	/** Torsionswinkel einer idealen Alpha-Helix. */
	constexpr float GHelixPhi = -57.f;
	constexpr float GHelixPsi = -47.f;

	/** Torsionswinkel eines gestreckten Beta-Strangs. */
	constexpr float GStrandPhi = -139.f;
	constexpr float GStrandPsi = 135.f;

	/**
	 * Setzt ein Atom relativ zu drei bekannten Atomen (NeRF-Verfahren).
	 * Gegeben sind Bindungslaenge C-D, Bindungswinkel B-C-D und Torsionswinkel A-B-C-D.
	 */
	FVector3f PlaceAtom(const FVector3f& A, const FVector3f& B, const FVector3f& C,
		float BondLength, float BondAngleDeg, float TorsionDeg)
	{
		const float Theta = FMath::DegreesToRadians(BondAngleDeg);
		const float Torsion = FMath::DegreesToRadians(TorsionDeg);

		const FVector3f BC = (C - B).GetSafeNormal();
		const FVector3f Normal = FVector3f::CrossProduct(B - A, BC).GetSafeNormal();
		const FVector3f InPlane = FVector3f::CrossProduct(Normal, BC);

		return C
			+ BC * (-BondLength * FMath::Cos(Theta))
			+ InPlane * (BondLength * FMath::Sin(Theta) * FMath::Cos(Torsion))
			+ Normal * (BondLength * FMath::Sin(Theta) * FMath::Sin(Torsion));
	}

	/**
	 * Baut eine Peptidkette mit idealer Geometrie und festen Torsionswinkeln.
	 *
	 * Der Sinn: die Sekundaerstruktur-Berechnung gegen bekannte Physik pruefen statt gegen
	 * selbst gesetzte Erwartungswerte. Eine Kette mit phi=-57 und psi=-47 *ist* eine
	 * Alpha-Helix — wenn das Verfahren dort keine findet, liegt es am Verfahren.
	 */
	void BuildIdealPeptide(int32 NumResidues, float PhiDeg, float PsiDeg,
		const TArray<FName>& ResidueNames, UMolecularStructure& Out)
	{
		Out.Reset();

		TArray<FVector3f> N, CA, C, O;
		N.SetNum(NumResidues);
		CA.SetNum(NumResidues);
		C.SetNum(NumResidues);
		O.SetNum(NumResidues);

		// Erstes Residuum als Startpunkt in der xy-Ebene.
		N[0] = FVector3f::ZeroVector;
		CA[0] = FVector3f(GBondNCA, 0.f, 0.f);

		const float StartAngle = FMath::DegreesToRadians(GAngleNCAC);
		CA[0] += FVector3f::ZeroVector;
		C[0] = CA[0] + FVector3f(-FMath::Cos(StartAngle), FMath::Sin(StartAngle), 0.f) * GBondCAC;

		for (int32 i = 0; i < NumResidues; ++i)
		{
			// Der Carbonyl-Sauerstoff steht dem folgenden Stickstoff gegenueber.
			O[i] = PlaceAtom(N[i], CA[i], C[i], GBondCO, GAngleCACO, PsiDeg + 180.f);

			if (i + 1 < NumResidues)
			{
				N[i + 1] = PlaceAtom(N[i], CA[i], C[i], GBondCN, GAngleCACN, PsiDeg);
				// Omega ist in trans-Konfiguration festgenagelt, wie in fast allen Proteinen.
				CA[i + 1] = PlaceAtom(CA[i], C[i], N[i + 1], GBondNCA, GAngleCNCA, 180.f);
				C[i + 1] = PlaceAtom(C[i], N[i + 1], CA[i + 1], GBondCAC, GAngleNCAC, PhiDeg);
			}
		}

		// In die Struktur uebertragen: vier Rueckgratatome je Residuum, in fester Reihenfolge.
		const int32 NumAtoms = NumResidues * 4;
		Out.PreallocateAtoms(NumAtoms);

		FMolChain& Chain = Out.Chains.AddDefaulted_GetRef();
		Chain.Id = FName("A");
		Chain.FirstResidue = 0;
		Chain.NumResidues = NumResidues;
		Chain.FirstAtom = 0;
		Chain.NumAtoms = NumAtoms;

		static const FName NameN(TEXT("N"));
		static const FName NameCA(TEXT("CA"));
		static const FName NameC(TEXT("C"));
		static const FName NameO(TEXT("O"));

		int32 AtomIndex = 0;
		for (int32 i = 0; i < NumResidues; ++i)
		{
			FMolResidue& Residue = Out.Residues.AddDefaulted_GetRef();
			Residue.Name = ResidueNames.IsValidIndex(i) ? ResidueNames[i] : FName("ALA");
			Residue.SequenceNumber = i + 1;
			Residue.ChainIndex = 0;
			Residue.FirstAtom = AtomIndex;
			Residue.NumAtoms = 4;

			const FVector3f Positions[4] = { N[i], CA[i], C[i], O[i] };
			const FName Names[4] = { NameN, NameCA, NameC, NameO };
			const uint8 Elements[4] = { 7, 6, 6, 8 };

			for (int32 k = 0; k < 4; ++k)
			{
				Out.AtomPositions[AtomIndex] = Positions[k];
				Out.AtomNames[AtomIndex] = Names[k];
				Out.AtomElements[AtomIndex] = Elements[k];
				Out.AtomResidueIndices[AtomIndex] = i;
				Out.AtomBFactors[AtomIndex] = 0.f;
				Out.AtomOccupancies[AtomIndex] = 1.f;
				Out.AtomFlags[AtomIndex] = MolAtom_Backbone | (k == 1 ? MolAtom_Anchor : 0);
				++AtomIndex;
			}
		}

		Out.FinalizeAfterLoad();
	}

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
