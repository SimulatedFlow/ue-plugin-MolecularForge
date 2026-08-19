// Copyright Simulated Flow. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "MolecularTrajectoryPlayer.h"
#include "MolecularTrajectory.h"
#include "MolTestChainBuilder.h"
#include "MolecularStructure.h"

#if WITH_DEV_AUTOMATION_TESTS

// Geprueft wird, was der Abspieler mit den Daten macht — nicht das Ticken selbst. Fuer
// die Zeitfortschaltung braeuchte es eine laufende Welt; sie besteht aber nur aus einer
// Addition und dem Umschlag am Ende, waehrend die interessante Arbeit im Anwenden eines
// Bildes steckt, und das laesst sich direkt aufrufen.

namespace
{
	/** Position von Atom `Atom` in Bild `Frame` — vorhersagbar, damit der Test rechnen kann. */
	FVector3f FramePosition(int32 Frame, int32 Atom)
	{
		return FVector3f(
			static_cast<float>(Atom) + 10.f * Frame,
			static_cast<float>(Atom) * 0.5f,
			-static_cast<float>(Frame));
	}

	UMolecularTrajectory* BuildTestTrajectory(int32 NumAtoms, int32 NumFrames)
	{
		UMolecularTrajectory* Trajectory = NewObject<UMolecularTrajectory>();
		Trajectory->NumAtoms = NumAtoms;
		Trajectory->TimeStepPicoseconds = 0.01f;
		Trajectory->Positions.Reserve(NumAtoms * NumFrames);

		for (int32 Frame = 0; Frame < NumFrames; ++Frame)
		{
			for (int32 Atom = 0; Atom < NumAtoms; ++Atom)
			{
				Trajectory->Positions.Add(FramePosition(Frame, Atom));
			}
		}

		return Trajectory;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMolTrajectoryPlayerApplyTest,
	"MolecularForge.Trajektorie.AbspielerSchreibtInDieStruktur",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMolTrajectoryPlayerApplyTest::RunTest(const FString& Parameters)
{
	constexpr int32 NumResidues = 5;
	constexpr int32 NumAtoms = NumResidues * 3;

	UMolecularStructure* Structure = MolTest::BuildChain(MolTest::MakeStraightAnchors(NumResidues), {});
	TestEqual(TEXT("Teststruktur hat die erwartete Atomzahl"), Structure->GetNumAtoms(), NumAtoms);

	UMolecularTrajectory* Trajectory = BuildTestTrajectory(NumAtoms, 4);

	UMolecularTrajectoryPlayer* Player = NewObject<UMolecularTrajectoryPlayer>();
	Player->Structure = Structure;
	Player->Trajectory = Trajectory;

	FString Reason;
	TestTrue(FString::Printf(TEXT("Trajektorie passt zur Struktur: %s"), *Reason),
		Player->IsTrajectoryCompatible(Reason));

	// Genau auf einem Bild.
	Player->SetFrameTime(2.f);
	for (int32 Atom = 0; Atom < NumAtoms; ++Atom)
	{
		if (!Structure->AtomPositions[Atom].Equals(FramePosition(2, Atom), 0.001f))
		{
			AddError(FString::Printf(TEXT("Atom %d steht nach dem Setzen auf Bild 2 falsch: %s"),
				Atom, *Structure->AtomPositions[Atom].ToString()));
			return false;
		}
	}

	// Zwischen zwei Bildern wird gemischt.
	Player->bInterpolate = true;
	Player->SetFrameTime(0.5f);
	const FVector3f Middle = (FramePosition(0, 3) + FramePosition(1, 3)) * 0.5f;
	TestTrue(FString::Printf(TEXT("Zwischenzustand gemischt (erwartet %s, erhalten %s)"),
		*Middle.ToString(), *Structure->AtomPositions[3].ToString()),
		Structure->AtomPositions[3].Equals(Middle, 0.001f));

	// Ohne Interpolation wird auf das naechste Bild gerundet.
	Player->bInterpolate = false;
	Player->SetFrameTime(0.6f);
	TestTrue(TEXT("Ohne Interpolation wird gerundet"),
		Structure->AtomPositions[3].Equals(FramePosition(1, 3), 0.001f));

	// Die Huelle muss mitgewandert sein, sonst greifen Kamerafahrt und Culling daneben.
	Player->bInterpolate = true;
	Player->SetFrameTime(3.f);
	const FBox Bounds = Structure->GetBoundsAngstrom();
	TestTrue(TEXT("Die Huelle folgt den neuen Positionen"),
		Bounds.Max.X > 30.0 && Bounds.Min.Z < -2.0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMolTrajectoryPlayerRestoreTest,
	"MolecularForge.Trajektorie.AusgangszustandKommtZurueck",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMolTrajectoryPlayerRestoreTest::RunTest(const FString& Parameters)
{
	constexpr int32 NumResidues = 4;
	constexpr int32 NumAtoms = NumResidues * 3;

	UMolecularStructure* Structure = MolTest::BuildChain(MolTest::MakeStraightAnchors(NumResidues), {});
	const TArray<FVector3f> Before = Structure->AtomPositions;

	UMolecularTrajectoryPlayer* Player = NewObject<UMolecularTrajectoryPlayer>();
	Player->Structure = Structure;
	Player->Trajectory = BuildTestTrajectory(NumAtoms, 3);

	// Schon das blosse Setzen der Bildposition veraendert die Struktur — und muss
	// deshalb den Ausgangszustand sichern, ohne dass jemand Abspielen gedrueckt hat.
	Player->SetFrameTime(2.f);

	bool bChanged = false;
	for (int32 i = 0; i < NumAtoms; ++i)
	{
		if (!Structure->AtomPositions[i].Equals(Before[i], 0.001f))
		{
			bChanged = true;
			break;
		}
	}
	TestTrue(TEXT("Die Struktur wurde tatsaechlich veraendert"), bChanged);

	Player->RestoreOriginalPositions();

	for (int32 i = 0; i < NumAtoms; ++i)
	{
		if (!Structure->AtomPositions[i].Equals(Before[i], 0.001f))
		{
			AddError(FString::Printf(TEXT("Atom %d kam nicht zurueck: %s statt %s"),
				i, *Structure->AtomPositions[i].ToString(), *Before[i].ToString()));
			return false;
		}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMolTrajectoryPlayerMismatchTest,
	"MolecularForge.Trajektorie.UnpassendeTrajektorieWirdErkannt",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMolTrajectoryPlayerMismatchTest::RunTest(const FString& Parameters)
{
	// Eine Trajektorie enthaelt nur Koordinaten und keine Atomnamen — sie kann sich nicht
	// selbst zuordnen. Passt die Atomzahl nicht, gehoert das gemeldet und nicht stillschweigend
	// halb angewandt: sonst saehe man ein Molekuel, das aus zwei verschiedenen zusammengesetzt ist.
	UMolecularStructure* Structure = MolTest::BuildChain(MolTest::MakeStraightAnchors(5), {});
	const TArray<FVector3f> Before = Structure->AtomPositions;

	UMolecularTrajectoryPlayer* Player = NewObject<UMolecularTrajectoryPlayer>();
	Player->Structure = Structure;
	Player->Trajectory = BuildTestTrajectory(/*NumAtoms=*/7, /*NumFrames=*/3);

	FString Reason;
	TestFalse(TEXT("Unterschiedliche Atomzahl wird abgelehnt"), Player->IsTrajectoryCompatible(Reason));
	TestTrue(TEXT("Der Hinweis nennt beide Zahlen"),
		Reason.Contains(TEXT("7")) && Reason.Contains(TEXT("15")));

	// Und es darf nichts geschrieben worden sein.
	Player->SetFrameTime(1.f);
	for (int32 i = 0; i < Structure->GetNumAtoms(); ++i)
	{
		if (!Structure->AtomPositions[i].Equals(Before[i], 0.001f))
		{
			AddError(TEXT("Eine unpassende Trajektorie hat die Struktur veraendert."));
			return false;
		}
	}

	// Ohne Struktur oder Trajektorie ebenfalls sauber ablehnen statt abzustuerzen.
	{
		UMolecularTrajectoryPlayer* Bare = NewObject<UMolecularTrajectoryPlayer>();
		FString BareReason;
		TestFalse(TEXT("Ohne Struktur nicht abspielbar"), Bare->IsTrajectoryCompatible(BareReason));
		TestFalse(TEXT("Mit Begruendung"), BareReason.IsEmpty());

		Bare->SetFrameTime(1.f);
		Bare->RestoreOriginalPositions();
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
