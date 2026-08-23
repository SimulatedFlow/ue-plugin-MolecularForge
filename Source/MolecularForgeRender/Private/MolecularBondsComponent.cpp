// Copyright 2026 Silvan Teufel All Rights Reserved.

#include "MolecularBondsComponent.h"
#include "MolecularStructure.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	/** Masse des Engine-Standardzylinders: Radius 50, Hoehe 100, Achse Z, um den Ursprung zentriert. */
	constexpr float GEngineCylinderRadius = 50.f;
	constexpr float GEngineCylinderHeight = 100.f;

	/** Custom-Data-Slots: 0..2 Farbe, 3 Radius in Angstroem. Gleiche Belegung wie bei den Atomen. */
	constexpr int32 GNumCustomDataFloats = 4;
}

UMolecularBondsComponent::UMolecularBondsComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetGenerateOverlapEvents(false);
	NumCustomDataFloats = GNumCustomDataFloats;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMesh.Succeeded())
	{
		SetStaticMesh(CylinderMesh.Object);
	}

	// Dasselbe Material wie bei den Kugeln: auch die Staebe tragen ihre Farbe als
	// Per-Instance-Daten, und beide Haelften einer Bindung sollen genau so aussehen
	// wie die Atome, an denen sie haengen.
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> DefaultBondMaterial(
		TEXT("/MolecularForge/MolecularForge/Materials/M_MF_Atoms.M_MF_Atoms"));
	if (DefaultBondMaterial.Succeeded())
	{
		BondMaterial = DefaultBondMaterial.Object;
		SetMaterial(0, BondMaterial);
	}
}

void UMolecularBondsComponent::OnRegister()
{
	Super::OnRegister();

	if (BondMaterial)
	{
		SetMaterial(0, BondMaterial);
	}

	if (Structure && GetInstanceCount() == 0)
	{
		RebuildInstances();
	}
}

void UMolecularBondsComponent::SetStructure(UMolecularStructure* InStructure)
{
	Structure = InStructure;
	RebuildInstances();
}

bool UMolecularBondsComponent::ShouldDrawAtom(int32 AtomIndex) const
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
	return true;
}

void UMolecularBondsComponent::RebuildInstances()
{
	ClearInstances();
	NumVisibleBonds = 0;
	InstanceSegments.Reset();

	if (!Structure || Structure->GetNumBonds() == 0)
	{
		MarkRenderStateDirty();
		return;
	}

	const float RadiusScale = (BondRadiusAngstrom * UnitsPerAngstrom) / GEngineCylinderRadius;

	TArray<FTransform> Transforms;
	TArray<FLinearColor> Colors;
	Transforms.Reserve(Structure->GetNumBonds() * (bSplitByAtomColor ? 2 : 1));
	Colors.Reserve(Transforms.Max());

	// Setzt einen Zylinder so, dass er von Start nach Ende reicht. Der Engine-Zylinder
	// steht auf der Z-Achse und ist um seinen Mittelpunkt zentriert, also wird er auf die
	// Mitte der Strecke gesetzt, entlang Z gestreckt und auf die Richtung gedreht.
	auto AddSegment = [&](const FVector& Start, const FVector& End, const FLinearColor& Color,
		int32 FromAtom, int32 ToAtom, uint8 Half)
	{
		const FVector Delta = End - Start;
		const double Length = Delta.Size();
		if (Length <= UE_SMALL_NUMBER)
		{
			return;
		}

		const FQuat Rotation = FRotationMatrix::MakeFromZ(Delta / Length).ToQuat();
		const FVector Scale(RadiusScale, RadiusScale, Length / GEngineCylinderHeight);

		Transforms.Emplace(Rotation, Start + Delta * 0.5, Scale);
		Colors.Add(Color);
		InstanceSegments.Add(FMolBondSegmentRef{ FromAtom, ToAtom, Half });
	};

	for (const FMolBond& Bond : Structure->Bonds)
	{
		if (!Structure->AtomPositions.IsValidIndex(Bond.AtomA) ||
			!Structure->AtomPositions.IsValidIndex(Bond.AtomB))
		{
			continue;
		}
		// Eine Bindung wird nur gezeichnet, wenn beide Enden sichtbar sind — sonst
		// endet der Stab im Nichts.
		if (!ShouldDrawAtom(Bond.AtomA) || !ShouldDrawAtom(Bond.AtomB))
		{
			continue;
		}

		const FVector A = FVector(Structure->AtomPositions[Bond.AtomA]) * UnitsPerAngstrom;
		const FVector B = FVector(Structure->AtomPositions[Bond.AtomB]) * UnitsPerAngstrom;

		if (bSplitByAtomColor)
		{
			const FVector Middle = (A + B) * 0.5;
			AddSegment(A, Middle, Structure->GetAtomColor(Bond.AtomA, ColorScheme, UniformColor),
				Bond.AtomA, Bond.AtomB, 1);
			AddSegment(Middle, B, Structure->GetAtomColor(Bond.AtomB, ColorScheme, UniformColor),
				Bond.AtomA, Bond.AtomB, 2);
		}
		else
		{
			AddSegment(A, B, Structure->GetAtomColor(Bond.AtomA, ColorScheme, UniformColor),
				Bond.AtomA, Bond.AtomB, 0);
		}

		++NumVisibleBonds;
	}

	if (Transforms.IsEmpty())
	{
		MarkRenderStateDirty();
		return;
	}

	AddInstances(Transforms, /*bShouldReturnIndices=*/false, /*bWorldSpace=*/false);

	for (int32 Instance = 0; Instance < Colors.Num(); ++Instance)
	{
		const FLinearColor& Color = Colors[Instance];
		SetCustomDataValue(Instance, 0, Color.R, /*bMarkRenderStateDirty=*/false);
		SetCustomDataValue(Instance, 1, Color.G, false);
		SetCustomDataValue(Instance, 2, Color.B, false);
		// In Unreal-Einheiten, wie bei den Kugeln — die Materialien teilen sich die
		// Belegung der Per-Instance-Daten.
		SetCustomDataValue(Instance, 3, BondRadiusAngstrom * UnitsPerAngstrom, false);
	}

	MarkRenderStateDirty();

	UE_LOG(LogMolecularForge, Verbose, TEXT("Bindungen aufgebaut: %d sichtbar, %d Instanzen."),
		NumVisibleBonds, Transforms.Num());
}

