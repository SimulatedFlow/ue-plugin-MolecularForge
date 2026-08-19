// Copyright Simulated Flow. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTraitBase.h"
#include "MolMesoscaleFragments.h"
#include "MolMesoscaleTrait.generated.h"

/**
 * Macht aus einer Mass-Entity ein diffundierendes Molekuel.
 *
 * Wird an eine Mass-Entity-Konfiguration gehaengt und bringt alles mit, was die beiden
 * Prozessoren brauchen: Transform und Geschwindigkeit aus der Engine, den eigenen
 * Molekuelzustand und die gemeinsamen Parameter.
 */
UCLASS(meta = (DisplayName = "MolecularForge Molekuel"))
class MOLECULARFORGEMASS_API UMolMesoscaleTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "MolecularForge")
	FMolMesoscaleParameters Parameters;

	/** Kontaktradius der Molekuele dieser Konfiguration, in Angstroem. */
	UPROPERTY(EditAnywhere, Category = "MolecularForge", meta = (ClampMin = "0.1"))
	float ContactRadiusAngstrom = 12.f;

	/**
	 * Startwert der Zufallsquelle.
	 * Ein fester Wert macht den Lauf wiederholbar — hilfreich, wenn ein Video zweimal
	 * gleich aussehen soll. 0 heisst: jedes Mal anders.
	 */
	UPROPERTY(EditAnywhere, Category = "MolecularForge")
	int32 RandomSeed = 0;

protected:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};
