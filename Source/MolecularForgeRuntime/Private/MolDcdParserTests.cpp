// Copyright 2026 Simulated Flow All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "MolDcdParser.h"
#include "MolecularTrajectory.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/**
	 * Setzt eine DCD-Datei Byte fuer Byte nach der Formatbeschreibung zusammen.
	 *
	 * Bewusst kein Aufruf eines eigenen Schreibers: waeren Schreiber und Leser aus
	 * derselben Annahme gebaut, steckte ein Missverstaendnis des Formats in beiden und
	 * der Test saehe es nie. Hier stehen stattdessen die Blockgroessen und Reihenfolgen
	 * ausgeschrieben — das ist die abgetippte Spezifikation, gegen die der Leser antritt.
	 */
	struct FDcdBuilder
	{
		TArray<uint8> Bytes;
		bool bBigEndian = false;

		void Raw(const void* Source, int32 Size)
		{
			const uint8* Start = static_cast<const uint8*>(Source);
			Bytes.Append(Start, Size);
		}

		void RawSwapped(const void* Source, int32 Size)
		{
			const uint8* Start = static_cast<const uint8*>(Source);
			if (bBigEndian)
			{
				for (int32 i = Size - 1; i >= 0; --i)
				{
					Bytes.Add(Start[i]);
				}
			}
			else
			{
				Bytes.Append(Start, Size);
			}
		}

		void Int32(int32 Value) { RawSwapped(&Value, sizeof(Value)); }
		void Float(float Value) { RawSwapped(&Value, sizeof(Value)); }
		void Double(double Value) { RawSwapped(&Value, sizeof(Value)); }

		/** Die Gleitkommazahl des Zeitschritts steht im Kopf an einer Ganzzahlstelle. */
		void FloatAsInt32Field(float Value)
		{
			int32 Bits = 0;
			FMemory::Memcpy(&Bits, &Value, sizeof(Bits));
			Int32(Bits);
		}
	};

	struct FDcdTestSpec
	{
		int32 NumAtoms = 3;
		int32 NumFrames = 2;
		int32 NumFixedAtoms = 0;
		bool bUnitCell = false;
		bool bBigEndian = false;
		float TimeStep = 0.002f;
		const char* Magic = "CORD";

		/** Bricht die Datei mitten im letzten Bild ab. */
		bool bTruncateLastFrame = false;
	};

	/** Position von Atom `Atom` in Bild `Frame` — vorhersagbar, damit der Test rechnen kann. */
	FVector3f ExpectedPosition(int32 Frame, int32 Atom)
	{
		return FVector3f(
			static_cast<float>(Atom) + 100.f * Frame,
			static_cast<float>(Atom) * 2.f,
			static_cast<float>(Atom) * 3.f - 10.f * Frame);
	}

	TArray<uint8> BuildDcd(const FDcdTestSpec& Spec)
	{
		FDcdBuilder B;
		B.bBigEndian = Spec.bBigEndian;

		// ---- Kopfblock: 4 Byte Kennung + 20 Ganzzahlen = 84 Byte ----
		B.Int32(84);
		B.Raw(Spec.Magic, 4);

		B.Int32(Spec.NumFrames);	// [0] Zahl der Bilder
		B.Int32(0);					// [1] erster Zeitschritt
		B.Int32(1);					// [2] Abstand der Speicherungen
		B.Int32(Spec.NumFrames);	// [3] Gesamtzahl Schritte
		for (int32 i = 4; i <= 7; ++i)
		{
			B.Int32(0);				// [4..7] ungenutzt
		}
		B.Int32(Spec.NumFixedAtoms);		// [8] festgehaltene Atome
		B.FloatAsInt32Field(Spec.TimeStep);	// [9] Zeitschritt
		B.Int32(Spec.bUnitCell ? 1 : 0);	// [10] Boxangaben vorhanden
		for (int32 i = 11; i <= 18; ++i)
		{
			B.Int32(0);				// [11..18] ungenutzt
		}
		B.Int32(24);				// [19] CHARMM-Version, ungleich null
		B.Int32(84);

		// ---- Titelblock ----
		const int32 NumTitleLines = 1;
		B.Int32(4 + NumTitleLines * 80);
		B.Int32(NumTitleLines);
		{
			char Title[80];
			FMemory::Memset(Title, ' ', sizeof(Title));
			FMemory::Memcpy(Title, "MolecularForge Testtrajektorie", 30);
			B.Raw(Title, sizeof(Title));
		}
		B.Int32(4 + NumTitleLines * 80);

		// ---- Atomzahl ----
		B.Int32(4);
		B.Int32(Spec.NumAtoms);
		B.Int32(4);

		// ---- Bilder ----
		const int32 CoordBlock = Spec.NumAtoms * 4;

		for (int32 Frame = 0; Frame < Spec.NumFrames; ++Frame)
		{
			const bool bLast = (Frame == Spec.NumFrames - 1);

			if (Spec.bUnitCell)
			{
				B.Int32(48);
				// Reihenfolge im Format: A, gamma, B, beta, alpha, C.
				B.Double(60.0 + Frame);		// A
				B.Double(90.0);				// gamma
				B.Double(70.0 + Frame);		// B
				B.Double(90.0);				// beta
				B.Double(90.0);				// alpha
				B.Double(80.0 + Frame);		// C
				B.Int32(48);
			}

			for (int32 Axis = 0; Axis < 3; ++Axis)
			{
				// Abbruch mitten im letzten Bild: nach der Laengenmarke ist Schluss.
				if (Spec.bTruncateLastFrame && bLast && Axis == 1)
				{
					B.Int32(CoordBlock);
					return B.Bytes;
				}

				B.Int32(CoordBlock);
				for (int32 Atom = 0; Atom < Spec.NumAtoms; ++Atom)
				{
					B.Float(ExpectedPosition(Frame, Atom)[Axis]);
				}
				B.Int32(CoordBlock);
			}
		}

		return B.Bytes;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMolDcdBasicTest,
	"MolecularForge.Trajektorie.DcdGrundlagen",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMolDcdBasicTest::RunTest(const FString& Parameters)
{
	FDcdTestSpec Spec;
	Spec.NumAtoms = 4;
	Spec.NumFrames = 3;

	const TArray<uint8> Bytes = BuildDcd(Spec);

	UMolecularTrajectory* Trajectory = NewObject<UMolecularTrajectory>();
	const FMolTrajectoryResult Result =
		MolecularForge::ParseDcd(Bytes, FMolTrajectoryLoadOptions(), *Trajectory);

	TestTrue(FString::Printf(TEXT("Laden erfolgreich: %s"), *Result.Error), Result.bSuccess);
	TestEqual(TEXT("Atomzahl"), Trajectory->NumAtoms, 4);
	TestEqual(TEXT("Bilderzahl"), Trajectory->GetNumFrames(), 3);
	TestEqual(TEXT("Kopf nennt dieselbe Bilderzahl"), Result.NumFramesInFile, 3);
	TestTrue(TEXT("Zeitschritt gelesen"),
		FMath::IsNearlyEqual(Trajectory->TimeStepPicoseconds, 0.002f, 0.0001f));

	// Jede einzelne Koordinate muss stimmen. Ein vertauschtes X und Z faellt hier auf,
	// waehrend eine blosse Pruefung der Bilderzahl es durchgehen liesse.
	for (int32 Frame = 0; Frame < 3; ++Frame)
	{
		for (int32 Atom = 0; Atom < 4; ++Atom)
		{
			const FVector3f Expected = ExpectedPosition(Frame, Atom);
			const FVector Actual = Trajectory->GetAtomPosition(Frame, Atom);

			if (!FVector(Expected).Equals(Actual, 0.001))
			{
				AddError(FString::Printf(
					TEXT("Bild %d, Atom %d: erwartet %s, gelesen %s"),
					Frame, Atom, *FVector(Expected).ToString(), *Actual.ToString()));
				return false;
			}
		}
	}

	// Zusammenhaengender Blick auf ein Bild.
	const TArrayView<const FVector3f> Frame1 = Trajectory->GetFrame(1);
	TestEqual(TEXT("Ein Bild umfasst alle Atome"), Frame1.Num(), 4);
	TestTrue(TEXT("Ungueltiger Bildindex ergibt eine leere Sicht"),
		Trajectory->GetFrame(99).IsEmpty());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMolDcdEndianTest,
	"MolecularForge.Trajektorie.BytereihenfolgeWirdErkannt",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMolDcdEndianTest::RunTest(const FString& Parameters)
{
	// DCD-Dateien von Grossrechnern kommen in umgekehrter Bytereihenfolge. Beide Varianten
	// derselben Daten muessen dasselbe Ergebnis liefern — auch der Zeitschritt, der als
	// Gleitkommazahl an einer Ganzzahlstelle steht und deshalb leicht doppelt getauscht wird.
	FDcdTestSpec Little;
	Little.NumAtoms = 3;
	Little.NumFrames = 2;

	FDcdTestSpec Big = Little;
	Big.bBigEndian = true;

	UMolecularTrajectory* FromLittle = NewObject<UMolecularTrajectory>();
	UMolecularTrajectory* FromBig = NewObject<UMolecularTrajectory>();

	const FMolTrajectoryResult ResultLittle =
		MolecularForge::ParseDcd(BuildDcd(Little), FMolTrajectoryLoadOptions(), *FromLittle);
	const FMolTrajectoryResult ResultBig =
		MolecularForge::ParseDcd(BuildDcd(Big), FMolTrajectoryLoadOptions(), *FromBig);

	TestTrue(TEXT("Kleine Bytereihenfolge geladen"), ResultLittle.bSuccess);
	TestTrue(FString::Printf(TEXT("Grosse Bytereihenfolge geladen: %s"), *ResultBig.Error),
		ResultBig.bSuccess);

	TestEqual(TEXT("Gleiche Atomzahl"), FromBig->NumAtoms, FromLittle->NumAtoms);
	TestEqual(TEXT("Gleiche Bilderzahl"), FromBig->GetNumFrames(), FromLittle->GetNumFrames());

	TestTrue(FString::Printf(TEXT("Gleicher Zeitschritt (%.6f gegen %.6f)"),
		FromBig->TimeStepPicoseconds, FromLittle->TimeStepPicoseconds),
		FMath::IsNearlyEqual(FromBig->TimeStepPicoseconds, FromLittle->TimeStepPicoseconds, 0.00001f));

	for (int32 i = 0; i < FromLittle->Positions.Num(); ++i)
	{
		if (!FromLittle->Positions[i].Equals(FromBig->Positions[i], 0.001f))
		{
			AddError(FString::Printf(TEXT("Position %d weicht ab: %s gegen %s"), i,
				*FromLittle->Positions[i].ToString(), *FromBig->Positions[i].ToString()));
			break;
		}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMolDcdUnitCellAndLimitsTest,
	"MolecularForge.Trajektorie.BoxangabenUndGrenzen",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMolDcdUnitCellAndLimitsTest::RunTest(const FString& Parameters)
{
	FDcdTestSpec Spec;
	Spec.NumAtoms = 3;
	Spec.NumFrames = 6;
	Spec.bUnitCell = true;

	const TArray<uint8> Bytes = BuildDcd(Spec);

	// Boxangaben: aus der Reihenfolge A, gamma, B, beta, alpha, C duerfen nur die
	// Kantenlaengen kommen. Wer die Winkel erwischt, bekommt ueberall 90.
	{
		UMolecularTrajectory* Trajectory = NewObject<UMolecularTrajectory>();
		const FMolTrajectoryResult Result =
			MolecularForge::ParseDcd(Bytes, FMolTrajectoryLoadOptions(), *Trajectory);

		TestTrue(FString::Printf(TEXT("Mit Boxangaben geladen: %s"), *Result.Error), Result.bSuccess);
		TestEqual(TEXT("Alle Bilder"), Trajectory->GetNumFrames(), 6);
		TestEqual(TEXT("Je Bild eine Boxangabe"), Trajectory->UnitCellSizes.Num(), 6);

		TestTrue(TEXT("Kantenlaengen des ersten Bildes"),
			Trajectory->UnitCellSizes[0].Equals(FVector3f(60.f, 70.f, 80.f), 0.01f));
		TestTrue(TEXT("Kantenlaengen wachsen mit dem Bildindex"),
			Trajectory->UnitCellSizes[2].Equals(FVector3f(62.f, 72.f, 82.f), 0.01f));
	}

	// Obergrenze.
	{
		FMolTrajectoryLoadOptions Options;
		Options.MaxFrames = 2;

		UMolecularTrajectory* Trajectory = NewObject<UMolecularTrajectory>();
		MolecularForge::ParseDcd(Bytes, Options, *Trajectory);

		TestEqual(TEXT("Auf zwei Bilder begrenzt"), Trajectory->GetNumFrames(), 2);
		TestTrue(TEXT("Das erste Bild ist das erste der Datei"),
			FVector(Trajectory->GetAtomPosition(0, 1)).Equals(FVector(ExpectedPosition(0, 1)), 0.001));
	}

	// Jedes zweite Bild.
	{
		FMolTrajectoryLoadOptions Options;
		Options.FrameStride = 2;

		UMolecularTrajectory* Trajectory = NewObject<UMolecularTrajectory>();
		MolecularForge::ParseDcd(Bytes, Options, *Trajectory);

		TestEqual(TEXT("Jedes zweite von sechs Bildern"), Trajectory->GetNumFrames(), 3);
		// Das zweite uebernommene Bild muss Bild 2 der Datei sein, nicht Bild 1.
		TestTrue(TEXT("Der Schritt greift wirklich"),
			FVector(Trajectory->GetAtomPosition(1, 0)).Equals(FVector(ExpectedPosition(2, 0)), 0.001));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMolDcdRejectionTest,
	"MolecularForge.Trajektorie.KaputteDateienWerdenAbgewiesen",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMolDcdRejectionTest::RunTest(const FString& Parameters)
{
	using namespace MolecularForge;

	// Keine DCD-Datei.
	{
		const TArray<uint8> Garbage = { 'H', 'a', 'l', 'l', 'o', ' ', 'W', 'e', 'l', 't' };
		UMolecularTrajectory* Trajectory = NewObject<UMolecularTrajectory>();
		const FMolTrajectoryResult Result = ParseDcd(Garbage, FMolTrajectoryLoadOptions(), *Trajectory);

		TestFalse(TEXT("Fremder Inhalt wird abgewiesen"), Result.bSuccess);
		TestFalse(TEXT("Mit Begruendung"), Result.Error.IsEmpty());
	}

	// Leere Datei.
	{
		UMolecularTrajectory* Trajectory = NewObject<UMolecularTrajectory>();
		const FMolTrajectoryResult Result =
			ParseDcd(TArrayView<const uint8>(), FMolTrajectoryLoadOptions(), *Trajectory);
		TestFalse(TEXT("Leere Datei wird abgewiesen"), Result.bSuccess);
	}

	// Falsche Kennung im Kopf.
	{
		FDcdTestSpec Spec;
		Spec.Magic = "VELD";	// Geschwindigkeiten statt Koordinaten

		UMolecularTrajectory* Trajectory = NewObject<UMolecularTrajectory>();
		const FMolTrajectoryResult Result = ParseDcd(BuildDcd(Spec), FMolTrajectoryLoadOptions(), *Trajectory);

		TestFalse(TEXT("Fremde Kennung wird abgewiesen"), Result.bSuccess);
		TestTrue(TEXT("Der Hinweis nennt die Kennung"), Result.Error.Contains(TEXT("CORD")));
	}

	// Festgehaltene Atome: lieber ablehnen als halb richtig lesen.
	{
		FDcdTestSpec Spec;
		Spec.NumFixedAtoms = 2;

		UMolecularTrajectory* Trajectory = NewObject<UMolecularTrajectory>();
		const FMolTrajectoryResult Result = ParseDcd(BuildDcd(Spec), FMolTrajectoryLoadOptions(), *Trajectory);

		TestFalse(TEXT("Datei mit festgehaltenen Atomen wird abgewiesen"), Result.bSuccess);
		TestTrue(TEXT("Der Hinweis erklaert warum"), Result.Error.Contains(TEXT("beweglichen")));
	}

	// Abbruch mitten im letzten Bild: die vollstaendigen Bilder bleiben brauchbar.
	{
		FDcdTestSpec Spec;
		Spec.NumAtoms = 3;
		Spec.NumFrames = 4;
		Spec.bTruncateLastFrame = true;

		UMolecularTrajectory* Trajectory = NewObject<UMolecularTrajectory>();
		const FMolTrajectoryResult Result = ParseDcd(BuildDcd(Spec), FMolTrajectoryLoadOptions(), *Trajectory);

		TestTrue(TEXT("Die heilen Bilder werden trotzdem geladen"), Result.bSuccess);
		TestEqual(TEXT("Das abgebrochene Bild faellt weg"), Trajectory->GetNumFrames(), 3);

		// Und die Positionsliste darf keine halben Bilder enthalten.
		TestEqual(TEXT("Keine angefangenen Bilder im Array"),
			Trajectory->Positions.Num(), 3 * Spec.NumAtoms);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMolTrajectorySamplingTest,
	"MolecularForge.Trajektorie.ZwischenbilderWerdenGemischt",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMolTrajectorySamplingTest::RunTest(const FString& Parameters)
{
	FDcdTestSpec Spec;
	Spec.NumAtoms = 3;
	Spec.NumFrames = 3;

	UMolecularTrajectory* Trajectory = NewObject<UMolecularTrajectory>();
	MolecularForge::ParseDcd(BuildDcd(Spec), FMolTrajectoryLoadOptions(), *Trajectory);

	TArray<FVector3f> Sampled;

	// Genau auf einem Bild.
	Trajectory->SampleInto(1.f, Sampled);
	TestEqual(TEXT("Ein Bild umfasst alle Atome"), Sampled.Num(), 3);
	TestTrue(TEXT("Auf einem Bild wird nichts gemischt"),
		Sampled[1].Equals(ExpectedPosition(1, 1), 0.001f));

	// Genau dazwischen.
	Trajectory->SampleInto(0.5f, Sampled);
	const FVector3f Middle = (ExpectedPosition(0, 2) + ExpectedPosition(1, 2)) * 0.5f;
	TestTrue(FString::Printf(TEXT("Mitte zwischen zwei Bildern (erwartet %s, erhalten %s)"),
		*Middle.ToString(), *Sampled[2].ToString()), Sampled[2].Equals(Middle, 0.001f));

	// Ausserhalb des Bereichs wird begrenzt statt daneben zu greifen.
	Trajectory->SampleInto(-5.f, Sampled);
	TestTrue(TEXT("Vor dem Anfang bleibt es beim ersten Bild"),
		Sampled[0].Equals(ExpectedPosition(0, 0), 0.001f));

	Trajectory->SampleInto(99.f, Sampled);
	TestTrue(TEXT("Nach dem Ende bleibt es beim letzten Bild"),
		Sampled[0].Equals(ExpectedPosition(2, 0), 0.001f));

	// Leere Trajektorie darf nicht stolpern.
	{
		UMolecularTrajectory* Empty = NewObject<UMolecularTrajectory>();
		TArray<FVector3f> Nothing;
		Empty->SampleInto(0.5f, Nothing);
		TestEqual(TEXT("Leere Trajektorie ergibt nichts"), Nothing.Num(), 0);
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