void UMolecularBondsComponent::RefreshTransformsFromStructure()
{
	if (!Structure || Structure->IsEmpty())
	{
		return;
	}

	if (InstanceSegments.Num() != GetInstanceCount())
	{
		RebuildInstances();
		return;
	}

	const float RadiusScale = (BondRadiusAngstrom * UnitsPerAngstrom) / GEngineCylinderRadius;

	TArray<FTransform> Transforms;
	Transforms.SetNumUninitialized(InstanceSegments.Num());

	for (int32 Instance = 0; Instance < InstanceSegments.Num(); ++Instance)
	{
		const FMolBondSegmentRef& Segment = InstanceSegments[Instance];

		if (!Structure->AtomPositions.IsValidIndex(Segment.FromAtom)
			|| !Structure->AtomPositions.IsValidIndex(Segment.ToAtom))
		{
			RebuildInstances();
			return;
		}

		const FVector A = FVector(Structure->AtomPositions[Segment.FromAtom]) * UnitsPerAngstrom;
		const FVector B = FVector(Structure->AtomPositions[Segment.ToAtom]) * UnitsPerAngstrom;
		const FVector Middle = (A + B) * 0.5;

		const FVector Start = (Segment.Half == 2) ? Middle : A;
		const FVector End = (Segment.Half == 1) ? Middle : B;

		const FVector Delta = End - Start;
		const double Length = Delta.Size();

		if (Length <= UE_SMALL_NUMBER)
		{
			// Zwei Atome sind aufeinandergefallen. Der Stab wird auf null gestaucht,
			// damit er nicht als Artefakt stehen bleibt.
			Transforms[Instance] = FTransform(FQuat::Identity, Start, FVector::ZeroVector);
			continue;
		}

		Transforms[Instance] = FTransform(
			FRotationMatrix::MakeFromZ(Delta / Length).ToQuat(),
			Start + Delta * 0.5,
			FVector(RadiusScale, RadiusScale, Length / GEngineCylinderHeight));
	}

	BatchUpdateInstancesTransforms(0, Transforms,
		/*bWorldSpace=*/false, /*bMarkRenderStateDirty=*/true, /*bTeleport=*/true);
}

#if WITH_EDITOR
void UMolecularBondsComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	static const TSet<FName> RebuildTriggers = {
		GET_MEMBER_NAME_CHECKED(UMolecularBondsComponent, Structure),
		GET_MEMBER_NAME_CHECKED(UMolecularBondsComponent, ColorScheme),
		GET_MEMBER_NAME_CHECKED(UMolecularBondsComponent, UniformColor),
		GET_MEMBER_NAME_CHECKED(UMolecularBondsComponent, BondRadiusAngstrom),
		GET_MEMBER_NAME_CHECKED(UMolecularBondsComponent, UnitsPerAngstrom),
		GET_MEMBER_NAME_CHECKED(UMolecularBondsComponent, bSplitByAtomColor),
		GET_MEMBER_NAME_CHECKED(UMolecularBondsComponent, bShowWater),
		GET_MEMBER_NAME_CHECKED(UMolecularBondsComponent, bShowHydrogen)
	};

	if (RebuildTriggers.Contains(PropertyChangedEvent.GetPropertyName()))
	{
		RebuildInstances();
	}
}
#endif
