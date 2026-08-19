// Copyright Simulated Flow. All Rights Reserved.

#include "MolecularSurfaceComponent.h"
#include "MolSurfaceBuilder.h"
#include "MolecularStructure.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

UMolecularSurfaceComponent::UMolecularSurfaceComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;

	SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Wie beim Band: die Farbe der Oberflaeche steckt in den Vertices, dort hat sie
	// die Erzeugung ueber das naechstgelegene Atom hingeschrieben.
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> DefaultSurfaceMaterial(
		TEXT("/MolecularForge/MolecularForge/Materials/M_MF_VertexColor.M_MF_VertexColor"));
	if (DefaultSurfaceMaterial.Succeeded())
	{
		SurfaceMaterial = DefaultSurfaceMaterial.Object;
		SetMaterial(0, SurfaceMaterial);
	}
}

void UMolecularSurfaceComponent::OnRegister()
{
	Super::OnRegister();

	if (SurfaceMaterial)
	{
		SetMaterial(0, SurfaceMaterial);
	}

	if (Structure && GetNumSections() == 0)
	{
		RebuildMesh();
	}
}

void UMolecularSurfaceComponent::SetStructure(UMolecularStructure* InStructure)
{
	Structure = InStructure;
	RebuildMesh();
}

void UMolecularSurfaceComponent::RebuildMesh()
{
	ClearAllMeshSections();
	LastTriangleCount = 0;
	LastError.Empty();

	if (!Structure || Structure->IsEmpty())
	{
		return;
	}

	FMolSurfaceOptions Options;
	Options.UnitsPerAngstrom = UnitsPerAngstrom;
	Options.VoxelSizeAngstrom = VoxelSizeAngstrom;
	Options.RadiusInflationAngstrom = RadiusInflationAngstrom;
	Options.Blobbiness = Blobbiness;
	Options.ColorScheme = ColorScheme;
	Options.UniformColor = UniformColor;
	Options.bShowWater = bShowWater;

	FMolMeshData Mesh;
	if (!MolecularForge::BuildGaussianSurface(*Structure, Options, Mesh, &LastError))
	{
		UE_LOG(LogMolecularForge, Warning, TEXT("Oberflaeche nicht erzeugt: %s"), *LastError);
		return;
	}

	TArray<FVector> Positions;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;

	Positions.Reserve(Mesh.NumVertices());
	Normals.Reserve(Mesh.NumVertices());
	UVs.Reserve(Mesh.NumVertices());

	for (int32 i = 0; i < Mesh.NumVertices(); ++i)
	{
		Positions.Add(FVector(Mesh.Positions[i]));
		Normals.Add(FVector(Mesh.Normals[i]));
		UVs.Add(FVector2D(Mesh.UVs[i]));
	}

	CreateMeshSection(
		/*SectionIndex=*/0,
		Positions,
		Mesh.Triangles,
		Normals,
		UVs,
		Mesh.Colors,
		TArray<FProcMeshTangent>(),
		/*bCreateCollision=*/false);

	// Siehe die gleichlautende Anmerkung am Cartoon-Band: vor dem ersten Abschnitt hat
	// ein Prozedural-Mesh keinen Materialschlitz, und die Zuweisung verpufft.
	if (SurfaceMaterial)
	{
		SetMaterial(0, SurfaceMaterial);
	}

	LastTriangleCount = Mesh.NumTriangles();
}

#if WITH_EDITOR
void UMolecularSurfaceComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Bewusst nur bei Aenderungen, die die Oberflaeche wirklich betreffen — ein Neuaufbau
	// kostet bei einem grossen Molekuel Sekunden, und im Editor wuerde das bei jedem
	// Zahlendreher spuerbar.
	static const TSet<FName> RebuildTriggers = {
		GET_MEMBER_NAME_CHECKED(UMolecularSurfaceComponent, Structure),
		GET_MEMBER_NAME_CHECKED(UMolecularSurfaceComponent, ColorScheme),
		GET_MEMBER_NAME_CHECKED(UMolecularSurfaceComponent, UniformColor),
		GET_MEMBER_NAME_CHECKED(UMolecularSurfaceComponent, UnitsPerAngstrom),
		GET_MEMBER_NAME_CHECKED(UMolecularSurfaceComponent, VoxelSizeAngstrom),
		GET_MEMBER_NAME_CHECKED(UMolecularSurfaceComponent, RadiusInflationAngstrom),
		GET_MEMBER_NAME_CHECKED(UMolecularSurfaceComponent, Blobbiness),
		GET_MEMBER_NAME_CHECKED(UMolecularSurfaceComponent, bShowWater)
	};

	if (RebuildTriggers.Contains(PropertyChangedEvent.GetPropertyName()))
	{
		RebuildMesh();
	}
}
#endif
