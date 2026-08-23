// Copyright 2026 Silvan Teufel All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "MolNiagaraArrays.h"
#include "MolecularStructure.h"
#include "MolElementTable.h"

#if WITH_DEV_AUTOMATION_TESTS

// Geprueft wird die Datenaufbereitung, nicht Niagara selbst. Ein laufendes Partikelsystem
// im Automationstest waere ein Test der Engine und nicht des Plugins — und headless ohne
// Renderer ohnehin nicht sinnvoll zu bewerten. Die Uebergabe an Niagara ist danach ein
// Dreizeiler ohne eigene Logik.

namespace
{
	/**
	 * Baut eine kleine gemischte Struktur: vier Proteinatome mit Rueckgrat- und
	 * Ankerkennzeichnung, ein Wassersauerstoff, ein Wasserstoff und ein Ligandenatom.
	 * Damit lassen sich alle Filterwege in einem Durchgang pruefen.
	 */
	UMolecularStructure* BuildMixedStructure()
	{
		UMolecularStructure* Structure = NewObject<UMolecularStructure>();

		constexpr int32 NumAtoms = 7;
		Structure->PreallocateAtoms(NumAtoms);

		FMolChain& Chain = Structure->Chains.AddDefaulted_GetRef();
		Chain.Id = FName("A");
		Chain.FirstResidue = 0;
		Chain.NumResidues = 3;
		Chain.FirstAtom = 0;
		Chain.NumAtoms = NumAtoms;

		struct FSpec
		{
			const TCHAR* Name;
			uint8 Element;
			uint8 Flags;
			int32 Residue;
		};

		const FSpec Specs[NumAtoms] =
		{
			{ TEXT("N"),  7, MolAtom_Backbone, 0 },
			{ TEXT("CA"), 6, static_cast<uint8>(MolAtom_Backbone | MolAtom_Anchor), 0 },
			{ TEXT("C"),  6, MolAtom_Backbone, 0 },
			{ TEXT("CB"), 6, MolAtom_None, 0 },
			{ TEXT("O"),  8, static_cast<uint8>(MolAtom_Hetatm | MolAtom_Water), 1 },
			{ TEXT("H"),  1, MolAtom_None, 0 },
			{ TEXT("ZN"), 30, MolAtom_Hetatm, 2 }
		};

		// Drei Residuen: Protein, Wasser, Ligand.
		const TCHAR* ResidueNames[3] = { TEXT("ALA"), TEXT("HOH"), TEXT("ZN") };
		const int32 ResidueFirstAtom[3] = { 0, 4, 6 };
		const int32 ResidueNumAtoms[3] = { 4, 1, 1 };

		for (int32 r = 0; r < 3; ++r)
		{
			FMolResidue& Residue = Structure->Residues.AddDefaulted_GetRef();
			Residue.Name = FName(ResidueNames[r]);
			Residue.SequenceNumber = r + 1;
			Residue.ChainIndex = 0;
			Residue.FirstAtom = ResidueFirstAtom[r];
			Residue.NumAtoms = ResidueNumAtoms[r];
		}
		// Der Wasserstoff haengt am ersten Residuum, steht aber hinter dem Wasser —
		// die Zuordnung geht ueber AtomResidueIndices, nicht ueber die Reihenfolge.
		Structure->Residues[0].NumAtoms = 4;

		for (int32 a = 0; a < NumAtoms; ++a)
		{
			Structure->AtomPositions[a] = FVector3f(a * 1.5f, 0.f, 0.f);
			Structure->AtomNames[a] = FName(Specs[a].Name);
			Structure->AtomElements[a] = Specs[a].Element;
			Structure->AtomResidueIndices[a] = Specs[a].Residue;
			Structure->AtomBFactors[a] = 0.f;
			Structure->AtomOccupancies[a] = 1.f;
			Structure->AtomFlags[a] = Specs[a].Flags;
		}

		// Zwei Bindungen: eine zwischen sichtbaren Atomen, eine zum Wasserstoff.
		Structure->Bonds.Add(FMolBond{ 0, 1, 1 });
		Structure->Bonds.Add(FMolBond{ 1, 5, 1 });

		Structure->FinalizeAfterLoad();
		return Structure;
	}

