// Copyright Simulated Flow. All Rights Reserved.

#include "MolecularCartoonComponent.h"
#include "MolBackboneSpline.h"
#include "MolecularStructure.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

UMolecularCartoonComponent::UMolecularCartoonComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;

	// Das Band ist Anschauung, keine Spielgeometrie. Kollision kostet hier nur Aufbauzeit.
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	bUseComplexAsSimpleCollision = true;

	// Das Band traegt seine Farbe in den Vertices — anders als Kugeln und Staebe ist es
	// ein einziges Mesh und kann keine Instanzdaten haben.
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> DefaultRibbonMaterial(
		TEXT("/MolecularForge/Materials/M_MF_VertexColor.M_MF_VertexColor"));
	if (DefaultRibbonMaterial.Succeeded())
	{
		RibbonMaterial = DefaultRibbonMaterial.Object;
		SetMaterial(0, RibbonMaterial);
	}
}

void UMolecularCartoonComponent::OnRegister()
{
	Super::OnRegister();

	if (RibbonMaterial)
	{
		SetMaterial(0, RibbonMaterial);
	}

	if (Structure && GetNumSections() == 0)
	{
		RebuildMesh();
	}
}

void UMolecularCartoonComponent::SetStructure(UMolecularStructure* InStructure)
{
	Structure = InStructure;
	RebuildMesh();
}

void UMolecularCartoonComponent::RebuildMesh()
{
	ClearAllMeshSections();
	LastTriangleCount = 0;
	LastSegmentCount = 0;

	if (!Structure || Structure->IsEmpty())
	{
		return;
	}

	FMolBackboneOptions BackboneOptions;
	BackboneOptions.SegmentsPerResidue = SegmentsPerResidue;

	TArray<FMolBackboneSegment> Segments;
	MolecularForge::BuildBackboneSegments(*Structure, BackboneOptions, Segments);

	LastSegmentCount = Segments.Num();
	if (Segments.IsEmpty())
	{
		// Keine Polymerkette — etwa eine Datei mit nur einem Liganden. Kein Fehler.
		return;
	}

	FMolRibbonOptions RibbonOptions;
	RibbonOptions.UnitsPerAngstrom = UnitsPerAngstrom;
	RibbonOptions.RingResolution = RingResolution;
	RibbonOptions.ColorScheme = ColorScheme;
	RibbonOptions.UniformColor = UniformColor;
	RibbonOptions.HelixHalfWidth = HelixHalfWidth;
	RibbonOptions.SheetHalfWidth = SheetHalfWidth;
	RibbonOptions.CoilRadius = CoilRadius;

	FMolMeshData Mesh;
	MolecularForge::BuildRibbonMesh(*Structure, Segments, RibbonOptions, Mesh);

	if (Mesh.IsEmpty())
	{
		return;
	}

	// Der Generator rechnet in einfacher Genauigkeit, die Komponente erwartet doppelte.
	// Das Umkopieren ist der Preis dafuer, dass die Erzeugung ohne Renderer testbar bleibt —
	// und es faellt nur einmal je Aufbau an, nicht je Bild.
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

	LastTriangleCount = Mesh.NumTriangles();

	UE_LOG(LogMolecularForge, Verbose,
		TEXT("Cartoon aufgebaut: %d Dreiecke aus %d Abschnitten."), LastTriangleCount, LastSegmentCount);
}

#if WITH_EDITOR
void UMolecularCartoonComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	static const TSet<FName> RebuildTriggers = {
		GET_MEMBER_NAME_CHECKED(UMolecularCartoonComponent, Structure),
		GET_MEMBER_NAME_CHECKED(UMolecularCartoonComponent, ColorScheme),
		GET_MEMBER_NAME_CHECKED(UMolecularCartoonComponent, UniformColor),
		GET_MEMBER_NAME_CHECKED(UMolecularCartoonComponent, UnitsPerAngstrom),
		GET_MEMBER_NAME_CHECKED(UMolecularCartoonComponent, SegmentsPerResidue),
		GET_MEMBER_NAME_CHECKED(UMolecularCartoonComponent, RingResolution),
		GET_MEMBER_NAME_CHECKED(UMolecularCartoonComponent, HelixHalfWidth),
		GET_MEMBER_NAME_CHECKED(UMolecularCartoonComponent, SheetHalfWidth),
		GET_MEMBER_NAME_CHECKED(UMolecularCartoonComponent, CoilRadius)
	};

	if (RebuildTriggers.Contains(PropertyChangedEvent.GetPropertyName()))
	{
		RebuildMesh();
	}
}
#endif
