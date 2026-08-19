// Copyright Simulated Flow. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "MolMesoscaleMath.h"

#if WITH_DEV_AUTOMATION_TESTS

// Geprueft wird die Physik hinter den Prozessoren, nicht Mass selbst. Ein Prozessor
// braucht eine laufende Welt mit Entity-Verwaltung; die Rechnung darin ist dagegen
// gewoehnlicher Code und laesst sich einzeln pruefen. Genau deshalb steht sie als freie
// Funktion daneben und nicht im Prozessor.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMolBrownianMotionTest,
	"MolecularForge.Mesoskala.DiffusionFolgtDerEinsteinRelation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMolBrownianMotionTest::RunTest(const FString& Parameters)
{
	using namespace MolecularForge;

	// D und dt so gewaehlt, dass 2*D*dt genau 1 ergibt — die erwartete Standardabweichung
	// je Achse ist dann 1, und alle Vergleichszahlen bleiben lesbar.
	constexpr float DiffusionCoefficient = 0.5f;
	constexpr float DeltaSeconds = 1.f;
	constexpr int32 NumSamples = 20000;

	FRandomStream Stream(12345);

	double SumPerAxis[3] = { 0.0, 0.0, 0.0 };
	double SumSquaresPerAxis[3] = { 0.0, 0.0, 0.0 };
	int32 WithinOneSigma = 0;

	for (int32 i = 0; i < NumSamples; ++i)
	{
		const FVector3f Step = ComputeBrownianStep(Stream, DiffusionCoefficient, DeltaSeconds);

		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			SumPerAxis[Axis] += Step[Axis];
			SumSquaresPerAxis[Axis] += static_cast<double>(Step[Axis]) * Step[Axis];
		}

		if (FMath::Abs(Step.X) <= 1.f)
		{
			++WithinOneSigma;
		}
	}

	// Der Erwartungswert ist null: Diffusion hat keine Vorzugsrichtung. Ein Fehler im
	// Vorzeichen oder ein vergessener Mittelwertabzug faellt hier sofort auf.
	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		const double Mean = SumPerAxis[Axis] / NumSamples;
		if (FMath::Abs(Mean) > 0.05)
		{
			AddError(FString::Printf(TEXT("Achse %d hat einen Mittelwert von %.4f statt null."), Axis, Mean));
			return false;
		}
	}

	// Und die mittlere quadratische Auslenkung ist 2*D*dt — die Einstein-Relation.
	// Waere die Standardabweichung etwa als D*dt angesetzt, laege der Wert bei 0,25
	// statt bei 1, und die Diffusion waere um den Faktor zwei zu langsam.
	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		const double Variance = SumSquaresPerAxis[Axis] / NumSamples;
		const double Expected = 2.0 * DiffusionCoefficient * DeltaSeconds;

		if (FMath::Abs(Variance - Expected) > 0.05)
		{
			AddError(FString::Printf(
				TEXT("Achse %d: mittlere quadratische Auslenkung ist %.4f statt %.4f."),
				Axis, Variance, Expected));
			return false;
		}
	}

	// Rund 68 Prozent der Werte muessen innerhalb einer Standardabweichung liegen. Das
	// unterscheidet eine echte Normalverteilung von einer Summe gleichverteilter Zahlen,
	// bei der es etwa 58 Prozent waeren — und es sind gerade die seltenen weiten Spruenge,
	// die den Stoff im Raum verteilen.
	const double Fraction = static_cast<double>(WithinOneSigma) / NumSamples;
	TestTrue(FString::Printf(TEXT("Anteil innerhalb einer Standardabweichung ist %.3f, erwartet rund 0.68"),
		Fraction), Fraction > 0.63 && Fraction < 0.73);

	// Ohne Diffusion oder ohne Zeit bewegt sich nichts.
	{
		FRandomStream Unused(1);
		TestTrue(TEXT("Ohne Diffusionskoeffizient kein Schritt"),
			ComputeBrownianStep(Unused, 0.f, 1.f).IsNearlyZero());
		TestTrue(TEXT("Ohne Zeitschritt kein Schritt"),
			ComputeBrownianStep(Unused, 1.f, 0.f).IsNearlyZero());
	}

	// Gleicher Startwert, gleiche Folge. Das ist die Voraussetzung dafuer, dass ein
	// aufgezeichnetes Video sich wiederholen laesst.
	{
		FRandomStream A(777);
		FRandomStream B(777);

		for (int32 i = 0; i < 10; ++i)
		{
			const FVector3f StepA = ComputeBrownianStep(A, 1.f, 0.1f);
			const FVector3f StepB = ComputeBrownianStep(B, 1.f, 0.1f);

			if (!StepA.Equals(StepB, 0.0001f))
			{
				AddError(TEXT("Gleicher Startwert liefert verschiedene Folgen."));
				return false;
			}
		}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMolBoundaryTest,
	"MolecularForge.Mesoskala.GrenzenHaltenDicht",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMolBoundaryTest::RunTest(const FString& Parameters)
{
	using namespace MolecularForge;

	const FBox3f Bounds(FVector3f(-10.f, -10.f, -10.f), FVector3f(10.f, 10.f, 10.f));

	// Innerhalb wird nichts angefasst.
	{
		FVector3f Position(1.f, 2.f, 3.f);
		FVector3f Velocity(5.f, 0.f, 0.f);

		TestFalse(TEXT("Innerhalb bleibt alles unveraendert"),
			ConfineToBounds(Bounds, EMolBoundaryMode::Reflect, Position, Velocity));
		TestTrue(TEXT("Die Position ist unveraendert"), Position.Equals(FVector3f(1.f, 2.f, 3.f), 0.001f));
	}

	// Reflektieren: zurueck in den Raum, und die Geschwindigkeit dreht um. Ohne das
	// Umdrehen klebte das Molekuel an der Wand, weil es im naechsten Bild sofort
	// wieder hinausdrueckte.
	{
		FVector3f Position(13.f, 0.f, 0.f);
		FVector3f Velocity(4.f, 0.f, 0.f);

		TestTrue(TEXT("Ausserhalb wird zurechtgerueckt"),
			ConfineToBounds(Bounds, EMolBoundaryMode::Reflect, Position, Velocity));
		TestTrue(FString::Printf(TEXT("An der Wand gespiegelt (x = %.2f, erwartet 7)"), Position.X),
			FMath::IsNearlyEqual(Position.X, 7.f, 0.001f));
		TestTrue(TEXT("Die Geschwindigkeit zeigt nach innen"), Velocity.X < 0.f);
	}

	// Auch ein weiter Sprung muss im Raum landen. Ein einzelnes Spiegeln reichte dafuer
	// nicht — deshalb die Schleife mit Deckel.
	{
		FVector3f Position(1000.f, -543.f, 77.f);
		FVector3f Velocity(1.f, 1.f, 1.f);

		ConfineToBounds(Bounds, EMolBoundaryMode::Reflect, Position, Velocity);

		TestTrue(FString::Printf(TEXT("Auch ein weiter Sprung landet im Raum (%s)"), *Position.ToString()),
			Bounds.IsInsideOrOn(Position));
	}

	// Umlaufen: auf der Gegenseite wieder herein.
	{
		FVector3f Position(12.f, 0.f, 0.f);
		FVector3f Velocity(1.f, 0.f, 0.f);

		ConfineToBounds(Bounds, EMolBoundaryMode::Wrap, Position, Velocity);
		TestTrue(FString::Printf(TEXT("Umlaufen setzt bei -8 wieder ein (x = %.2f)"), Position.X),
			FMath::IsNearlyEqual(Position.X, -8.f, 0.001f));
		TestTrue(TEXT("Beim Umlaufen bleibt die Richtung erhalten"), Velocity.X > 0.f);
	}

	// Auch mehrere Boxlaengen weit draussen.
	{
		FVector3f Position(-247.f, 0.f, 0.f);
		FVector3f Velocity = FVector3f::ZeroVector;

		ConfineToBounds(Bounds, EMolBoundaryMode::Wrap, Position, Velocity);
		TestTrue(FString::Printf(TEXT("Mehrfaches Umlaufen landet im Raum (x = %.2f)"), Position.X),
			Position.X >= -10.f && Position.X <= 10.f);
	}

	// Ohne Grenze bleibt alles, wie es ist.
	{
		FVector3f Position(1000.f, 0.f, 0.f);
		FVector3f Velocity = FVector3f::ZeroVector;

		TestFalse(TEXT("Ohne Grenze wird nichts veraendert"),
			ConfineToBounds(Bounds, EMolBoundaryMode::None, Position, Velocity));
		TestTrue(TEXT("Die Position bleibt draussen"), FMath::IsNearlyEqual(Position.X, 1000.f, 0.001f));
	}

	// Ein Raum ohne Ausdehnung darf nicht durch null teilen.
	{
		const FBox3f Degenerate(FVector3f::ZeroVector, FVector3f::ZeroVector);
		FVector3f Position(5.f, 0.f, 0.f);
		FVector3f Velocity = FVector3f::ZeroVector;

		TestFalse(TEXT("Entarteter Raum wird uebergangen"),
			ConfineToBounds(Degenerate, EMolBoundaryMode::Wrap, Position, Velocity));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMolDetailLevelTest,
	"MolecularForge.Mesoskala.Detailstufen",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMolDetailLevelTest::RunTest(const FString& Parameters)
{
	using namespace MolecularForge;

	constexpr float Full = 100.f;
	constexpr float Backbone = 500.f;
	constexpr float Blob = 2000.f;

	auto At = [](float Distance) { return Distance * Distance; };

	TestEqual(TEXT("Direkt davor"), ComputeDetailLevel(At(0.f), Full, Backbone, Blob), EMolMesoDetail::Full);
	TestEqual(TEXT("Genau an der ersten Schwelle"),
		ComputeDetailLevel(At(100.f), Full, Backbone, Blob), EMolMesoDetail::Full);
	TestEqual(TEXT("Knapp dahinter"),
		ComputeDetailLevel(At(101.f), Full, Backbone, Blob), EMolMesoDetail::Backbone);
	TestEqual(TEXT("Mittlere Entfernung"),
		ComputeDetailLevel(At(1000.f), Full, Backbone, Blob), EMolMesoDetail::Blob);
	TestEqual(TEXT("Weit weg"),
		ComputeDetailLevel(At(5000.f), Full, Backbone, Blob), EMolMesoDetail::Hidden);

	// Falsch herum angegebene Schwellen duerfen keine Stufe unerreichbar machen —
	// sonst haette eine vertippte Zahl im Editor zur Folge, dass eine Darstellung
	// niemals erscheint, ohne dass jemand den Grund faende.
	{
		const EMolMesoDetail Near = ComputeDetailLevel(At(50.f), 1000.f, 10.f, 5.f);
		TestEqual(TEXT("Verdrehte Schwellen ergeben trotzdem die naechste Stufe"),
			Near, EMolMesoDetail::Full);

		const EMolMesoDetail Far = ComputeDetailLevel(At(5000.f), 1000.f, 10.f, 5.f);
		TestEqual(TEXT("Und in der Ferne die letzte"), Far, EMolMesoDetail::Hidden);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMolBindingProbabilityTest,
	"MolecularForge.Mesoskala.BindungswahrscheinlichkeitBleibtSinnvoll",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMolBindingProbabilityTest::RunTest(const FString& Parameters)
{
	using namespace MolecularForge;

	constexpr float ContactSum = 10.f;
	constexpr float InRange = 81.f;		// Abstand 9, also innerhalb
	constexpr float OutOfRange = 121.f;	// Abstand 11, also ausserhalb

	// Ausserhalb der Reichweite wird nie gebunden, egal wie hoch die Wahrscheinlichkeit ist.
	TestFalse(TEXT("Ausser Reichweite wird nicht gebunden"),
		ShouldBind(OutOfRange, ContactSum, 1000.f, 1.f, 0.f));

	// Innerhalb und mit einem sehr niedrigen Wurf wird gebunden.
	TestTrue(TEXT("In Reichweite und mit niedrigem Wurf wird gebunden"),
		ShouldBind(InRange, ContactSum, 1.f, 1.f, 0.f));

	// Ein Wurf knapp unter eins darf bei massvoller Rate nicht binden.
	TestFalse(TEXT("Hoher Wurf bindet bei massvoller Rate nicht"),
		ShouldBind(InRange, ContactSum, 0.5f, 0.1f, 0.99f));

	// Ohne Rate oder ohne Zeit passiert nichts.
	TestFalse(TEXT("Ohne Rate keine Bindung"), ShouldBind(InRange, ContactSum, 0.f, 1.f, 0.f));
	TestFalse(TEXT("Ohne Zeitschritt keine Bindung"), ShouldBind(InRange, ContactSum, 1.f, 0.f, 0.f));

	// Der entscheidende Punkt: bei grossem Zeitschritt darf die Wahrscheinlichkeit nicht
	// ueber eins laufen. Ein schlichtes Multiplizieren mit dt ergaebe hier 20 — und dann
	// bindet alles beim ersten Bild, statt sich nach und nach zusammenzufinden.
	{
		// Rate 2 je Sekunde, Zeitschritt 10 Sekunden: die Schrittwahrscheinlichkeit
		// liegt bei 1 - exp(-20), also praktisch bei eins, aber eben nicht darueber.
		TestTrue(TEXT("Grosser Zeitschritt bindet fast sicher"),
			ShouldBind(InRange, ContactSum, 2.f, 10.f, 0.999f));

		// Und der Grenzfall: ein Wurf von genau eins darf nie binden, sonst waere die
		// Wahrscheinlichkeit ueberschritten.
		TestFalse(TEXT("Ein Wurf von eins bindet nie"),
			ShouldBind(InRange, ContactSum, 2.f, 10.f, 1.f));
	}

	// Die Rate muss sich auch tatsaechlich auswirken: bei gleichem Wurf bindet die
	// hoehere Rate und die niedrigere nicht.
	{
		constexpr float Roll = 0.4f;
		TestTrue(TEXT("Hohe Rate bindet"), ShouldBind(InRange, ContactSum, 5.f, 0.2f, Roll));
		TestFalse(TEXT("Niedrige Rate bindet nicht"), ShouldBind(InRange, ContactSum, 0.5f, 0.2f, Roll));
	}

	// Das Loesen folgt derselben Formel.
	TestTrue(TEXT("Niedriger Wurf loest"), ShouldUnbind(1.f, 1.f, 0.f));
	TestFalse(TEXT("Hoher Wurf loest nicht"), ShouldUnbind(0.1f, 0.1f, 0.9f));
	TestFalse(TEXT("Ohne Rate loest nichts"), ShouldUnbind(0.f, 1.f, 0.f));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