	FMolNiagaraOptions MakeTestOptions()
	{
		FMolNiagaraOptions Options;
		Options.UnitsPerAngstrom = 10.f;
		return Options;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMolNiagaraFilterTest,
	"MolecularForge.Niagara.FilterWirkenWieBeiDenKugeln",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMolNiagaraFilterTest::RunTest(const FString& Parameters)
{
	using namespace MolecularForge;

	UMolecularStructure* Structure = BuildMixedStructure();

	// Standard: kein Wasser, kein Wasserstoff. Bleiben N, CA, C, CB und Zink.
	{
		FMolNiagaraArrays Arrays;
		BuildNiagaraArrays(*Structure, MakeTestOptions(), Arrays);

		TestEqual(TEXT("Wasser und Wasserstoff fallen weg"), Arrays.Num(), 5);
		TestEqual(TEXT("Farben je Atom"), Arrays.Colors.Num(), Arrays.Num());
		TestEqual(TEXT("Radien je Atom"), Arrays.Radii.Num(), Arrays.Num());
	}

	// Mit Wasser und Wasserstoff sind es alle sieben.
	{
		FMolNiagaraOptions Options = MakeTestOptions();
		Options.bShowWater = true;
		Options.bShowHydrogen = true;

		FMolNiagaraArrays Arrays;
		BuildNiagaraArrays(*Structure, Options, Arrays);
		TestEqual(TEXT("Mit allem sind es sieben"), Arrays.Num(), 7);
	}

	// Rueckgrat: N, CA, C plus die Heterogruppe Zink. Wasser ist zwar HETATM, faellt
	// aber schon am Wasserfilter heraus — das ist die Reihenfolge, die auch die
	// Kugeldarstellung anwendet.
	{
		FMolNiagaraOptions Options = MakeTestOptions();
		Options.AtomSubset = EMolRepresentation::Backbone;

		FMolNiagaraArrays Arrays;
		BuildNiagaraArrays(*Structure, Options, Arrays);
		TestEqual(TEXT("Rueckgrat plus Ligand"), Arrays.Num(), 4);
	}

	// CA-Spur: nur das Ankeratom und der Ligand.
	{
		FMolNiagaraOptions Options = MakeTestOptions();
		Options.AtomSubset = EMolRepresentation::AlphaTrace;

		FMolNiagaraArrays Arrays;
		BuildNiagaraArrays(*Structure, Options, Arrays);
		TestEqual(TEXT("Anker plus Ligand"), Arrays.Num(), 2);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMolNiagaraScalingTest,
	"MolecularForge.Niagara.MassstabUndFarbenStimmen",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMolNiagaraScalingTest::RunTest(const FString& Parameters)
{
	using namespace MolecularForge;

	UMolecularStructure* Structure = BuildMixedStructure();

	FMolNiagaraOptions Options = MakeTestOptions();
	Options.UnitsPerAngstrom = 10.f;

	FMolNiagaraArrays Arrays;
	BuildNiagaraArrays(*Structure, Options, Arrays);

	// Das erste uebernommene Atom ist der Stickstoff bei x = 0.
	TestTrue(TEXT("Erstes Atom liegt im Ursprung"), Arrays.Positions[0].IsNearlyZero());

	// Das zweite ist CA bei 1,5 A — in Unreal-Einheiten also 15.
	TestTrue(FString::Printf(TEXT("Zweites Atom bei 15 Einheiten (war %.2f)"), Arrays.Positions[1].X),
		FMath::IsNearlyEqual(Arrays.Positions[1].X, 15.0, 0.01));

	// Radien werden ebenfalls umgerechnet, sonst passten Partikelgroesse und Abstand
	// nicht zueinander — ein Fehler, den man erst im laufenden Effekt saehe.
	const float ExpectedNitrogenRadius = GetElement(7).VdWRadius * 10.f;
	TestTrue(FString::Printf(TEXT("Stickstoffradius umgerechnet (%.2f erwartet, %.2f erhalten)"),
		ExpectedNitrogenRadius, Arrays.Radii[0]),
		FMath::IsNearlyEqual(Arrays.Radii[0], ExpectedNitrogenRadius, 0.01f));

	// Die Farben muessen mit denen der Kugeldarstellung uebereinstimmen, sonst zeigten
	// Mesh und Partikel dasselbe Molekuel in verschiedenen Farben.
	const FLinearColor Expected = Structure->GetAtomColor(0, Options.ColorScheme, Options.UniformColor);
	TestTrue(TEXT("Farbe stimmt mit der Kugeldarstellung ueberein"),
		Arrays.Colors[0].Equals(Expected, 0.001f));

	// Ein anderer Massstab muss durchschlagen.
	{
		FMolNiagaraOptions Doubled = Options;
		Doubled.UnitsPerAngstrom = 20.f;

		FMolNiagaraArrays DoubledArrays;
		BuildNiagaraArrays(*Structure, Doubled, DoubledArrays);

		TestTrue(TEXT("Doppelter Massstab verdoppelt die Abstaende"),
			FMath::IsNearlyEqual(DoubledArrays.Positions[1].X, 30.0, 0.01));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMolNiagaraLimitTest,
	"MolecularForge.Niagara.AusduennenBleibtGleichmaessig",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMolNiagaraLimitTest::RunTest(const FString& Parameters)
{
	using namespace MolecularForge;

	// Eine lange Kette entlang X. Wird sie gekuerzt, muss die Auswahl ueber die ganze
	// Laenge verteilt bleiben — vorne abzuschneiden waere einfacher, wuerde aber sichtbar
	// das halbe Molekuel weglassen.
	constexpr int32 NumAtoms = 100;

	UMolecularStructure* Structure = NewObject<UMolecularStructure>();
	Structure->PreallocateAtoms(NumAtoms);

	FMolChain& Chain = Structure->Chains.AddDefaulted_GetRef();
	Chain.Id = FName("A");
	Chain.FirstResidue = 0;
	Chain.NumResidues = NumAtoms;
	Chain.FirstAtom = 0;
	Chain.NumAtoms = NumAtoms;

	for (int32 a = 0; a < NumAtoms; ++a)
	{
		FMolResidue& Residue = Structure->Residues.AddDefaulted_GetRef();
		Residue.Name = FName("LIG");
		Residue.ChainIndex = 0;
		Residue.FirstAtom = a;
		Residue.NumAtoms = 1;

		Structure->AtomPositions[a] = FVector3f(static_cast<float>(a), 0.f, 0.f);
		Structure->AtomNames[a] = FName("C");
		Structure->AtomElements[a] = 6;
		Structure->AtomResidueIndices[a] = a;
		Structure->AtomBFactors[a] = 0.f;
		Structure->AtomOccupancies[a] = 1.f;
		Structure->AtomFlags[a] = MolAtom_Hetatm;
	}
	Structure->FinalizeAfterLoad();

	FMolNiagaraOptions Options = MakeTestOptions();
	Options.UnitsPerAngstrom = 1.f;
	Options.MaxAtoms = 10;

	FMolNiagaraArrays Arrays;
	BuildNiagaraArrays(*Structure, Options, Arrays);

	TestEqual(TEXT("Auf die Obergrenze gekuerzt"), Arrays.Num(), 10);
	TestEqual(TEXT("Die urspruengliche Zahl wird gemeldet"), Arrays.NumAtomsBeforeLimit, NumAtoms);

	// Der letzte uebernommene Punkt muss weit hinten liegen. Waere vorne abgeschnitten
	// worden, laege er bei 9 statt bei ueber 80.
	const double LastX = Arrays.Positions.Last().X;
	TestTrue(FString::Printf(TEXT("Die Auswahl reicht bis ans Ende (letztes Atom bei x=%.1f)"), LastX),
		LastX > 80.0);

	// Und die Abstaende zwischen den ausgewaehlten Punkten muessen gleichmaessig sein.
	double MinStep = TNumericLimits<double>::Max();
	double MaxStep = 0.0;
	for (int32 i = 1; i < Arrays.Num(); ++i)
	{
		const double Step = Arrays.Positions[i].X - Arrays.Positions[i - 1].X;
		MinStep = FMath::Min(MinStep, Step);
		MaxStep = FMath::Max(MaxStep, Step);
	}
	TestTrue(FString::Printf(TEXT("Gleichmaessige Schritte (%.1f bis %.1f)"), MinStep, MaxStep),
		MaxStep - MinStep <= 1.0);

	// Ohne Obergrenze bleibt alles erhalten.
	{
		FMolNiagaraOptions Unlimited = Options;
		Unlimited.MaxAtoms = 0;

		FMolNiagaraArrays All;
		BuildNiagaraArrays(*Structure, Unlimited, All);
		TestEqual(TEXT("Ohne Obergrenze bleibt alles"), All.Num(), NumAtoms);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMolNiagaraBondsTest,
	"MolecularForge.Niagara.BindungenEndenNichtImLeeren",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMolNiagaraBondsTest::RunTest(const FString& Parameters)
{
	using namespace MolecularForge;

	UMolecularStructure* Structure = BuildMixedStructure();

	FMolNiagaraOptions Options = MakeTestOptions();
	Options.bIncludeBonds = true;

	FMolNiagaraArrays Arrays;
	BuildNiagaraArrays(*Structure, Options, Arrays);

	TestEqual(TEXT("Start- und Endpunkte sind gleich viele"),
		Arrays.BondStarts.Num(), Arrays.BondEnds.Num());

	// Von den zwei Bindungen fuehrt eine zum Wasserstoff, der herausgefiltert wurde.
	// Sie darf nicht uebergeben werden, sonst zeigte sie ins Leere.
	TestEqual(TEXT("Nur die Bindung zwischen sichtbaren Atomen bleibt"), Arrays.BondStarts.Num(), 1);

	// Und ihre Endpunkte muessen auf den tatsaechlichen Atompositionen liegen.
	const FVector ExpectedStart = FVector(Structure->AtomPositions[0]) * Options.UnitsPerAngstrom;
	const FVector ExpectedEnd = FVector(Structure->AtomPositions[1]) * Options.UnitsPerAngstrom;

	TestTrue(TEXT("Startpunkt liegt auf dem Atom"), Arrays.BondStarts[0].Equals(ExpectedStart, 0.01));
	TestTrue(TEXT("Endpunkt liegt auf dem Atom"), Arrays.BondEnds[0].Equals(ExpectedEnd, 0.01));

	// Ohne die Option bleiben die Bindungsarrays leer.
	{
		FMolNiagaraArrays Without;
		BuildNiagaraArrays(*Structure, MakeTestOptions(), Without);
		TestEqual(TEXT("Ohne Option keine Bindungen"), Without.BondStarts.Num(), 0);
	}

	// Leere Struktur darf nicht stolpern.
	{
		UMolecularStructure* Empty = NewObject<UMolecularStructure>();
		FMolNiagaraArrays Arrays2;
		BuildNiagaraArrays(*Empty, Options, Arrays2);
		TestTrue(TEXT("Leere Struktur ergibt leere Arrays"), Arrays2.IsEmpty());
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
