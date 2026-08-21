// Copyright 2026 Simulated Flow All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "MolStructureFetcher.h"
#include "MolStructureCache.h"
#include "MolecularStructure.h"
#include "MolecularForgeTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * Abruf gegen die echten Server.
 *
 * Bewusst NICHT unter "MolecularForge" eingeordnet, sondern unter "Netzabruf": die
 * regulaere Suite laeuft bei jedem Bau und muss ohne Internet gruen sein. Ein Test, der
 * an RCSB oder EMBL-EBI haengt, wird rot, wenn dort gewartet wird, und gruen, wenn unser
 * Code kaputt ist, aber noch etwas im Zwischenspeicher liegt. Solche Tests gehoeren nicht
 * in den Alltagslauf.
 *
 * Von Hand aufrufen:
 *   UnrealEditor-Cmd.exe <uproject> -ExecCmds="Automation RunTests Netzabruf; Quit" -unattended
 *
 * Geprueft wird das, was sich sonst nirgends pruefen laesst: ob die gebauten Adressen
 * stimmen, ob die AlphaFold-API die erwartete Form hat, und ob die heruntergeladene
 * Datei durch unseren Leser geht.
 */

namespace
{
	/** Zustand eines laufenden Abrufs, den der latente Befehl beobachtet. */
	struct FMolFetchProbe
	{
		bool bFinished = false;
		bool bSuccess = false;
		FString Error;
		FString ResolvedUrl;
		int32 NumAtoms = 0;
		bool bIsPLDDT = false;
		FString Attribution;
		FString Identifier;
		bool bFromCache = false;
		double Deadline = 0.0;
	};

	using FMolFetchProbePtr = TSharedPtr<FMolFetchProbe, ESPMode::ThreadSafe>;

	/**
	 * Ein Platz, in den ein spaeter gestarteter Abruf sein Ergebnis legt.
	 *
	 * Noetig, weil latente Befehle erst laufen, nachdem RunTest zurueckgekehrt ist: der
	 * zweite Abruf darf erst beginnen, wenn der erste fertig ist, und beim Anlegen der
	 * Befehlskette gibt es ihn deshalb noch gar nicht.
	 */
	using FMolProbeSlot = TSharedPtr<FMolFetchProbePtr, ESPMode::ThreadSafe>;

	FMolFetchProbePtr StartFetch(EMolFetchSource Source, const FString& Identifier,
		float TimeoutSeconds, bool bUseCache = false)
	{
		FMolFetchProbePtr Probe = MakeShared<FMolFetchProbe, ESPMode::ThreadSafe>();
		Probe->Deadline = FPlatformTime::Seconds() + TimeoutSeconds;

		FMolFetchOptions Options;
		Options.Source = Source;
		Options.Identifier = Identifier;
		// Voreingestellt ohne Zwischenspeicher: sonst pruefte der zweite Lauf nur noch
		// die Festplatte statt die Verbindung.
		Options.bUseCache = bUseCache;
		Options.TimeoutSeconds = TimeoutSeconds;
		Options.LoadOptions.bDiscardWater = true;

		FOnMolStructureFetched Callback;
		Callback.BindLambda([Probe](UMolecularStructure* Structure, const FMolFetchResult& Result)
		{
			Probe->bFinished = true;
			Probe->bSuccess = Result.bSuccess;
			Probe->Error = Result.Error;
			Probe->ResolvedUrl = Result.ResolvedUrl;
			Probe->bFromCache = Result.bFromCache;

			if (Structure)
			{
				Probe->NumAtoms = Structure->GetNumAtoms();
				Probe->bIsPLDDT = Structure->Meta.bBFactorIsPLDDT;
				Probe->Attribution = Structure->Meta.Attribution;
				Probe->Identifier = Structure->Meta.Identifier;
			}
		});

		MolecularForge::FetchStructure(nullptr, Options, Callback);
		return Probe;
	}
}

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(
	FMolWaitForFetch, FMolFetchProbePtr, Probe);

