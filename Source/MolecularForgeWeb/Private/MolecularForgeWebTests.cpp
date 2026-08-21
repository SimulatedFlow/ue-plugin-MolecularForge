// Copyright 2026 Simulated Flow All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "MolStructureIdentifiers.h"
#include "MolStructureCache.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS

// Hier wird bewusst nichts abgerufen. Ein Test, der an RCSB oder EMBL-EBI haengt, ist
// kein Test des Plugins, sondern einer der Internetverbindung — er wuerde im Flugzeug
// rot und bei einer Serverwartung ohne eigenes Zutun ebenfalls. Geprueft wird alles,
// was ohne Netz entscheidbar ist: Kennungspruefung, Adressbildung, Zwischenspeicher.
// Der eigentliche Abruf ist damit nicht abgedeckt und muss von Hand probiert werden.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMolPdbIdentifierTest,
	"MolecularForge.Web.PdbKennung",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMolPdbIdentifierTest::RunTest(const FString& Parameters)
{
	using namespace MolecularForge;

	TestTrue(TEXT("1CRN"), IsValidPdbIdentifier(TEXT("1CRN")));
	TestTrue(TEXT("4HHB"), IsValidPdbIdentifier(TEXT("4HHB")));
	TestTrue(TEXT("6VXX"), IsValidPdbIdentifier(TEXT("6VXX")));
	TestTrue(TEXT("Kleinschreibung ist erlaubt"), IsValidPdbIdentifier(TEXT("1crn")));
	TestTrue(TEXT("Erweiterte Form"), IsValidPdbIdentifier(TEXT("pdb_00001crn")));

	TestFalse(TEXT("Leer"), IsValidPdbIdentifier(TEXT("")));
	TestFalse(TEXT("Zu kurz"), IsValidPdbIdentifier(TEXT("1CR")));
	TestFalse(TEXT("Zu lang"), IsValidPdbIdentifier(TEXT("1CRNX")));
	TestFalse(TEXT("Beginnt mit Buchstabe"), IsValidPdbIdentifier(TEXT("ACRN")));
	TestFalse(TEXT("Beginnt mit Null"), IsValidPdbIdentifier(TEXT("0CRN")));
	TestFalse(TEXT("Bindestrich"), IsValidPdbIdentifier(TEXT("1C-N")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMolUniProtIdentifierTest,
	"MolecularForge.Web.UniProtKennung",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMolUniProtIdentifierTest::RunTest(const FString& Parameters)
{
	using namespace MolecularForge;

	// Sechsstellige Accessions nach dem ersten Muster.
	TestTrue(TEXT("P69905 (Haemoglobin alpha)"), IsValidUniProtAccession(TEXT("P69905")));
	TestTrue(TEXT("Q8N158"), IsValidUniProtAccession(TEXT("Q8N158")));
	TestTrue(TEXT("O00533"), IsValidUniProtAccession(TEXT("O00533")));
	TestTrue(TEXT("Kleinschreibung ist erlaubt"), IsValidUniProtAccession(TEXT("p69905")));

	// Sechs- und zehnstellige nach dem zweiten Muster.
	TestTrue(TEXT("A2BC19"), IsValidUniProtAccession(TEXT("A2BC19")));
	TestTrue(TEXT("A0A023GPI8"), IsValidUniProtAccession(TEXT("A0A023GPI8")));

	TestFalse(TEXT("Leer"), IsValidUniProtAccession(TEXT("")));
	TestFalse(TEXT("Zu kurz"), IsValidUniProtAccession(TEXT("P6990")));
	TestFalse(TEXT("Acht Zeichen gibt es nicht"), IsValidUniProtAccession(TEXT("A0A023GP")));
	TestFalse(TEXT("Beginnt mit Ziffer"), IsValidUniProtAccession(TEXT("1P6990")));
	// Zehnstellige Accessions beginnen nie mit O, P oder Q — das gibt das Muster nicht her.
	TestFalse(TEXT("P am Anfang einer zehnstelligen"), IsValidUniProtAccession(TEXT("P0A023GPI8")));
	TestFalse(TEXT("Letzte Stelle keine Ziffer"), IsValidUniProtAccession(TEXT("P6990X")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMolRequestUrlTest,
	"MolecularForge.Web.Adressbildung",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMolRequestUrlTest::RunTest(const FString& Parameters)
{
	using namespace MolecularForge;

	TestEqual(TEXT("RCSB liefert mmCIF"),
		BuildRequestUrl(EMolFetchSource::RcsbPdb, TEXT("1crn")),
		FString(TEXT("https://files.rcsb.org/download/1CRN.cif")));

	TestEqual(TEXT("AlphaFold geht ueber die API, nicht direkt auf die Datei"),
		BuildRequestUrl(EMolFetchSource::AlphaFoldDb, TEXT("p69905")),
		FString(TEXT("https://alphafold.ebi.ac.uk/api/prediction/P69905")));

	TestTrue(TEXT("Ungueltige PDB-Kennung ergibt keine Adresse"),
		BuildRequestUrl(EMolFetchSource::RcsbPdb, TEXT("ACRN")).IsEmpty());
	TestTrue(TEXT("Ungueltige Accession ergibt keine Adresse"),
		BuildRequestUrl(EMolFetchSource::AlphaFoldDb, TEXT("nichtgueltig")).IsEmpty());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMolIdentifierInjectionTest,
	"MolecularForge.Web.KennungLaesstSichNichtUnterschieben",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMolIdentifierInjectionTest::RunTest(const FString& Parameters)
{
	using namespace MolecularForge;

	// Die Kennung landet ungefiltert in einer URL und in einem Dateinamen. Deshalb muss
	// die Pruefung eine Positivliste sein und nicht ein nachtraegliches Herausfiltern:
	// alles, was nicht ausdruecklich erlaubt ist, darf gar nicht erst durchkommen.
	const TCHAR* Versuche[] =
	{
		TEXT("../../../etc/passwd"),
		TEXT("..\\..\\Windows\\System32"),
		TEXT("1CRN/../../secret"),
		TEXT("1CRN?redirect=http://example.com"),
		TEXT("1CRN#fragment"),
		TEXT("1CRN%2F..%2F"),
		TEXT("1CRN "),
		TEXT("1 RN"),
		TEXT("1CRN\nHost: example.com"),
		TEXT("http://example.com/evil.cif")
	};

	for (const TCHAR* Versuch : Versuche)
	{
		const FString Eingabe(Versuch);

		// Nachlaufender Leerraum ist harmlos und wird beim Normalisieren entfernt;
		// dieser eine Fall darf also gueltig werden. Alle anderen nicht.
		const bool bDarfGueltigSein = Eingabe.TrimStartAndEnd().Equals(TEXT("1CRN"), ESearchCase::IgnoreCase);

		const FString Normalisiert = NormalizeIdentifier(EMolFetchSource::RcsbPdb, Eingabe);
		const bool bGueltig = IsValidIdentifier(EMolFetchSource::RcsbPdb, Normalisiert);

		if (!bDarfGueltigSein)
		{
			TestFalse(FString::Printf(TEXT("'%s' wird abgewiesen"), Versuch), bGueltig);
			TestTrue(FString::Printf(TEXT("'%s' ergibt keine Adresse"), Versuch),
				BuildRequestUrl(EMolFetchSource::RcsbPdb, Eingabe).IsEmpty());
			TestTrue(FString::Printf(TEXT("'%s' ergibt keinen Cache-Pfad"), Versuch),
				GetCacheFilePath(EMolFetchSource::RcsbPdb, Eingabe).IsEmpty());
		}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMolStructureCacheTest,
	"MolecularForge.Web.Zwischenspeicher",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMolStructureCacheTest::RunTest(const FString& Parameters)
{
	using namespace MolecularForge;

	// Eine Kennung, die formal gueltig ist, damit der Pfad gebaut wird. Der Eintrag
	// wird am Ende wieder entfernt, damit der Test keine Spuren hinterlaesst.
	const FString TestKennung = TEXT("9ZZZ");
	const FString TestInhalt = TEXT("data_9ZZZ\n_entry.id 9ZZZ\n");

	RemoveCachedStructure(EMolFetchSource::RcsbPdb, TestKennung);

	const FString Pfad = GetCacheFilePath(EMolFetchSource::RcsbPdb, TestKennung);
	TestFalse(TEXT("Cache-Pfad wird gebildet"), Pfad.IsEmpty());
	TestTrue(TEXT("Pfad liegt im Cache-Verzeichnis"), Pfad.StartsWith(GetCacheDirectory()));
	TestTrue(TEXT("Pfad enthaelt die Quelle"), Pfad.Contains(TEXT("rcsb")));
	TestFalse(TEXT("Pfad enthaelt keinen Rueckwaertsschritt"), Pfad.Contains(TEXT("..")));

	TestFalse(TEXT("Vor dem Schreiben liegt nichts im Cache"),
		HasCachedStructure(EMolFetchSource::RcsbPdb, TestKennung));

	TestTrue(TEXT("Schreiben klappt"),
		WriteCachedStructure(EMolFetchSource::RcsbPdb, TestKennung, TestInhalt));
	TestTrue(TEXT("Danach ist der Eintrag da"),
		HasCachedStructure(EMolFetchSource::RcsbPdb, TestKennung));

	FString Gelesen;
	TestTrue(TEXT("Lesen klappt"),
		ReadCachedStructure(EMolFetchSource::RcsbPdb, TestKennung, Gelesen));
	TestEqual(TEXT("Inhalt kommt unveraendert zurueck"), Gelesen, TestInhalt);

	// Gross- und Kleinschreibung duerfen nicht zu zwei Eintraegen fuehren.
	TestTrue(TEXT("Kleingeschriebene Kennung findet denselben Eintrag"),
		HasCachedStructure(EMolFetchSource::RcsbPdb, TEXT("9zzz")));

	// Ein leerer Inhalt ist ein abgebrochener Download und gehoert nicht in den Cache.
	TestFalse(TEXT("Leerer Inhalt wird nicht geschrieben"),
		WriteCachedStructure(EMolFetchSource::RcsbPdb, TEXT("9ZZY"), FString()));

	// Dieselbe Kennung bei anderer Quelle ist ein anderer Eintrag.
	TestFalse(TEXT("Quellen teilen sich keinen Eintrag"),
		HasCachedStructure(EMolFetchSource::AlphaFoldDb, TEXT("P69905")) &&
		GetCacheFilePath(EMolFetchSource::AlphaFoldDb, TEXT("P69905")) == Pfad);

	TestTrue(TEXT("Entfernen klappt"),
		RemoveCachedStructure(EMolFetchSource::RcsbPdb, TestKennung));
	TestFalse(TEXT("Danach ist der Eintrag weg"),
		HasCachedStructure(EMolFetchSource::RcsbPdb, TestKennung));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
