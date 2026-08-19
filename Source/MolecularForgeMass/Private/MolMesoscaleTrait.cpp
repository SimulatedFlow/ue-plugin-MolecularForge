// Copyright Simulated Flow. All Rights Reserved.

#include "MolMesoscaleTrait.h"

#include "Mass/EntityFragments.h"
#include "MassEntityTemplateRegistry.h"
#include "MassEntityUtils.h"
#include "MassMovementFragments.h"

void UMolMesoscaleTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	BuildContext.AddFragment<FTransformFragment>();
	BuildContext.AddFragment<FMassVelocityFragment>();
	BuildContext.AddFragment<FMolMoleculeFragment>();
	BuildContext.AddTag<FMolMoleculeTag>();

	// Parameter zurechtruecken, bevor sie geteilt werden. Danach sind sie unveraenderlich,
	// und ein unsinniger Wert liesse sich nicht mehr korrigieren.
	FMolMesoscaleParameters Validated = Parameters;

	// Ein Raum ohne Ausdehnung wuerde jedes Molekuel im selben Punkt festhalten.
	Validated.BoundsExtent = Validated.BoundsExtent.ComponentMax(FVector(1.0, 1.0, 1.0));

	// Die Detailstufen muessen aufsteigend sein, sonst waere eine unerreichbar.
	Validated.BackboneDistance = FMath::Max(Validated.BackboneDistance, Validated.FullDetailDistance);
	Validated.BlobDistance = FMath::Max(Validated.BlobDistance, Validated.BackboneDistance);

	if (Validated.BindingGridCellSize <= 0.f)
	{
		// Automatisch: zwei Kontaktradien, umgerechnet in Unreal-Einheiten. Groessere
		// Zellen wuerden mehr Kandidaten je Abfrage liefern, kleinere mehr Zellen.
		Validated.BindingGridCellSize =
			FMath::Max(2.f * ContactRadiusAngstrom * Validated.UnitsPerAngstrom, 1.f);
	}

	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(World);
	const FConstSharedStruct SharedParameters = EntityManager.GetOrCreateConstSharedFragment(Validated);
	BuildContext.AddConstSharedFragment(SharedParameters);
}