bool FMolWaitForFetch::Update()
{
	// Fertig oder Zeit abgelaufen. Der latente Befehl wird vom Editor getickt, und
	// dabei laeuft auch die HTTP-Verwaltung mit — genau deshalb ist ein Automationstest
	// hier der richtige Ort und nicht ein Kommandozeilenskript.
	return Probe->bFinished || FPlatformTime::Seconds() > Probe->Deadline;
}

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(
	FMolWaitForSlot, FMolProbeSlot, Slot);

bool FMolWaitForSlot::Update()
{
	const FMolFetchProbePtr& Probe = *Slot;
	if (!Probe.IsValid())
	{
		return true;
	}
	return Probe->bFinished || FPlatformTime::Seconds() > Probe->Deadline;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMolFetchFromRcsbTest,
	"Netzabruf.StrukturVomRcsb",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMolFetchFromRcsbTest::RunTest(const FString& Parameters)
{
	const FMolFetchProbePtr Probe = StartFetch(EMolFetchSource::RcsbPdb, TEXT("1CRN"), 45.f);

	ADD_LATENT_AUTOMATION_COMMAND(FMolWaitForFetch(Probe));

	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, Probe]()
	{
		if (!Probe->bFinished)
		{
			AddError(TEXT("Der Abruf bei RCSB kam nicht zurueck. Kein Netz?"));
			return true;
		}

		if (!Probe->bSuccess)
		{
			AddError(FString::Printf(TEXT("Abruf fehlgeschlagen: %s (Adresse: %s)"),
				*Probe->Error, *Probe->ResolvedUrl));
			return true;
		}

		AddInfo(FString::Printf(TEXT("Geladen von %s"), *Probe->ResolvedUrl));

		// Crambin ohne Wasser hat 327 Atome — derselbe Wert wie bei der Pruefung
		// gegen die mitgelieferte Datei.
		TestEqual(TEXT("Crambin hat 327 Atome"), Probe->NumAtoms, 327);
		TestTrue(TEXT("Attributionstext gesetzt"), Probe->Attribution.Contains(TEXT("RCSB")));
		TestFalse(TEXT("Eine Kristallstruktur traegt keinen pLDDT-Wert"), Probe->bIsPLDDT);

		return true;
	}));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMolFetchFromAlphaFoldTest,
	"Netzabruf.VorhersageVonAlphaFold",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMolFetchFromAlphaFoldTest::RunTest(const FString& Parameters)
{
	// P69905 ist die Alpha-Kette des menschlichen Haemoglobins — 141 Aminosaeuren,
	// also klein genug fuer einen schnellen Abruf und bekannt genug, dass die Zahlen
	// nachschlagbar sind.
	const FMolFetchProbePtr Probe = StartFetch(EMolFetchSource::AlphaFoldDb, TEXT("P69905"), 60.f);

	ADD_LATENT_AUTOMATION_COMMAND(FMolWaitForFetch(Probe));

	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, Probe]()
	{
		if (!Probe->bFinished)
		{
			AddError(TEXT("Der Abruf bei AlphaFold kam nicht zurueck. Kein Netz?"));
			return true;
		}

		if (!Probe->bSuccess)
		{
			AddError(FString::Printf(TEXT("Abruf fehlgeschlagen: %s (Adresse: %s)"),
				*Probe->Error, *Probe->ResolvedUrl));
			return true;
		}

		AddInfo(FString::Printf(TEXT("Geladen von %s"), *Probe->ResolvedUrl));
		AddInfo(FString::Printf(TEXT("%d Atome, Kennung '%s'"), Probe->NumAtoms, *Probe->Identifier));

		// Der zweistufige Abruf ist der eigentliche Pruefpunkt: die API muss eine
		// Datei-Adresse genannt haben, sonst waere hier nichts angekommen.
		TestTrue(TEXT("Die aufgeloeste Adresse zeigt auf eine Datei, nicht auf die API"),
			Probe->ResolvedUrl.Contains(TEXT("/files/")));

		// 141 Aminosaeuren ergeben rund 1100 Schweratome.
		TestTrue(FString::Printf(TEXT("Plausible Atomzahl (%d)"), Probe->NumAtoms),
			Probe->NumAtoms > 900 && Probe->NumAtoms < 1300);

		// Das Entscheidende: AlphaFold legt den Konfidenzwert ins B-Faktor-Feld, und
		// das muss erkannt werden — sonst faerbt die Darstellung auf der falschen Skala.
		TestTrue(TEXT("Als AlphaFold-Vorhersage erkannt"), Probe->bIsPLDDT);
		TestTrue(TEXT("Attributionstext nennt die Lizenz"),
			Probe->Attribution.Contains(TEXT("CC-BY")));

		return true;
	}));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMolFetchCacheTest,
	"Netzabruf.ZweiterAbrufKommtAusDemZwischenspeicher",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMolFetchCacheTest::RunTest(const FString& Parameters)
{
	// Der Zwischenspeicher ist Teil des anstaendigen Umgangs mit den Quellen. Dass er
	// greift, laesst sich nur mit einem echten Abruf zeigen: erst holen, dann noch
	// einmal holen und pruefen, dass die zweite Antwort nicht aus dem Netz kam.
	using namespace MolecularForge;

	RemoveCachedStructure(EMolFetchSource::RcsbPdb, TEXT("1CRN"));

	// Beide Abrufe laufen ueber latente Befehle. Der erste Anlauf hatte den zweiten
	// mit einer eigenen Warteschleife bedient — das ging schief, weil der Rueckweg aus
	// dem Zwischenspeicher ueber einen Hintergrundtask und von dort zurueck auf den
	// Spielthread laeuft. Diese Uebergabe treibt nur der Editor selbst an, nicht eine
	// Schleife im Test.
	const FMolProbeSlot FirstSlot = MakeShared<FMolFetchProbePtr, ESPMode::ThreadSafe>();
	const FMolProbeSlot SecondSlot = MakeShared<FMolFetchProbePtr, ESPMode::ThreadSafe>();

	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([FirstSlot]()
	{
		*FirstSlot = StartFetch(EMolFetchSource::RcsbPdb, TEXT("1CRN"), 45.f, /*bUseCache=*/true);
		return true;
	}));

	ADD_LATENT_AUTOMATION_COMMAND(FMolWaitForSlot(FirstSlot));

	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, FirstSlot]()
	{
		const FMolFetchProbePtr& First = *FirstSlot;

		if (!First.IsValid() || !First->bFinished || !First->bSuccess)
		{
			AddError(FString::Printf(TEXT("Erster Abruf misslungen: %s"),
				First.IsValid() ? *First->Error : TEXT("kein Ergebnis")));
			return true;
		}

		TestFalse(TEXT("Der erste Abruf kam aus dem Netz"), First->bFromCache);
		TestTrue(TEXT("Danach liegt die Datei im Zwischenspeicher"),
			MolecularForge::HasCachedStructure(EMolFetchSource::RcsbPdb, TEXT("1CRN")));

		return true;
	}));

	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([SecondSlot]()
	{
		*SecondSlot = StartFetch(EMolFetchSource::RcsbPdb, TEXT("1CRN"), 20.f, /*bUseCache=*/true);
		return true;
	}));

	ADD_LATENT_AUTOMATION_COMMAND(FMolWaitForSlot(SecondSlot));

	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, SecondSlot]()
	{
		const FMolFetchProbePtr& Second = *SecondSlot;

		if (!Second.IsValid() || !Second->bFinished)
		{
			AddError(TEXT("Der zweite Abruf kam nicht zurueck."));
			return true;
		}

		TestTrue(FString::Printf(TEXT("Zweiter Abruf erfolgreich: %s"), *Second->Error),
			Second->bSuccess);
		TestTrue(TEXT("Und er kam aus dem Zwischenspeicher"), Second->bFromCache);
		TestEqual(TEXT("Mit demselben Inhalt"), Second->NumAtoms, 327);

		return true;
	}));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
