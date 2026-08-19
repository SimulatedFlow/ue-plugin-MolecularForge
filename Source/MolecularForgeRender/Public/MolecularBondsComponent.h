// Copyright Simulated Flow. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "MolecularForgeTypes.h"
#include "MolecularBondsComponent.generated.h"

class UMolecularStructure;

/**
 * Stellt die kovalenten Bindungen als Staebe dar — die zweite Haelfte von Ball-and-Stick.
 *
 * Jede Bindung wird in zwei Haelften zerlegt, die je die Farbe ihres eigenen Atoms tragen.
 * Das ist nicht Zierrat, sondern die uebliche Darstellung: an einem durchgehend eingefaerbten
 * Stab liesse sich nicht ablesen, welches Element an welchem Ende sitzt, und genau darum
 * geht es bei Ball-and-Stick.
 *
 * Wie bei den Atomen sind Instanzen der richtige Weg und ein zusammengebackenes Mesh der
 * falsche: ein mittleres Protein hat einige tausend Bindungen, ein grosser Komplex
 * hunderttausende. Als Instanzen kostet jede davon eine Matrix, als Geometrie einen ganzen
 * Zylinder.
 */
UCLASS(ClassGroup = (MolecularForge), meta = (BlueprintSpawnableComponent),
	HideCategories = (Instances, Physics, Collision))
class MOLECULARFORGERENDER_API UMolecularBondsComponent : public UInstancedStaticMeshComponent
{
	GENERATED_BODY()

public:
	UMolecularBondsComponent();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MolecularForge")
	TObjectPtr<UMolecularStructure> Structure;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MolecularForge")
	EMolColorScheme ColorScheme = EMolColorScheme::Element;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MolecularForge")
	FLinearColor UniformColor = FLinearColor(0.8f, 0.8f, 0.8f);

	/** Stabradius in Angstroem. 0,15 entspricht der ueblichen Darstellung. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MolecularForge", meta = (ClampMin = "0.01", UIMax = "0.5"))
	float BondRadiusAngstrom = 0.15f;

	/** Umrechnung Angstroem -> Unreal-Einheiten. Muss zu den Ladeoptionen passen. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MolecularForge", meta = (ClampMin = "0.01"))
	float UnitsPerAngstrom = 10.f;

	/**
	 * Jede Bindung in zwei verschieden gefaerbte Haelften zerlegen.
	 * Ausgeschaltet halbiert das die Instanzzahl, kostet aber die Ablesbarkeit.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MolecularForge")
	bool bSplitByAtomColor = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MolecularForge")
	bool bShowWater = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MolecularForge")
	bool bShowHydrogen = false;

	UFUNCTION(BlueprintCallable, Category = "MolecularForge")
	void SetStructure(UMolecularStructure* InStructure);

	UFUNCTION(BlueprintCallable, Category = "MolecularForge")
	void RebuildInstances();

	UFUNCTION(BlueprintPure, Category = "MolecularForge")
	int32 GetNumVisibleBonds() const { return NumVisibleBonds; }

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

protected:
	virtual void OnRegister() override;

private:
	bool ShouldDrawAtom(int32 AtomIndex) const;

	UPROPERTY(Transient)
	int32 NumVisibleBonds = 0;
};
