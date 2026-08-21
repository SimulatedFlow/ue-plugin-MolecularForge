// Copyright 2026 Simulated Flow All Rights Reserved.

#include "MassProcessor_MolecularMotion.h"
#include "MolMesoscaleFragments.h"
#include "MolMesoscaleMath.h"

#include "Mass/EntityFragments.h"
#include "MassCommonTypes.h"
#include "MassExecutionContext.h"
#include "MassMovementFragments.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

UMassProcessor_MolecularMotion::UMassProcessor_MolecularMotion()
	// Die Abfrage muss hier an den Prozessor gebunden werden. Ohne das laeuft
	// ConfigureQueries in eine Zusicherung — und zwar erst zur Laufzeit beim
	// Weltstart, nicht beim Uebersetzen.
	: MotionQuery(*this)
{
	bAutoRegisterWithProcessingPhases = true;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::AllNetModes);
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
}

void UMassProcessor_MolecularMotion::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	MotionQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);
	MotionQuery.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadWrite);
	MotionQuery.AddRequirement<FMolMoleculeFragment>(EMassFragmentAccess::ReadWrite);
	MotionQuery.AddConstSharedRequirement<FMolMesoscaleParameters>(EMassFragmentPresence::All);
	MotionQuery.AddTagRequirement<FMolMoleculeTag>(EMassFragmentPresence::All);
}

FVector UMassProcessor_MolecularMotion::GetViewerLocation(const UWorld* World) const
{
	if (!World)
	{
		return FVector::ZeroVector;
	}

	if (const APlayerController* Controller = World->GetFirstPlayerController())
	{
		FVector Location = FVector::ZeroVector;
		FRotator Rotation = FRotator::ZeroRotator;
		Controller->GetPlayerViewPoint(Location, Rotation);
		return Location;
	}

	return FVector::ZeroVector;
}

void UMassProcessor_MolecularMotion::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	const float DeltaSeconds = Context.GetDeltaTimeSeconds();
	if (DeltaSeconds <= 0.f)
	{
		return;
	}

	// Einmal je Bild ermitteln und in die parallele Phase hineingeben. Aus einem
	// Worker-Thread heraus auf die Welt zuzugreifen waere nicht zulaessig.
	const FVector ViewerLocation = GetViewerLocation(EntityManager.GetWorld());

	MotionQuery.ParallelForEachEntityChunk(Context,
		[DeltaSeconds, ViewerLocation](FMassExecutionContext& ChunkContext)
	{
		const FMolMesoscaleParameters& Parameters =
			ChunkContext.GetConstSharedFragment<FMolMesoscaleParameters>();

		const TArrayView<FTransformFragment> Transforms =
			ChunkContext.GetMutableFragmentView<FTransformFragment>();
		const TArrayView<FMassVelocityFragment> Velocities =
			ChunkContext.GetMutableFragmentView<FMassVelocityFragment>();
		const TArrayView<FMolMoleculeFragment> Molecules =
			ChunkContext.GetMutableFragmentView<FMolMoleculeFragment>();

		const FBox3f Bounds = Parameters.GetBounds();
		const FVector3f Viewer(ViewerLocation);

		for (int32 Index = 0; Index < ChunkContext.GetNumEntities(); ++Index)
		{
			FMolMoleculeFragment& Molecule = Molecules[Index];

			// Ein gebundenes Molekuel zappelt nur noch, statt frei zu wandern. Das ist
			// bewusst eine Daempfung und kein vollstaendiges Anhalten: ein Andockpartner,
			// der schlagartig einfriert, sieht falsch aus, weil auch gebundene Molekuele
			// in Wirklichkeit weiter zittern.
			const float DiffusionScale = Molecule.BoundTo.IsSet() ? Parameters.BoundDiffusionScale : 1.f;

			const FVector3f StepAngstrom = MolecularForge::ComputeBrownianStep(
				Molecule.RandomStream,
				Parameters.DiffusionCoefficient * DiffusionScale,
				DeltaSeconds);

			FTransform& Transform = Transforms[Index].GetMutableTransform();
			FVector3f Position(Transform.GetLocation());

			Position += StepAngstrom * Parameters.UnitsPerAngstrom;

			// Die Geschwindigkeit wird aus dem tatsaechlichen Weg abgeleitet und nicht
			// integriert. Diffusion hat keinen Impuls — ein Molekuel merkt sich nicht,
			// wohin es zuletzt gestossen wurde. Fuer die Randbedingung und fuer alles,
			// was die Geschwindigkeit ausliest, muss sie trotzdem stimmen.
			FVector3f Velocity = (StepAngstrom * Parameters.UnitsPerAngstrom) / DeltaSeconds;

			MolecularForge::ConfineToBounds(Bounds, Parameters.BoundaryMode, Position, Velocity);

			Transform.SetLocation(FVector(Position));
			Velocities[Index].Value = FVector(Velocity);

			Molecule.Detail = MolecularForge::ComputeDetailLevel(
				FVector3f::DistSquared(Position, Viewer),
				Parameters.FullDetailDistance,
				Parameters.BackboneDistance,
				Parameters.BlobDistance);
		}
	});
}
