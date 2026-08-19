// Copyright Silvan Teufel. All Rights Reserved.

#include "MolecularAtomsComponent.h"
#include "MolecularStructure.h"
#include "MolElementTable.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	/** Radius des Engine-Standardkugelmeshes in Unreal-Einheiten. */
	constexpr float GEngineSphereRadius = 50.f;

	/** Custom-Data-Slots: 0..2 Farbe, 3 Radius in Angstroem. */
	constexpr int32 GNumCustomDataFloats = 4;
}

UMolecularAtomsComponent::UMolecularAtomsComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	// Kugeln muessen weder kollidieren noch Schatten empfangen wollen — bei
	// hunderttausend Instanzen kostet beides mehr, als es einbringt.
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetGenerateOverlapEvents(false);
	NumCustomDataFloats = GNumCustomDataFloats;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		SetStaticMesh(SphereMesh.Object);
	}
}

void UMolecularAtomsComponent::OnRegister()
{
	Super::OnRegister();

	if (Structure && GetInstanceCount() == 0)
	{
		RebuildInstances();
	}
}

void UMolecularAtomsComponent::SetStructure(UMolecularStructure* InStructure)
{
	Structure = InStructure;
	RebuildInstances();
}

float UMolecularAtomsComponent::GetRepresentationRadiusFactor() const
{
	switch (Representation)
	{
	case EMolRepresentation::SpaceFilling:
		return 1.f;
	case EMolRepresentation::BallAndStick:
		// Ein Viertel des Van-der-Waals-Radius ist die uebliche Wahl: gross genug,
		// dass die Kugel das Element noch erkennen laesst, klein genug, dass die
		// Bindungsstaebe dazwischen sichtbar bleiben.
		return 0.25f;
	case EMolRepresentation::Backbone:
	case EMolRepresentation::AlphaTrace:
		return 0.35f;
	default:
		return 1.f;
	}
}

bool UMolecularAtomsComponent::ShouldDrawAtom(int32 AtomIndex) const
{
	const uint8 Flags = Structure->AtomFlags[AtomIndex];

	if (!bShowWater && (Flags & MolAtom_Water) != 0)
	{
		return false;
	}
	if (!bShowHydrogen && Structure->AtomElements[AtomIndex] == 1)
	{
		return false;
	}

	switch (Representation)
	{
	case EMolRepresentation::Backbone:
		// Heterogruppen bleiben sichtbar: der Ligand ist bei einer Rueckgratdarstellung
		// meistens genau das, was man sehen will.
		return (Flags & MolAtom_Backbone) != 0 || (Flags & MolAtom_Hetatm) != 0;

	case EMolRepresentation::AlphaTrace:
		return (Flags & MolAtom_Anchor) != 0 || (Flags & MolAtom_Hetatm) != 0;

	default:
		return true;
	}
}

void UMolecularAtomsComponent::RebuildInstances()
{
	ClearInstances();
	NumVisibleAtoms = 0;

	if (!Structure || Structure->IsEmpty())
	{
		MarkRenderStateDirty();
		return;
	}

	const int32 NumAtoms = Structure->GetNumAtoms();
	const float RadiusFactor = GetRepresentationRadiusFactor() * RadiusScale;

	// Transformationen zuerst vollstaendig aufbauen und in einem Rutsch uebergeben.
	// Instanzweises AddInstance markiert jedes Mal den Renderstate — bei sechsstelligen
	// Atomzahlen ist das der Unterschied zwischen Sekunden und Millisekunden.
	TArray<FTransform> Transforms;
	TArray<FLinearColor> Colors;
	TArray<float> Radii;

	Transforms.Reserve(NumAtoms);
	Colors.Reserve(NumAtoms);
	Radii.Reserve(NumAtoms);

	for (int32 i = 0; i < NumAtoms; ++i)
	{
		if (!ShouldDrawAtom(i))
		{
			continue;
		}

		const float RadiusAngstrom = Structure->GetAtomVdWRadius(i) * RadiusFactor;
		const float ScaleFactor = (RadiusAngstrom * UnitsPerAngstrom) / GEngineSphereRadius;

		Transforms.Emplace(
			FQuat::Identity,
			FVector(Structure->AtomPositions[i]) * UnitsPerAngstrom,
			FVector(ScaleFactor));

		Colors.Add(Structure->GetAtomColor(i, ColorScheme, UniformColor));
		Radii.Add(RadiusAngstrom);
	}

	NumVisibleAtoms = Transforms.Num();
	if (NumVisibleAtoms == 0)
	{
		MarkRenderStateDirty();
		return;
	}

	AddInstances(Transforms, /*bShouldReturnIndices=*/false, /*bWorldSpace=*/false);

	// Farbe und Radius als Custom Data. Das Standardmaterial wertet das noch nicht aus —
	// das Impostor-Material aus Phase 2 tut es, ohne dass sich hier etwas aendern muss.
	for (int32 Instance = 0; Instance < NumVisibleAtoms; ++Instance)
	{
		const FLinearColor& Color = Colors[Instance];
		SetCustomDataValue(Instance, 0, Color.R, /*bMarkRenderStateDirty=*/false);
		SetCustomDataValue(Instance, 1, Color.G, false);
		SetCustomDataValue(Instance, 2, Color.B, false);
		SetCustomDataValue(Instance, 3, Radii[Instance], false);
	}

	MarkRenderStateDirty();

	UE_LOG(LogMolecularForge, Verbose, TEXT("Instanzen aufgebaut: %d von %d Atomen sichtbar."),
		NumVisibleAtoms, NumAtoms);
}

#if WITH_EDITOR
void UMolecularAtomsComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	static const TSet<FName> RebuildTriggers = {
		GET_MEMBER_NAME_CHECKED(UMolecularAtomsComponent, Structure),
		GET_MEMBER_NAME_CHECKED(UMolecularAtomsComponent, Representation),
		GET_MEMBER_NAME_CHECKED(UMolecularAtomsComponent, ColorScheme),
		GET_MEMBER_NAME_CHECKED(UMolecularAtomsComponent, UniformColor),
		GET_MEMBER_NAME_CHECKED(UMolecularAtomsComponent, RadiusScale),
		GET_MEMBER_NAME_CHECKED(UMolecularAtomsComponent, UnitsPerAngstrom),
		GET_MEMBER_NAME_CHECKED(UMolecularAtomsComponent, bShowWater),
		GET_MEMBER_NAME_CHECKED(UMolecularAtomsComponent, bShowHydrogen)
	};

	const FName Changed = PropertyChangedEvent.GetPropertyName();
	if (RebuildTriggers.Contains(Changed))
	{
		RebuildInstances();
	}
}
#endif
