// Copyright Simulated Flow. All Rights Reserved.

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
}

void UMolecularBondsComponent::OnRegister()
{
	Super::OnRegister();

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
	auto AddSegment = [&](const FVector& Start, const FVector& End, const FLinearColor& Color)
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
			AddSegment(A, Middle, Structure->GetAtomColor(Bond.AtomA, ColorScheme, UniformColor));
			AddSegment(Middle, B, Structure->GetAtomColor(Bond.AtomB, ColorScheme, UniformColor));
		}
		else
		{
			AddSegment(A, B, Structure->GetAtomColor(Bond.AtomA, ColorScheme, UniformColor));
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
		SetCustomDataValue(Instance, 3, BondRadiusAngstrom, false);
	}

	MarkRenderStateDirty();

	UE_LOG(LogMolecularForge, Verbose, TEXT("Bindungen aufgebaut: %d sichtbar, %d Instanzen."),
		NumVisibleBonds, Transforms.Num());
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
