// Copyright 2026 Silvan Teufel All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "MolecularStructure.h"
#include "MolPdbParser.h"
#include "MolCifParser.h"
#include "MolStructureIO.h"
#include "MolElementTable.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/**
	 * Ausschnitt aus 1CRN (Crambin), woertlich im PDB-Spaltenformat.
	 * Bewusst nicht programmatisch erzeugt: waeren Testdaten und Parser aus derselben
	 * Formatannahme gebaut, wuerde ein Spaltenversatz in beiden stecken und der Test
	 * ihn nie sehen. Diese Zeilen kommen aus dem echten Archiv.
	 */
	const TCHAR* GCrambinFragment =
		TEXT("HEADER    PLANT PROTEIN                           30-APR-81   1CRN              \n")
		TEXT("TITLE     WATER STRUCTURE OF A HYDROPHOBIC PROTEIN AT ATOMIC RESOLUTION          \n")
		TEXT("EXPDTA    X-RAY DIFFRACTION                                                     \n")
		TEXT("HELIX    1  H1 ILE A    7  PRO A   19  1                                  13    \n")
		TEXT("ATOM      1  N   THR A   1      17.047  14.099   3.625  1.00 13.79           N  \n")
		TEXT("ATOM      2  CA  THR A   1      16.967  12.784   4.338  1.00 10.80           C  \n")
		TEXT("ATOM      3  C   THR A   1      15.685  12.755   5.133  1.00  9.19           C  \n")
		TEXT("ATOM      4  O   THR A   1      15.268  13.825   5.594  1.00  9.85           O  \n")
		TEXT("ATOM      5  CB  THR A   1      18.170  12.703   5.337  1.00 13.02           C  \n")
		TEXT("ATOM      6  OG1 THR A   1      19.334  12.829   4.463  1.00 15.06           O  \n")
		TEXT("ATOM      7  CG2 THR A   1      18.150  11.546   6.304  1.00 14.23           C  \n")
		TEXT("ATOM      8  N   THR A   2      15.115  11.555   5.265  1.00  7.81           N  \n")
		TEXT("ATOM      9  CA  THR A   2      13.856  11.469   6.066  1.00  8.31           C  \n")
		TEXT("TER      10      THR A   2                                                      \n")
		TEXT("HETATM   11 ZN    ZN B 100      10.000  10.000  10.000  1.00 20.00          ZN  \n")
		TEXT("HETATM   12  O   HOH B 200       5.000   5.000   5.000  1.00 30.00           O  \n")
		TEXT("END                                                                             \n");

	/**
	 * Derselbe Ausschnitt als mmCIF, Atom fuer Atom deckungsgleich zum PDB-Text oben.
	 * Die Trennung von Kette A und B kommt hier nicht aus einem TER-Record, sondern aus
	 * der wechselnden asym-ID — beide Wege muessen zum selben Ergebnis fuehren.
	 */
	const TCHAR* GCrambinCif =
		TEXT("data_1CRN\n")
		TEXT("#\n")
		TEXT("_entry.id   1CRN\n")
		TEXT("#\n")
		TEXT("_struct.title\n")
		TEXT(";WATER STRUCTURE OF A HYDROPHOBIC PROTEIN AT ATOMIC RESOLUTION\n")
		TEXT(";\n")
		TEXT("#\n")
		TEXT("_exptl.method            'X-RAY DIFFRACTION'\n")
		TEXT("_refine.ls_d_res_high    1.500\n")
		TEXT("#\n")
		TEXT("loop_\n")
		TEXT("_struct_conf.conf_type_id\n")
		TEXT("_struct_conf.id\n")
		TEXT("_struct_conf.beg_auth_asym_id\n")
		TEXT("_struct_conf.beg_auth_seq_id\n")
		TEXT("_struct_conf.end_auth_asym_id\n")
		TEXT("_struct_conf.end_auth_seq_id\n")
		TEXT("HELX_P HELX_P1 A 7 A 19\n")
		TEXT("#\n")
		TEXT("loop_\n")
		TEXT("_atom_site.group_PDB\n")
		TEXT("_atom_site.id\n")
		TEXT("_atom_site.type_symbol\n")
		TEXT("_atom_site.label_atom_id\n")
		TEXT("_atom_site.label_alt_id\n")
		TEXT("_atom_site.label_comp_id\n")
		TEXT("_atom_site.label_asym_id\n")
		TEXT("_atom_site.label_seq_id\n")
		TEXT("_atom_site.pdbx_PDB_ins_code\n")
		TEXT("_atom_site.Cartn_x\n")
		TEXT("_atom_site.Cartn_y\n")
		TEXT("_atom_site.Cartn_z\n")
		TEXT("_atom_site.occupancy\n")
		TEXT("_atom_site.B_iso_or_equiv\n")
		TEXT("_atom_site.auth_seq_id\n")
		TEXT("_atom_site.auth_comp_id\n")
		TEXT("_atom_site.auth_asym_id\n")
		TEXT("_atom_site.auth_atom_id\n")
		TEXT("_atom_site.pdbx_PDB_model_num\n")
		TEXT("ATOM   1  N  N   . THR A 1 ? 17.047 14.099 3.625 1.00 13.79 1   THR A N   1\n")
		TEXT("ATOM   2  C  CA  . THR A 1 ? 16.967 12.784 4.338 1.00 10.80 1   THR A CA  1\n")
		TEXT("ATOM   3  C  C   . THR A 1 ? 15.685 12.755 5.133 1.00  9.19 1   THR A C   1\n")
		TEXT("ATOM   4  O  O   . THR A 1 ? 15.268 13.825 5.594 1.00  9.85 1   THR A O   1\n")
		TEXT("ATOM   5  C  CB  . THR A 1 ? 18.170 12.703 5.337 1.00 13.02 1   THR A CB  1\n")
		TEXT("ATOM   6  O  OG1 . THR A 1 ? 19.334 12.829 4.463 1.00 15.06 1   THR A OG1 1\n")
		TEXT("ATOM   7  C  CG2 . THR A 1 ? 18.150 11.546 6.304 1.00 14.23 1   THR A CG2 1\n")
		TEXT("ATOM   8  N  N   . THR A 2 ? 15.115 11.555 5.265 1.00  7.81 2   THR A N   1\n")
		TEXT("ATOM   9  C  CA  . THR A 2 ? 13.856 11.469 6.066 1.00  8.31 2   THR A CA  1\n")
		TEXT("HETATM 10 ZN ZN  . ZN  B . ? 10.000 10.000 10.000 1.00 20.00 100 ZN  B ZN  1\n")
		TEXT("HETATM 11 O  O   . HOH B . ?  5.000  5.000  5.000 1.00 30.00 200 HOH B O   1\n")
		TEXT("#\n");

	FMolLoadOptions MakeTestOptions()
	{
		FMolLoadOptions Options;
		// Zentrieren aus, damit die Testkoordinaten mit den Dateiwerten vergleichbar bleiben.
		Options.bCenterOnOrigin = false;
		Options.bDiscardWater = false;
		Options.bDeriveBonds = true;
		return Options;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMolPdbParserBasicTest,
	"MolecularForge.Parser.PdbGrundlagen",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMolPdbParserBasicTest::RunTest(const FString& Parameters)
{
	UMolecularStructure* Structure = NewObject<UMolecularStructure>();
	const FMolParseResult Result = MolecularForge::ParsePdb(GCrambinFragment, MakeTestOptions(), *Structure);

	TestTrue(TEXT("Laden erfolgreich"), Result.bSuccess);
	TestEqual(TEXT("Keine fehlerhaften Zeilen"), Result.NumMalformedLines, 0);

	// 9 Proteinatome + Zink + Wassersauerstoff.
	TestEqual(TEXT("Atomzahl"), Structure->GetNumAtoms(), 11);

	// THR 1, THR 2, ZN 100, HOH 200.
	TestEqual(TEXT("Residuenzahl"), Structure->GetNumResidues(), 4);

	// Kette A endet mit TER; Zink und Wasser liegen in Kette B.
	TestEqual(TEXT("Kettenzahl"), Structure->GetNumChains(), 2);

	// ---- Kopfdaten ----
	TestEqual(TEXT("PDB-Code aus HEADER"), Structure->Meta.Identifier, FString(TEXT("1CRN")));
	TestTrue(TEXT("Titel gelesen"), Structure->Meta.Title.Contains(TEXT("HYDROPHOBIC PROTEIN")));
	TestTrue(TEXT("Methode gelesen"), Structure->Meta.ExperimentalMethod.Contains(TEXT("X-RAY")));
	TestFalse(TEXT("Kein AlphaFold, also B-Faktor kein pLDDT"), Structure->Meta.bBFactorIsPLDDT);

	// ---- Elementerkennung ----
	TestEqual(TEXT("Atom 0 ist Stickstoff"), static_cast<int32>(Structure->AtomElements[0]), 7);
	TestEqual(TEXT("Atom 1 ist Kohlenstoff"), static_cast<int32>(Structure->AtomElements[1]), 6);
	TestEqual(TEXT("Atom 3 ist Sauerstoff"), static_cast<int32>(Structure->AtomElements[3]), 8);
	// Der kritische Fall: "ZN" darf nicht als Stickstoff durchgehen.
	TestEqual(TEXT("Atom 9 ist Zink"), static_cast<int32>(Structure->AtomElements[9]), 30);

	// ---- Koordinaten ----
	const FVector FirstAtom = Structure->GetAtomPosition(0);
	TestTrue(TEXT("X von Atom 0"), FMath::IsNearlyEqual(FirstAtom.X, 17.047, 0.001));
	TestTrue(TEXT("Y von Atom 0"), FMath::IsNearlyEqual(FirstAtom.Y, 14.099, 0.001));
	TestTrue(TEXT("Z von Atom 0"), FMath::IsNearlyEqual(FirstAtom.Z, 3.625, 0.001));
	TestTrue(TEXT("B-Faktor von Atom 0"), FMath::IsNearlyEqual(Structure->AtomBFactors[0], 13.79f, 0.001f));

	// ---- Gruppierung ----
	TestEqual(TEXT("Residuum 0 heisst THR"), Structure->Residues[0].Name, FName("THR"));
	TestEqual(TEXT("Residuum 0 hat 7 Atome"), Structure->Residues[0].NumAtoms, 7);
	TestEqual(TEXT("Residuum 1 hat 2 Atome"), Structure->Residues[1].NumAtoms, 2);
	TestEqual(TEXT("Einbuchstaben-Code von THR"), static_cast<int32>(Structure->Residues[0].OneLetterCode), static_cast<int32>('T'));
	TestEqual(TEXT("Kette A ist Protein"), Structure->Chains[0].Kind, EMolChainKind::Protein);
	TestEqual(TEXT("Sequenz der Kette A"), Structure->GetChainSequence(0), FString(TEXT("TT")));

	// ---- Atomflags ----
	TestTrue(TEXT("N ist Rueckgrat"), (Structure->AtomFlags[0] & MolAtom_Backbone) != 0);
	TestTrue(TEXT("CA ist Ankeratom"), (Structure->AtomFlags[1] & MolAtom_Anchor) != 0);
	TestFalse(TEXT("CB ist kein Rueckgrat"), (Structure->AtomFlags[4] & MolAtom_Backbone) != 0);
	TestTrue(TEXT("Zink ist HETATM"), (Structure->AtomFlags[9] & MolAtom_Hetatm) != 0);
	TestTrue(TEXT("Wasser ist als Wasser markiert"), (Structure->AtomFlags[10] & MolAtom_Water) != 0);

	// ---- Bindungen ----
	// Erwartet im ersten THR: N-CA, CA-C, C-O, CA-CB, CB-OG1, CB-CG2 sowie C-N zum
	// naechsten Residuum und dort N-CA. Das freie Zink darf keine Bindung bekommen.
	TestTrue(TEXT("Bindungen abgeleitet"), Structure->GetNumBonds() >= 6);

	auto HasBond = [Structure](int32 A, int32 B)
	{
		const int32 Lo = FMath::Min(A, B);
		const int32 Hi = FMath::Max(A, B);
		return Structure->Bonds.ContainsByPredicate([Lo, Hi](const FMolBond& Bond)
		{
			return Bond.AtomA == Lo && Bond.AtomB == Hi;
		});
	};

	TestTrue(TEXT("N-CA gebunden"), HasBond(0, 1));
	TestTrue(TEXT("CA-C gebunden"), HasBond(1, 2));
	TestTrue(TEXT("C-O gebunden"), HasBond(2, 3));
	TestTrue(TEXT("CA-CB gebunden"), HasBond(1, 4));
	TestTrue(TEXT("Peptidbindung C(1)-N(2)"), HasBond(2, 7));
	TestFalse(TEXT("N und O des Rueckgrats sind nicht direkt gebunden"), HasBond(0, 3));

	const bool bZincBonded = Structure->Bonds.ContainsByPredicate([](const FMolBond& Bond)
	{
		return Bond.AtomA == 9 || Bond.AtomB == 9;
	});
	TestFalse(TEXT("Freies Zink bleibt ungebunden"), bZincBonded);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMolPdbLoadOptionsTest,
	"MolecularForge.Parser.Ladeoptionen",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMolPdbLoadOptionsTest::RunTest(const FString& Parameters)
{
	{
		FMolLoadOptions Options = MakeTestOptions();
		Options.bDiscardWater = true;

		UMolecularStructure* Structure = NewObject<UMolecularStructure>();
		const FMolParseResult Result = MolecularForge::ParsePdb(GCrambinFragment, Options, *Structure);

		TestTrue(TEXT("Laden erfolgreich"), Result.bSuccess);
		TestEqual(TEXT("Wasser verworfen"), Structure->GetNumAtoms(), 10);
		TestEqual(TEXT("Ein verworfenes Atom gezaehlt"), Result.NumAtomsDiscarded, 1);
	}

	{
		FMolLoadOptions Options = MakeTestOptions();
		Options.bCenterOnOrigin = true;

		UMolecularStructure* Structure = NewObject<UMolecularStructure>();
		const FMolParseResult Result = MolecularForge::ParsePdb(GCrambinFragment, Options, *Structure);

		TestTrue(TEXT("Laden erfolgreich"), Result.bSuccess);

		const FBox Bounds = Structure->GetBoundsAngstrom();
		TestTrue(TEXT("Schwerpunkt liegt im Ursprung"), Bounds.GetCenter().IsNearlyZero(0.01));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMolAlphaFoldDetectionTest,
	"MolecularForge.Parser.AlphaFoldErkennung",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMolAlphaFoldDetectionTest::RunTest(const FString& Parameters)
{
	// AlphaFold-Dateien tragen ihre Herkunft in den REMARK-Zeilen und legen den
	// pLDDT-Konfidenzwert in das B-Faktor-Feld. Beides muss erkannt werden, sonst
	// faerbt die B-Faktor-Darstellung auf der falschen Skala.
	const TCHAR* AlphaFoldSnippet =
		TEXT("HEADER                                            01-JUN-22                     \n")
		TEXT("TITLE     ALPHAFOLD MONOMER V2.0 PREDICTION FOR HEMOGLOBIN SUBUNIT ALPHA        \n")
		TEXT("REMARK   1  THIS MODEL WAS PRODUCED BY ALPHAFOLD                                \n")
		TEXT("ATOM      1  N   MET A   1      -8.123   4.567  12.345  1.00 95.12           N  \n")
		TEXT("ATOM      2  CA  MET A   1      -7.456   3.210  11.876  1.00 93.44           C  \n")
		TEXT("END                                                                             \n");

	UMolecularStructure* Structure = NewObject<UMolecularStructure>();
	const FMolParseResult Result = MolecularForge::ParsePdb(AlphaFoldSnippet, MakeTestOptions(), *Structure);

	TestTrue(TEXT("Laden erfolgreich"), Result.bSuccess);
	TestTrue(TEXT("Als AlphaFold erkannt"), Structure->Meta.bBFactorIsPLDDT);
	TestEqual(TEXT("Quelle gesetzt"), Structure->Meta.Source, EMolStructureSource::AlphaFoldDb);
	TestTrue(TEXT("Attributionstext vorhanden"), Structure->Meta.Attribution.Contains(TEXT("CC-BY")));

	// pLDDT 95 liegt in der obersten Konfidenzstufe und muss dunkelblau sein,
	// nicht die auf die Datei normierte B-Faktor-Skala.
	const FLinearColor Color = Structure->GetAtomColor(0, EMolColorScheme::BFactor);
	TestTrue(TEXT("Hohe Konfidenz ergibt Blau"), Color.B > Color.R && Color.B > Color.G);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMolElementTableTest,
	"MolecularForge.Elementtabelle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMolElementTableTest::RunTest(const FString& Parameters)
{
	using namespace MolecularForge;

	TestEqual(TEXT("Symbol C"), static_cast<int32>(AtomicNumberFromSymbol(TEXT(" C"))), 6);
	TestEqual(TEXT("Symbol FE gross"), static_cast<int32>(AtomicNumberFromSymbol(TEXT("FE"))), 26);
	TestEqual(TEXT("Symbol Fe gemischt"), static_cast<int32>(AtomicNumberFromSymbol(TEXT("Fe"))), 26);
	TestEqual(TEXT("Unbekanntes Symbol"), static_cast<int32>(AtomicNumberFromSymbol(TEXT("Qq"))), 0);

	// Der klassische Stolperstein: im Atomnamen bedeutet " CA " Kohlenstoff-alpha,
	// "CA  " dagegen Calcium. Nur die Spaltenposition unterscheidet die beiden.
	TestEqual(TEXT("' CA ' ist Kohlenstoff"),
		static_cast<int32>(GuessAtomicNumberFromAtomName(TEXT(" CA "), false)), 6);
	TestEqual(TEXT("'CA  ' im HETATM ist Calcium"),
		static_cast<int32>(GuessAtomicNumberFromAtomName(TEXT("CA  "), true)), 20);
	// Und in einem ATOM-Record ist "HG11" Wasserstoff, nicht Quecksilber.
	TestEqual(TEXT("'HG11' ist Wasserstoff"),
		static_cast<int32>(GuessAtomicNumberFromAtomName(TEXT("HG11"), false)), 1);

	TestTrue(TEXT("Kohlenstoffradius plausibel"),
		FMath::IsNearlyEqual(GetElement(6).VdWRadius, 1.70f, 0.001f));
	TestEqual(TEXT("Unbekannt faellt auf Index 0"), FString(GetElement(200).Symbol), FString(TEXT("X")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMolCifMatchesPdbTest,
	"MolecularForge.Parser.CifStimmtMitPdbUeberein",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMolCifMatchesPdbTest::RunTest(const FString& Parameters)
{
	// Der schaerfste Test fuer einen zweiten Leser ist der erste Leser. Beide bekommen
	// denselben Ausschnitt in ihrem jeweiligen Format und muessen Atom fuer Atom dasselbe
	// liefern — ein Spaltenversatz oder eine vertauschte Koordinate faellt hier sofort auf,
	// waehrend eine Pruefung gegen selbst gesetzte Erwartungswerte ihn durchlassen koennte.
	UMolecularStructure* FromPdb = NewObject<UMolecularStructure>();
	UMolecularStructure* FromCif = NewObject<UMolecularStructure>();

	const FMolParseResult PdbResult = MolecularForge::ParsePdb(GCrambinFragment, MakeTestOptions(), *FromPdb);
	const FMolParseResult CifResult = MolecularForge::ParseCif(GCrambinCif, MakeTestOptions(), *FromCif);

	TestTrue(TEXT("PDB geladen"), PdbResult.bSuccess);
	TestTrue(TEXT("mmCIF geladen"), CifResult.bSuccess);
	TestEqual(TEXT("mmCIF ohne fehlerhafte Zeilen"), CifResult.NumMalformedLines, 0);

	if (!PdbResult.bSuccess || !CifResult.bSuccess)
	{
		return false;
	}

	TestEqual(TEXT("Gleiche Atomzahl"), FromCif->GetNumAtoms(), FromPdb->GetNumAtoms());
	TestEqual(TEXT("Gleiche Residuenzahl"), FromCif->GetNumResidues(), FromPdb->GetNumResidues());
	TestEqual(TEXT("Gleiche Kettenzahl"), FromCif->GetNumChains(), FromPdb->GetNumChains());
	TestEqual(TEXT("Gleiche Bindungszahl"), FromCif->GetNumBonds(), FromPdb->GetNumBonds());

	if (FromCif->GetNumAtoms() == FromPdb->GetNumAtoms())
	{
		for (int32 i = 0; i < FromPdb->GetNumAtoms(); ++i)
		{
			const FVector A = FromPdb->GetAtomPosition(i);
			const FVector B = FromCif->GetAtomPosition(i);
			if (!A.Equals(B, 0.001))
			{
				AddError(FString::Printf(TEXT("Atom %d weicht ab: PDB %s vs. mmCIF %s"),
					i, *A.ToString(), *B.ToString()));
				break;
			}
			if (FromPdb->AtomElements[i] != FromCif->AtomElements[i])
			{
				AddError(FString::Printf(TEXT("Atom %d: Element %d (PDB) vs. %d (mmCIF)"),
					i, FromPdb->AtomElements[i], FromCif->AtomElements[i]));
				break;
			}
			if (FromPdb->AtomNames[i] != FromCif->AtomNames[i])
			{
				AddError(FString::Printf(TEXT("Atom %d: Name '%s' (PDB) vs. '%s' (mmCIF)"),
					i, *FromPdb->AtomNames[i].ToString(), *FromCif->AtomNames[i].ToString()));
				break;
			}
			if (FromPdb->AtomFlags[i] != FromCif->AtomFlags[i])
			{
				AddError(FString::Printf(TEXT("Atom %d: Flags %d (PDB) vs. %d (mmCIF)"),
					i, FromPdb->AtomFlags[i], FromCif->AtomFlags[i]));
				break;
			}
		}
	}

	TestEqual(TEXT("Gleicher Bezeichner"), FromCif->Meta.Identifier, FromPdb->Meta.Identifier);
	TestEqual(TEXT("Gleiche Sequenz in Kette A"), FromCif->GetChainSequence(0), FromPdb->GetChainSequence(0));
	TestTrue(TEXT("Titel aus dem mehrzeiligen Textfeld gelesen"),
		FromCif->Meta.Title.Contains(TEXT("HYDROPHOBIC PROTEIN")));
	TestTrue(TEXT("Aufloesung aus refine gelesen"),
		FMath::IsNearlyEqual(FromCif->Meta.ResolutionAngstrom, 1.5f, 0.001f));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMolCifFeaturesTest,
	"MolecularForge.Parser.CifBesonderheiten",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMolCifFeaturesTest::RunTest(const FString& Parameters)
{
	// Enthaelt bewusst die Stellen, an denen ein naiver Tokenizer scheitert:
	// Atomnamen mit Apostroph in Anfuehrungszeichen ("O5'"), eine zweite Kette mit
	// Nukleotiden, Sekundaerstruktur aus struct_conf und eine Zeile aus Modell 2.
	const TCHAR* Snippet =
		TEXT("data_TEST\n")
		TEXT("#\n")
		TEXT("loop_\n")
		TEXT("_struct_conf.conf_type_id\n")
		TEXT("_struct_conf.beg_auth_asym_id\n")
		TEXT("_struct_conf.beg_auth_seq_id\n")
		TEXT("_struct_conf.end_auth_asym_id\n")
		TEXT("_struct_conf.end_auth_seq_id\n")
		TEXT("HELX_P A 1 A 2\n")
		TEXT("STRN   A 3 A 3\n")
		TEXT("#\n")
		TEXT("loop_\n")
		TEXT("_atom_site.group_PDB\n")
		TEXT("_atom_site.type_symbol\n")
		TEXT("_atom_site.label_atom_id\n")
		TEXT("_atom_site.label_alt_id\n")
		TEXT("_atom_site.label_comp_id\n")
		TEXT("_atom_site.auth_asym_id\n")
		TEXT("_atom_site.auth_seq_id\n")
		TEXT("_atom_site.Cartn_x\n")
		TEXT("_atom_site.Cartn_y\n")
		TEXT("_atom_site.Cartn_z\n")
		TEXT("_atom_site.occupancy\n")
		TEXT("_atom_site.B_iso_or_equiv\n")
		TEXT("_atom_site.pdbx_PDB_model_num\n")
		TEXT("ATOM N N     . ALA A 1  0.000 0.000 0.000 1.00 10.00 1\n")
		TEXT("ATOM C CA    . ALA A 1  1.458 0.000 0.000 1.00 11.00 1\n")
		TEXT("ATOM N N     . ALA A 2  2.000 1.400 0.000 1.00 12.00 1\n")
		TEXT("ATOM C CA    . ALA A 2  3.400 1.700 0.000 1.00 13.00 1\n")
		TEXT("ATOM N N     . ALA A 3  4.000 3.000 0.000 1.00 14.00 1\n")
		TEXT("ATOM P P     . DA  C 1 10.000 0.000 0.000 1.00 15.00 1\n")
		TEXT("ATOM O \"O5'\" . DA  C 1 11.500 0.000 0.000 1.00 16.00 1\n")
		TEXT("ATOM C \"C1'\" . DA  C 1 13.000 0.000 0.000 1.00 17.00 1\n")
		TEXT("ATOM N N     . ALA A 1 90.000 90.000 90.000 1.00 99.00 2\n")
		TEXT("#\n");

	UMolecularStructure* Structure = NewObject<UMolecularStructure>();
	const FMolParseResult Result = MolecularForge::ParseCif(Snippet, MakeTestOptions(), *Structure);

	TestTrue(TEXT("Laden erfolgreich"), Result.bSuccess);
	if (!Result.bSuccess)
	{
		return false;
	}

	// Die Zeile aus Modell 2 darf nicht in der Struktur landen, aber gezaehlt werden.
	TestEqual(TEXT("Nur Modell 1 uebernommen"), Structure->GetNumAtoms(), 8);
	TestEqual(TEXT("Zwei Modelle erkannt"), Structure->Meta.NumModelsInFile, 2);

	TestEqual(TEXT("Zwei Ketten"), Structure->GetNumChains(), 2);
	TestEqual(TEXT("Kette A ist Protein"), Structure->Chains[0].Kind, EMolChainKind::Protein);
	TestEqual(TEXT("Kette C ist DNA"), Structure->Chains[1].Kind, EMolChainKind::Dna);
	TestEqual(TEXT("Ketten-ID bleibt erhalten"), Structure->Chains[1].Id, FName("C"));

	// Der Apostroph im Atomnamen darf den Wert nicht abschneiden.
	TestEqual(TEXT("Atomname O5' vollstaendig"), Structure->AtomNames[6], FName("O5'"));
	TestEqual(TEXT("Atomname C1' vollstaendig"), Structure->AtomNames[7], FName("C1'"));
	TestTrue(TEXT("C1' ist Ankeratom"), (Structure->AtomFlags[7] & MolAtom_Anchor) != 0);
	TestTrue(TEXT("O5' gehoert zum Rueckgrat"), (Structure->AtomFlags[6] & MolAtom_Backbone) != 0);

	// Sekundaerstruktur aus struct_conf.
	TestEqual(TEXT("Residuum 1 ist Helix"),
		Structure->Residues[0].SecondaryStructure, EMolSecondaryStructure::Helix);
	TestEqual(TEXT("Residuum 2 ist Helix"),
		Structure->Residues[1].SecondaryStructure, EMolSecondaryStructure::Helix);
	TestEqual(TEXT("Residuum 3 ist Faltblatt"),
		Structure->Residues[2].SecondaryStructure, EMolSecondaryStructure::Sheet);
	TestEqual(TEXT("Nukleotid bleibt Coil"),
		Structure->Residues[3].SecondaryStructure, EMolSecondaryStructure::Coil);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMolFormatDetectionTest,
	"MolecularForge.Parser.Formaterkennung",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMolFormatDetectionTest::RunTest(const FString& Parameters)
{
	using namespace MolecularForge;

	TestEqual(TEXT("PDB-Text erkannt"), DetectFormatFromContent(GCrambinFragment), EMolFileFormat::Pdb);
	TestEqual(TEXT("mmCIF-Text erkannt"), DetectFormatFromContent(GCrambinCif), EMolFileFormat::MmCif);
	TestEqual(TEXT("Wirrer Text bleibt unbekannt"),
		DetectFormatFromContent(TEXT("Das hier ist keine Strukturdatei.")), EMolFileFormat::Unknown);

	// Die Endung schlaegt den Inhalt, damit eine korrekt benannte Datei auch dann
	// geladen wird, wenn ihr Kopf ungewoehnlich aussieht.
	TestEqual(TEXT("Endung .cif gewinnt"), DetectFormat(TEXT("x.cif"), TEXT("")), EMolFileFormat::MmCif);
	TestEqual(TEXT("Endung .pdb gewinnt"), DetectFormat(TEXT("x.pdb"), TEXT("")), EMolFileFormat::Pdb);
	// Ohne brauchbare Endung entscheidet der Inhalt.
	TestEqual(TEXT("Endung .txt faellt auf den Inhalt zurueck"),
		DetectFormat(TEXT("x.txt"), GCrambinCif), EMolFileFormat::MmCif);

	// Und der gemeinsame Einstieg waehlt tatsaechlich den richtigen Leser.
	{
		UMolecularStructure* Structure = NewObject<UMolecularStructure>();
		const FMolParseResult Result = ParseStructureText(GCrambinCif, MakeTestOptions(), *Structure);
		TestTrue(TEXT("mmCIF ueber den gemeinsamen Einstieg"), Result.bSuccess);
		TestEqual(TEXT("Atomzahl stimmt"), Structure->GetNumAtoms(), 11);
	}
	{
		UMolecularStructure* Structure = NewObject<UMolecularStructure>();
		const FMolParseResult Result = ParseStructureText(GCrambinFragment, MakeTestOptions(), *Structure);
		TestTrue(TEXT("PDB ueber den gemeinsamen Einstieg"), Result.bSuccess);
		TestEqual(TEXT("Atomzahl stimmt"), Structure->GetNumAtoms(), 11);
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
