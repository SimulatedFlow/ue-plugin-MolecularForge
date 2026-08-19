// Copyright Simulated Flow. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "MolMeasurement.h"
#include "MolSelection.h"
#include "MolTestPeptideBuilder.h"
#include "MolElementTable.h"
#include "MolecularStructure.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/** Baut eine Struktur aus frei gesetzten Punkten, um Formeln von Hand nachzurechnen. */
	UMolecularStructure* BuildPointStructure(const TArray<FVector3f>& Points, uint8 Element = 6)
	{
		UMolecularStructure* Structure = NewObject<UMolecularStructure>();
		Structure->PreallocateAtoms(Points.Num());

		FMolChain& Chain = Structure->Chains.AddDefaulted_GetRef();
		Chain.Id = FName("A");
		Chain.FirstResidue = 0;
		Chain.NumResidues = Points.Num();
		Chain.FirstAtom = 0;
		Chain.NumAtoms = Points.Num();

		for (int32 i = 0; i < Points.Num(); ++i)
		{
			FMolResidue& Residue = Structure->Residues.AddDefaulted_GetRef();
			Residue.Name = FName("LIG");
			Residue.SequenceNumber = i + 1;
			Residue.ChainIndex = 0;
			Residue.FirstAtom = i;
			Residue.NumAtoms = 1;

			Structure->AtomPositions[i] = Points[i];
			Structure->AtomNames[i] = FName("C");
			Structure->AtomElements[i] = Element;
			Structure->AtomResidueIndices[i] = i;
			Structure->AtomBFactors[i] = 0.f;
			Structure->AtomOccupancies[i] = 1.f;
			Structure->AtomFlags[i] = MolAtom_Hetatm;
		}

		Structure->FinalizeAfterLoad();
		return Structure;
	}

	TBitArray<> AllOf(const UMolecularStructure& Structure)
	{
		return TBitArray<>(true, Structure.GetNumAtoms());
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMolMeasureBasicTest,
	"MolecularForge.Messung.AbstandUndWinkel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMolMeasureBasicTest::RunTest(const FString& Parameters)
{
	using namespace MolecularForge;

	// Ein rechter Winkel mit Schenkeln der Laenge 3 und 4 — die Hypotenuse ist dann 5.
	UMolecularStructure* Structure = BuildPointStructure({
		FVector3f(3.f, 0.f, 0.f),
		FVector3f(0.f, 0.f, 0.f),
		FVector3f(0.f, 4.f, 0.f)
	});

	TestTrue(TEXT("Abstand 3"),
		FMath::IsNearlyEqual(MeasureDistance(*Structure, 0, 1), 3.f, 0.001f));
	TestTrue(TEXT("Abstand 5 ueber die Hypotenuse"),
		FMath::IsNearlyEqual(MeasureDistance(*Structure, 0, 2), 5.f, 0.001f));
	TestTrue(TEXT("Winkel 90 Grad"),
		FMath::IsNearlyEqual(MeasureAngle(*Structure, 0, 1, 2), 90.f, 0.01f));

	// Gestreckt sind es 180 Grad.
	UMolecularStructure* Straight = BuildPointStructure({
		FVector3f(-1.f, 0.f, 0.f),
		FVector3f(0.f, 0.f, 0.f),
		FVector3f(1.f, 0.f, 0.f)
	});
	TestTrue(TEXT("Gestreckter Winkel ist 180 Grad"),
		FMath::IsNearlyEqual(MeasureAngle(*Straight, 0, 1, 2), 180.f, 0.01f));

	// Ungueltige Indizes duerfen nicht danebengreifen.
	TestTrue(TEXT("Ungueltiger Abstand meldet sich"), MeasureDistance(*Structure, 0, 99) < 0.f);
	TestTrue(TEXT("Ungueltiger Winkel meldet sich"), MeasureAngle(*Structure, 0, 1, 99) < 0.f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMolMeasureDihedralTest,
	"MolecularForge.Messung.Torsionswinkel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMolMeasureDihedralTest::RunTest(const FString& Parameters)
{
	using namespace MolecularForge;

	// Erst gegen eine von Hand nachgerechnete Anordnung. Die Punkte sind so gewaehlt,
	// dass sich die Formel im Kopf ausrechnen laesst: die beiden Ebenen stehen senkrecht
	// aufeinander, der Torsionswinkel muss also 90 Grad betragen.
	{
		UMolecularStructure* Structure = BuildPointStructure({
			FVector3f(1.f, 0.f, 0.f),
			FVector3f(0.f, 0.f, 0.f),
			FVector3f(0.f, 1.f, 0.f),
			FVector3f(0.f, 1.f, 1.f)
		});

		const float Dihedral = MeasureDihedral(*Structure, 0, 1, 2, 3);
		TestTrue(FString::Printf(TEXT("Rechtwinklige Anordnung ergibt -90 Grad (gemessen %.2f)"), Dihedral),
			FMath::IsNearlyEqual(Dihedral, -90.f, 0.01f));

		// Das Vorzeichen ist bei Torsionen die halbe Aussage. Spiegelt man den letzten
		// Punkt, muss der Winkel das Vorzeichen wechseln — ein Verfahren ueber den
		// Arkuskosinus koennte das nicht und lieferte beide Male 90.
		UMolecularStructure* Mirrored = BuildPointStructure({
			FVector3f(1.f, 0.f, 0.f),
			FVector3f(0.f, 0.f, 0.f),
			FVector3f(0.f, 1.f, 0.f),
			FVector3f(0.f, 1.f, -1.f)
		});

		const float MirroredDihedral = MeasureDihedral(*Mirrored, 0, 1, 2, 3);
		TestTrue(FString::Printf(TEXT("Gespiegelt ergibt +90 Grad (gemessen %.2f)"), MirroredDihedral),
			FMath::IsNearlyEqual(MirroredDihedral, 90.f, 0.01f));
	}

	// Und nun der eigentliche Zweck: die Torsionswinkel einer gebauten Kette wieder
	// zurueckmessen. Der Kettenbau setzt Atome aus Winkeln, die Messung holt Winkel aus
	// Atomen — zwei entgegengesetzte Rechnungen, die sich gegenseitig pruefen.
	{
		UMolecularStructure* Helix = NewObject<UMolecularStructure>();
		MolTestPeptide::BuildIdealPeptide(12, MolTestPeptide::HelixPhi, MolTestPeptide::HelixPsi, {}, *Helix);

		auto AtomOf = [](int32 Residue, int32 Offset) { return Residue * 4 + Offset; };

		for (int32 r = 2; r <= 8; ++r)
		{
			const float Phi = MeasureDihedral(*Helix,
				AtomOf(r - 1, MolTestPeptide::OffsetC),
				AtomOf(r, MolTestPeptide::OffsetN),
				AtomOf(r, MolTestPeptide::OffsetCA),
				AtomOf(r, MolTestPeptide::OffsetC));

			const float Psi = MeasureDihedral(*Helix,
				AtomOf(r, MolTestPeptide::OffsetN),
				AtomOf(r, MolTestPeptide::OffsetCA),
				AtomOf(r, MolTestPeptide::OffsetC),
				AtomOf(r + 1, MolTestPeptide::OffsetN));

			if (!FMath::IsNearlyEqual(Phi, MolTestPeptide::HelixPhi, 0.1f))
			{
				AddError(FString::Printf(TEXT("Residuum %d: phi ist %.2f statt %.2f"),
					r, Phi, MolTestPeptide::HelixPhi));
				break;
			}
			if (!FMath::IsNearlyEqual(Psi, MolTestPeptide::HelixPsi, 0.1f))
			{
				AddError(FString::Printf(TEXT("Residuum %d: psi ist %.2f statt %.2f"),
					r, Psi, MolTestPeptide::HelixPsi));
				break;
			}
		}

		// Omega steht in trans, also bei 180 Grad.
		const float Omega = MeasureDihedral(*Helix,
			AtomOf(4, MolTestPeptide::OffsetCA),
			AtomOf(4, MolTestPeptide::OffsetC),
			AtomOf(5, MolTestPeptide::OffsetN),
			AtomOf(5, MolTestPeptide::OffsetCA));

		TestTrue(FString::Printf(TEXT("Omega steht bei 180 Grad (gemessen %.2f)"), Omega),
			FMath::IsNearlyEqual(FMath::Abs(Omega), 180.f, 0.1f));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMolHelixHandednessTest,
	"MolecularForge.Messung.HelixIstRechtsgaengig",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMolHelixHandednessTest::RunTest(const FString& Parameters)
{
	// Diese Pruefung braucht keine Torsionskonvention und ist deshalb der einzige wirklich
	// aeussere Anker der Testgeometrie: eine Alpha-Helix in Proteinen ist rechtsgaengig,
	// das ist eine Tatsache ueber die Welt und nicht ueber unseren Code. Waeren Kettenbau
	// und Torsionsmessung beide mit demselben Vorzeichenfehler behaftet, hoben sie sich
	// gegenseitig auf — hier nicht.
	UMolecularStructure* Helix = NewObject<UMolecularStructure>();
	MolTestPeptide::BuildIdealPeptide(16, MolTestPeptide::HelixPhi, MolTestPeptide::HelixPsi, {}, *Helix);

	auto CA = [Helix](int32 Residue)
	{
		return Helix->AtomPositions[Residue * 4 + MolTestPeptide::OffsetCA];
	};

	// Der Abstand zweier aufeinanderfolgender CA-Atome liegt bei 3,8 A — das gilt in jeder
	// Peptidkette und ist die erste Probe darauf, dass ueberhaupt eine Kette entstanden ist.
	for (int32 r = 0; r < 15; ++r)
	{
		const float Step = FVector3f::Dist(CA(r), CA(r + 1));
		if (!FMath::IsNearlyEqual(Step, 3.8f, 0.05f))
		{
			AddError(FString::Printf(TEXT("CA-Abstand zwischen %d und %d ist %.2f A statt 3.8"),
				r, r + 1, Step));
			return false;
		}
	}

	// Die Haendigkeit ergibt sich aus dem Spatprodukt dreier aufeinanderfolgender Sehnen.
	// Das kommt ohne jede Achsenschaetzung aus — eine Achse aus Anfangs- und Endpunkt zu
	// naehern ist bei rund viereinhalb Windungen ungenau genug, um einzelne Schritte
	// kippen zu lassen. Das Spatprodukt ist dagegen rein lokal: bei einer rechtsgaengigen
	// Schraube ist es positiv, bei einer linksgaengigen negativ.
	int32 RightHandedSteps = 0;
	int32 LeftHandedSteps = 0;

	for (int32 r = 2; r <= 11; ++r)
	{
		const FVector3f D1 = CA(r + 1) - CA(r);
		const FVector3f D2 = CA(r + 2) - CA(r + 1);
		const FVector3f D3 = CA(r + 3) - CA(r + 2);

		const float Triple = FVector3f::DotProduct(D1, FVector3f::CrossProduct(D2, D3));

		if (Triple > 0.f) { ++RightHandedSteps; }
		else if (Triple < 0.f) { ++LeftHandedSteps; }
	}

	TestTrue(FString::Printf(
		TEXT("Die Helix ist durchgehend rechtsgaengig (%d rechts, %d links)"),
		RightHandedSteps, LeftHandedSteps),
		RightHandedSteps > 0 && LeftHandedSteps == 0);

	// Gegenprobe: dieselbe Kette mit umgedrehten Torsionswinkeln muss linksgaengig sein.
	// Ohne diese Probe koennte das Spatprodukt aus einem ganz anderen Grund positiv sein.
	{
		UMolecularStructure* Mirror = NewObject<UMolecularStructure>();
		MolTestPeptide::BuildIdealPeptide(16, -MolTestPeptide::HelixPhi, -MolTestPeptide::HelixPsi,
			{}, *Mirror);

		auto MirrorCA = [Mirror](int32 Residue)
		{
			return Mirror->AtomPositions[Residue * 4 + MolTestPeptide::OffsetCA];
		};

		int32 MirrorLeft = 0;
		for (int32 r = 2; r <= 11; ++r)
		{
			const FVector3f D1 = MirrorCA(r + 1) - MirrorCA(r);
			const FVector3f D2 = MirrorCA(r + 2) - MirrorCA(r + 1);
			const FVector3f D3 = MirrorCA(r + 3) - MirrorCA(r + 2);

			if (FVector3f::DotProduct(D1, FVector3f::CrossProduct(D2, D3)) < 0.f)
			{
				++MirrorLeft;
			}
		}

		TestTrue(FString::Printf(
			TEXT("Mit umgedrehten Winkeln wird die Helix linksgaengig (%d Schritte)"), MirrorLeft),
			MirrorLeft > 0);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMolMeasureAggregateTest,
	"MolecularForge.Messung.SchwerpunktUndAusdehnung",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMolMeasureAggregateTest::RunTest(const FString& Parameters)
{
	using namespace MolecularForge;

	// Vier Punkte symmetrisch um den Ursprung.
	UMolecularStructure* Structure = BuildPointStructure({
		FVector3f(-1.f, 0.f, 0.f),
		FVector3f(1.f, 0.f, 0.f),
		FVector3f(0.f, -1.f, 0.f),
		FVector3f(0.f, 1.f, 0.f)
	});

	const TBitArray<> All = AllOf(*Structure);

	TestTrue(TEXT("Mittelpunkt liegt im Ursprung"),
		ComputeCentroid(*Structure, All).IsNearlyZero(0.001));

	// Bei gleichen Elementen faellt der Massenschwerpunkt mit dem Mittelpunkt zusammen.
	TestTrue(TEXT("Massenschwerpunkt gleich Mittelpunkt bei gleichen Atomen"),
		ComputeCenterOfMass(*Structure, All).IsNearlyZero(0.001));

	// Alle Punkte haben Abstand 1 vom Mittelpunkt, also ist der Traegheitsradius 1.
	TestTrue(TEXT("Traegheitsradius ist 1"),
		FMath::IsNearlyEqual(ComputeRadiusOfGyration(*Structure, All), 1.f, 0.001f));

	const FBox Bounds = ComputeSelectionBounds(*Structure, All);
	TestTrue(TEXT("Huelle stimmt"),
		Bounds.Min.Equals(FVector(-1, -1, 0), 0.001) && Bounds.Max.Equals(FVector(1, 1, 0), 0.001));

	// Jetzt mit einem schweren Atom auf einer Seite: der Massenschwerpunkt muss dorthin
	// wandern, der geometrische Mittelpunkt darf sich nicht ruehren. Genau dieser
	// Unterschied ist der Grund, beide Funktionen anzubieten.
	{
		UMolecularStructure* Mixed = BuildPointStructure({
			FVector3f(-1.f, 0.f, 0.f),
			FVector3f(1.f, 0.f, 0.f)
		});
		Mixed->AtomElements[1] = 26;	// Eisen statt Kohlenstoff

		const TBitArray<> MixedAll = AllOf(*Mixed);

		TestTrue(TEXT("Geometrischer Mittelpunkt bleibt im Ursprung"),
			ComputeCentroid(*Mixed, MixedAll).IsNearlyZero(0.001));

		const FVector CenterOfMass = ComputeCenterOfMass(*Mixed, MixedAll);
		TestTrue(FString::Printf(TEXT("Massenschwerpunkt wandert zum Eisen (x = %.3f)"), CenterOfMass.X),
			CenterOfMass.X > 0.5);

		// Nachrechnen: (12,011 * -1 + 55,845 * 1) / (12,011 + 55,845).
		const float Expected =
			(GetAtomicMass(6) * -1.f + GetAtomicMass(26) * 1.f) / (GetAtomicMass(6) + GetAtomicMass(26));
		TestTrue(TEXT("Massenschwerpunkt stimmt mit der Rechnung ueberein"),
			FMath::IsNearlyEqual(static_cast<float>(CenterOfMass.X), Expected, 0.001f));
	}

	// Teilauswahl: nur die beiden Punkte auf der x-Achse.
	{
		TBitArray<> Half(false, Structure->GetNumAtoms());
		Half[0] = true;
		Half[1] = true;

		TestTrue(TEXT("Teilauswahl liefert eigenen Mittelpunkt"),
			ComputeCentroid(*Structure, Half).IsNearlyZero(0.001));

		const FBox HalfBounds = ComputeSelectionBounds(*Structure, Half);
		TestTrue(TEXT("Huelle der Teilauswahl ist flacher"),
			FMath::IsNearlyEqual(HalfBounds.Max.Y, 0.0, 0.001));
	}

	// Leere Auswahl darf nicht durch null teilen.
	{
		const TBitArray<> Nothing(false, Structure->GetNumAtoms());
		TestTrue(TEXT("Leere Auswahl ergibt den Ursprung"),
			ComputeCentroid(*Structure, Nothing).IsNearlyZero());
		TestTrue(TEXT("Leere Auswahl ergibt Traegheitsradius null"),
			FMath::IsNearlyEqual(ComputeRadiusOfGyration(*Structure, Nothing), 0.f, 0.001f));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMolRmsdTest,
	"MolecularForge.Messung.Abweichung",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMolRmsdTest::RunTest(const FString& Parameters)
{
	using namespace MolecularForge;

	TArray<FVector3f> A = { FVector3f::ZeroVector, FVector3f(1.f, 0.f, 0.f), FVector3f(0.f, 2.f, 0.f) };
	TArray<FVector3f> B = A;

	float Rmsd = -1.f;
	TestTrue(TEXT("Gleiche Saetze lassen sich vergleichen"), ComputeRmsd(A, B, Rmsd));
	TestTrue(TEXT("Gleiche Saetze haben Abweichung null"), FMath::IsNearlyEqual(Rmsd, 0.f, 0.0001f));

	// Alle Punkte um genau 3 verschoben: dann ist die Abweichung ebenfalls 3.
	for (FVector3f& Point : B)
	{
		Point.X += 3.f;
	}
	TestTrue(TEXT("Verschobene Saetze lassen sich vergleichen"), ComputeRmsd(A, B, Rmsd));
	TestTrue(FString::Printf(TEXT("Gleichmaessige Verschiebung ergibt genau diesen Wert (%.4f)"), Rmsd),
		FMath::IsNearlyEqual(Rmsd, 3.f, 0.0001f));

	// Nur ein Punkt bewegt sich, um 3: die Abweichung ueber drei Punkte ist dann
	// sqrt(9/3) = sqrt(3) — der Test rechnet die Formel nach und nimmt sie nicht hin.
	B = A;
	B[1].X += 3.f;
	TestTrue(TEXT("Einzelne Abweichung"), ComputeRmsd(A, B, Rmsd));
	TestTrue(FString::Printf(TEXT("Ein bewegter Punkt von dreien ergibt sqrt(3) (%.4f)"), Rmsd),
		FMath::IsNearlyEqual(Rmsd, FMath::Sqrt(3.f), 0.0001f));

	// Mit Maske zaehlt nur der ausgewaehlte Punkt, die Abweichung ist dann volle 3.
	{
		TBitArray<> Mask(false, 3);
		Mask[1] = true;

		float MaskedRmsd = -1.f;
		TestTrue(TEXT("Maskierter Vergleich klappt"), ComputeRmsdMasked(A, B, Mask, MaskedRmsd));
		TestTrue(FString::Printf(TEXT("Nur der bewegte Punkt zaehlt (%.4f)"), MaskedRmsd),
			FMath::IsNearlyEqual(MaskedRmsd, 3.f, 0.0001f));

		// Eine Maske ohne gesetzte Bits hat nichts zu vergleichen.
		const TBitArray<> Empty(false, 3);
		TestFalse(TEXT("Leere Maske wird abgelehnt"), ComputeRmsdMasked(A, B, Empty, MaskedRmsd));
	}

	// Verschieden lange Saetze duerfen nicht stillschweigend halb verglichen werden.
	{
		TArray<FVector3f> Shorter = { FVector3f::ZeroVector };
		TestFalse(TEXT("Verschiedene Laengen werden abgelehnt"), ComputeRmsd(A, Shorter, Rmsd));
		TestFalse(TEXT("Leere Saetze werden abgelehnt"),
			ComputeRmsd(TArrayView<const FVector3f>(), TArrayView<const FVector3f>(), Rmsd));
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
