// Copyright Simulated Flow. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "MolSelection.h"
#include "MolStructureIO.h"
#include "MolecularStructure.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/**
	 * Eine kleine, aber vollstaendige Teststruktur: zwei Proteinketten, ein Ligand und
	 * Wasser. Damit lassen sich alle Auswahlklassen an einem Beispiel pruefen, statt fuer
	 * jede eine eigene Struktur zu bauen.
	 *
	 * Der Weg ueber den PDB-Text ist Absicht: so wird gleich mitgeprueft, dass die Auswahl
	 * auf dem arbeitet, was der Parser tatsaechlich erzeugt — Flags, Kettenzuordnung und
	 * Residuenklassen inbegriffen.
	 */
	const TCHAR* GSelectionTestPdb =
		TEXT("HEADER    TESTFALL                                30-APR-81   1TST              \n")
		TEXT("HELIX    1  H1 ALA A    1  ALA A    2  1                                   2    \n")
		TEXT("ATOM      1  N   ALA A   1      0.000   0.000   0.000  1.00 10.00           N  \n")
		TEXT("ATOM      2  CA  ALA A   1      1.458   0.000   0.000  1.00 20.00           C  \n")
		TEXT("ATOM      3  C   ALA A   1      2.000   1.400   0.000  1.00 30.00           C  \n")
		TEXT("ATOM      4  O   ALA A   1      1.400   2.400   0.000  1.00 40.00           O  \n")
		TEXT("ATOM      5  CB  ALA A   1      2.000  -1.200   0.000  1.00 50.00           C  \n")
		TEXT("ATOM      6  N   GLY A   2      3.300   1.500   0.000  1.00 60.00           N  \n")
		TEXT("ATOM      7  CA  GLY A   2      4.000   2.700   0.000  1.00 70.00           C  \n")
		TEXT("TER       8      GLY A   2                                                      \n")
		TEXT("ATOM      9  N   VAL B  10     20.000   0.000   0.000  1.00 80.00           N  \n")
		TEXT("ATOM     10  CA  VAL B  10     21.458   0.000   0.000  1.00 90.00           C  \n")
		TEXT("HETATM   11 ZN    ZN C 100      5.000   0.000   0.000  1.00 15.00           ZN \n")
		TEXT("HETATM   12  O   HOH D 200     40.000  40.000  40.000  1.00 25.00           O  \n")
		TEXT("END                                                                             \n");

	UMolecularStructure* LoadSelectionTestStructure()
	{
		FMolLoadOptions Options;
		Options.bCenterOnOrigin = false;
		Options.bDiscardWater = false;
		Options.bDeriveBonds = false;

		UMolecularStructure* Structure = NewObject<UMolecularStructure>();
		MolecularForge::ParseStructureText(GSelectionTestPdb, Options, *Structure);
		return Structure;
	}

	int32 CountSelected(const UMolecularStructure& Structure, const TCHAR* Expression, FString& OutError)
	{
		const FMolSelectionResult Result = MolecularForge::SelectAtoms(Structure, Expression);
		OutError = Result.Error;
		return Result.bSuccess ? Result.NumSelected : -1;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMolSelectionClassesTest,
	"MolecularForge.Auswahl.Klassen",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMolSelectionClassesTest::RunTest(const FString& Parameters)
{
	UMolecularStructure* Structure = LoadSelectionTestStructure();

	// Fuenf Atome in ALA, zwei in GLY, zwei in VAL, dazu Zink und ein Wassersauerstoff.
	// Die Seriennummern in der Datei gehen bis 12, weil der TER-Record eine mitverbraucht —
	// Atome sind es trotzdem nur elf.
	TestEqual(TEXT("Teststruktur vollstaendig geladen"), Structure->GetNumAtoms(), 11);

	FString Error;

	TestEqual(TEXT("all"), CountSelected(*Structure, TEXT("all"), Error), 11);
	TestEqual(TEXT("none"), CountSelected(*Structure, TEXT("none"), Error), 0);

	// Sieben Atome in Kette A, zwei in Kette B.
	TestEqual(TEXT("protein"), CountSelected(*Structure, TEXT("protein"), Error), 9);
	TestEqual(TEXT("water"), CountSelected(*Structure, TEXT("water"), Error), 1);

	// Zink und Wasser sind beide HETATM, aber nur Zink ist ein Ligand.
	TestEqual(TEXT("hetero"), CountSelected(*Structure, TEXT("hetero"), Error), 2);
	TestEqual(TEXT("ligand"), CountSelected(*Structure, TEXT("ligand"), Error), 1);

	// Rueckgrat der neun Proteinatome: N, CA, C, O in ALA, N und CA in GLY, N und CA in VAL.
	TestEqual(TEXT("backbone"), CountSelected(*Structure, TEXT("backbone"), Error), 8);
	// Bleibt genau das CB von ALA.
	TestEqual(TEXT("sidechain"), CountSelected(*Structure, TEXT("sidechain"), Error), 1);

	TestEqual(TEXT("nucleic in einem Protein"), CountSelected(*Structure, TEXT("nucleic"), Error), 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMolSelectionTermsTest,
	"MolecularForge.Auswahl.MitArgument",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMolSelectionTermsTest::RunTest(const FString& Parameters)
{
	UMolecularStructure* Structure = LoadSelectionTestStructure();
	FString Error;

	TestEqual(TEXT("chain A"), CountSelected(*Structure, TEXT("chain A"), Error), 7);
	TestEqual(TEXT("chain B"), CountSelected(*Structure, TEXT("chain B"), Error), 2);
	TestEqual(TEXT("chain A+B"), CountSelected(*Structure, TEXT("chain A+B"), Error), 9);
	TestEqual(TEXT("Kleinschreibung der Kette"), CountSelected(*Structure, TEXT("chain a"), Error), 7);

	TestEqual(TEXT("resi 1"), CountSelected(*Structure, TEXT("resi 1"), Error), 5);
	TestEqual(TEXT("resi 1-2"), CountSelected(*Structure, TEXT("resi 1-2"), Error), 7);
	TestEqual(TEXT("resi 1+10"), CountSelected(*Structure, TEXT("resi 1+10"), Error), 7);

	TestEqual(TEXT("resn ALA"), CountSelected(*Structure, TEXT("resn ALA"), Error), 5);
	TestEqual(TEXT("resn ALA+GLY"), CountSelected(*Structure, TEXT("resn ALA+GLY"), Error), 7);

	TestEqual(TEXT("name CA"), CountSelected(*Structure, TEXT("name CA"), Error), 3);
	TestEqual(TEXT("name N+CA"), CountSelected(*Structure, TEXT("name N+CA"), Error), 6);

	// Sechs Kohlenstoffe: CA, C, CB in ALA, CA in GLY, CA in VAL — plus keiner sonst.
	TestEqual(TEXT("element C"), CountSelected(*Structure, TEXT("element C"), Error), 5);
	TestEqual(TEXT("element Zn"), CountSelected(*Structure, TEXT("element Zn"), Error), 1);

	// Der HELIX-Record deckt beide Residuen der Kette A ab.
	TestEqual(TEXT("ss H"), CountSelected(*Structure, TEXT("ss H"), Error), 7);

	// B-Faktoren laufen von 10 bis 90 in Zehnerschritten.
	TestEqual(TEXT("b > 50"), CountSelected(*Structure, TEXT("b > 50"), Error), 4);
	TestEqual(TEXT("b < 25"), CountSelected(*Structure, TEXT("b < 25"), Error), 3);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMolSelectionLogicTest,
	"MolecularForge.Auswahl.Verknuepfung",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMolSelectionLogicTest::RunTest(const FString& Parameters)
{
	UMolecularStructure* Structure = LoadSelectionTestStructure();
	FString Error;

	TestEqual(TEXT("not water"), CountSelected(*Structure, TEXT("not water"), Error), 10);
	TestEqual(TEXT("chain A and name CA"), CountSelected(*Structure, TEXT("chain A and name CA"), Error), 2);
	TestEqual(TEXT("chain A or chain B"), CountSelected(*Structure, TEXT("chain A or chain B"), Error), 9);

	// Zwei Ausdruecke nebeneinander gelten als 'and' — das ist die Schreibweise,
	// die aus PyMOL gewohnt ist.
	TestEqual(TEXT("chain A name CA ohne 'and'"),
		CountSelected(*Structure, TEXT("chain A name CA"), Error), 2);

	// Klammern muessen die Vorrangregel aushebeln koennen.
	TestEqual(TEXT("Klammern"),
		CountSelected(*Structure, TEXT("(chain A or chain B) and name CA"), Error), 3);
	TestEqual(TEXT("Ohne Klammern bindet 'and' staerker"),
		CountSelected(*Structure, TEXT("chain A or chain B and name CA"), Error), 8);

	TestEqual(TEXT("not innerhalb einer Klammer"),
		CountSelected(*Structure, TEXT("protein and not backbone"), Error), 1);

	// `within`: das Zink sitzt bei x=5, das C von ALA bei (2.0, 1.4, 0) — Abstand rund 3,3.
	// Bei 4 A muessen also Zink selbst und einige ALA-Atome erfasst sein, das ferne
	// Wasser bei (40,40,40) dagegen nicht.
	{
		const FMolSelectionResult Near = MolecularForge::SelectAtoms(*Structure, TEXT("within 4 of resn ZN"));
		TestTrue(FString::Printf(TEXT("within laesst sich auswerten: %s"), *Near.Error), Near.bSuccess);
		TestTrue(TEXT("Das Zink selbst ist dabei"), Near.NumSelected >= 1);
		TestTrue(TEXT("Aber nicht die ganze Struktur"), Near.NumSelected < Structure->GetNumAtoms());

		const FMolSelectionResult Far = MolecularForge::SelectAtoms(*Structure, TEXT("within 100 of resn ZN"));
		TestEqual(TEXT("Bei grossem Radius ist alles dabei"), Far.NumSelected, Structure->GetNumAtoms());

		const FMolSelectionResult Tiny = MolecularForge::SelectAtoms(*Structure,
			TEXT("within 0.1 of resn ZN"));
		TestEqual(TEXT("Bei winzigem Radius bleibt nur das Zink"), Tiny.NumSelected, 1);
	}

	// Ein zusammengesetzter Ausdruck, wie ihn jemand tatsaechlich tippen wuerde.
	{
		const FMolSelectionResult Result = MolecularForge::SelectAtoms(*Structure,
			TEXT("(chain A and resi 1) or (within 5 of resn ZN and not water)"));
		TestTrue(FString::Printf(TEXT("Zusammengesetzter Ausdruck: %s"), *Result.Error), Result.bSuccess);
		TestTrue(TEXT("Er waehlt etwas aus"), Result.NumSelected >= 5);
	}

	// Die Maske muss sich in eine Indexliste umwandeln lassen.
	{
		const FMolSelectionResult Result = MolecularForge::SelectAtoms(*Structure, TEXT("name CA"));
		TArray<int32> Indices;
		MolecularForge::MaskToIndices(Result.Mask, Indices);

		TestEqual(TEXT("Indexliste hat die richtige Laenge"), Indices.Num(), Result.NumSelected);
		for (int32 Index : Indices)
		{
			TestEqual(TEXT("Jeder Index zeigt auf ein CA"), Structure->AtomNames[Index], FName("CA"));
		}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMolSelectionErrorTest,
	"MolecularForge.Auswahl.FehlerWerdenErklaert",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMolSelectionErrorTest::RunTest(const FString& Parameters)
{
	// Eine Auswahlsprache wird von Hand getippt, also wird sie auch falsch getippt.
	// Die Rueckmeldung muss dann sagen, was nicht verstanden wurde und wo — sonst
	// probiert der Anwender im Dunkeln herum.
	UMolecularStructure* Structure = LoadSelectionTestStructure();

	auto ExpectFailure = [this, Structure](const TCHAR* Expression, const TCHAR* ExpectedFragment)
	{
		const FMolSelectionResult Result = MolecularForge::SelectAtoms(*Structure, Expression);

		TestFalse(FString::Printf(TEXT("'%s' wird abgewiesen"), Expression), Result.bSuccess);
		if (Result.bSuccess)
		{
			return;
		}

		TestTrue(FString::Printf(TEXT("Die Meldung zu '%s' nennt '%s' (war: %s)"),
			Expression, ExpectedFragment, *Result.Error),
			Result.Error.Contains(ExpectedFragment));
	};

	ExpectFailure(TEXT("quatsch"), TEXT("quatsch"));
	ExpectFailure(TEXT("chain"), TEXT("fehlt"));
	ExpectFailure(TEXT("(chain A"), TEXT("Klammer"));
	ExpectFailure(TEXT("chain A)"), TEXT("Klammer"));
	ExpectFailure(TEXT("element Qq"), TEXT("Elementsymbol"));
	ExpectFailure(TEXT("ss X"), TEXT("Sekundaerstruktur"));
	ExpectFailure(TEXT("b = 50"), TEXT("erwartet"));
	ExpectFailure(TEXT("within 5 resn ZN"), TEXT("of"));
	ExpectFailure(TEXT(""), TEXT("leer"));

	// Die Fehlerposition muss auf die schuldige Stelle zeigen und nicht auf den Anfang.
	{
		const FMolSelectionResult Result = MolecularForge::SelectAtoms(*Structure, TEXT("chain A and quatsch"));
		TestFalse(TEXT("Fehler im hinteren Teil wird gefunden"), Result.bSuccess);
		TestTrue(FString::Printf(TEXT("Die Position zeigt nach hinten (war %d)"), Result.ErrorPosition),
			Result.ErrorPosition > 10);
	}

	// Eine leere Struktur ist kein Absturzgrund.
	{
		UMolecularStructure* Empty = NewObject<UMolecularStructure>();
		const FMolSelectionResult Result = MolecularForge::SelectAtoms(*Empty, TEXT("all"));
		TestFalse(TEXT("Leere Struktur wird abgewiesen"), Result.bSuccess);
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
