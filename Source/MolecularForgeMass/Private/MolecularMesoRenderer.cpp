// Copyright Simulated Flow. All Rights Reserved.

#include "MolecularMesoRenderer.h"
#include "MolMesoscaleFragments.h"
#include "MolecularAtomsComponent.h"
#include "MolecularStructure.h"
#include "MolecularForgeTypes.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Mass/EntityFragments.h"
#include "MassEntityManager.h"
#include "MassEntityQuery.h"
#include "MassEntitySubsystem.h"
#include "MassExecutionContext.h"

namespace
{
	/** Radius des Engine-Standardkugelmeshes in Unreal-Einheiten. */
	constexpr float GEngineSphereRadius = 50.f;
}

AMolecularMesoRenderer::AMolecularMesoRenderer()
{
	PrimaryActorTick.bCanEverTick = true;
	// Nach allen Mass-Phasen: erst dann stehen die Transformationen dieses Bildes fest.
	PrimaryActorTick.TickGroup = TG_LastDemotable;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	BackboneInstances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("BackboneInstances"));
	BackboneInstances->SetupAttachment(Root);
	BackboneInstances->SetMobility(EComponentMobility::Movable);
	BackboneInstances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BackboneInstances->SetCastShadow(false);

	BlobInstances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("BlobInstances"));
	BlobInstances->SetupAttachment(Root);
	BlobInstances->SetMobility(EComponentMobility::Movable);
	BlobInstances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BlobInstances->SetCastShadow(false);
}

void AMolecularMesoRenderer::BeginPlay()
{
	Super::BeginPlay();

	auto ResolveMesh = [](const TSoftObjectPtr<UStaticMesh>& Soft, const TCHAR* FallbackPath)
	{
		UStaticMesh* Mesh = Soft.LoadSynchronous();
		return Mesh ? Mesh : LoadObject<UStaticMesh>(nullptr, FallbackPath);
	};

	BackboneInstances->SetStaticMesh(ResolveMesh(BackboneMesh, TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
	BlobInstances->SetStaticMesh(ResolveMesh(BlobMesh, TEXT("/Engine/BasicShapes/Sphere.Sphere")));

	UWorld* World = GetWorld();
	UMassEntitySubsystem* EntitySubsystem = World ? World->GetSubsystem<UMassEntitySubsystem>() : nullptr;
	if (!EntitySubsystem)
	{
		UE_LOG(LogMolecularForge, Warning,
			TEXT("Mesoskala-Renderer: kein Mass-Entity-Subsystem, es wird nichts gezeichnet."));
		return;
	}

	MoleculeQuery = MakeUnique<FMassEntityQuery>(EntitySubsystem->GetMutableEntityManager().AsShared());
	MoleculeQuery->AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	MoleculeQuery->AddRequirement<FMolMoleculeFragment>(EMassFragmentAccess::ReadOnly);
	MoleculeQuery->AddTagRequirement<FMolMoleculeTag>(EMassFragmentPresence::All);

	EnsureAtomPool();
}

void AMolecularMesoRenderer::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	MoleculeQuery.Reset();
	Super::EndPlay(EndPlayReason);
}

void AMolecularMesoRenderer::GetLastCounts(int32& OutFull, int32& OutBackbone, int32& OutBlob, int32& OutHidden) const
{
	OutFull = FullDetail.Num();
	OutBackbone = BackboneDetail.Num();
	OutBlob = BlobDetail.Num();
	OutHidden = LastHiddenCount;
}

void AMolecularMesoRenderer::EnsureAtomPool()
{
	const int32 Desired = FMath::Clamp(MaxFullDetailMolecules, 0, 64);

	// Nur wachsen, nie schrumpfen. Eine einmal erzeugte Komponente wieder abzubauen
	// spart nichts, was den Aufwand rechtfertigen wuerde — und die Zahl schwankt
	// waehrend einer Kamerafahrt staendig.
	while (AtomPool.Num() < Desired)
	{
		const FName ComponentName(*FString::Printf(TEXT("MesoAtoms_%d"), AtomPool.Num()));

		UMolecularAtomsComponent* Component =
			NewObject<UMolecularAtomsComponent>(this, UMolecularAtomsComponent::StaticClass(), ComponentName);

		Component->UnitsPerAngstrom = UnitsPerAngstrom;
		Component->SetupAttachment(GetRootComponent());
		Component->RegisterComponent();
		Component->SetVisibility(false);

		AtomPool.Add(Component);
	}
}

void AMolecularMesoRenderer::UpdateInstancedLevel(UInstancedStaticMeshComponent* Component,
	const TArray<FMesoInstance>& Instances, bool bScaleByRadius)
{
	if (!Component || !Component->GetStaticMesh())
	{
		return;
	}

	TransformScratch.Reset(Instances.Num());

	for (const FMesoInstance& Instance : Instances)
	{
		FTransform Transform = Instance.Transform;

		if (bScaleByRadius)
		{
			const float Scale = (Instance.ContactRadius * UnitsPerAngstrom) / GEngineSphereRadius;
			Transform.SetScale3D(FVector(FMath::Max(Scale, UE_KINDA_SMALL_NUMBER)));
		}

		TransformScratch.Add(Transform);
	}

	// Die Instanzzahl aendert sich nur, wenn Molekuele die Stufe wechseln. Im ruhigen
	// Fall laeuft deshalb nur der billige Aktualisierungspfad und nicht der Neuaufbau.
	if (Component->GetInstanceCount() != TransformScratch.Num())
	{
		Component->ClearInstances();
		if (!TransformScratch.IsEmpty())
		{
			Component->AddInstances(TransformScratch, /*bShouldReturnIndices=*/false, /*bWorldSpace=*/true);
		}
	}
	else if (!TransformScratch.IsEmpty())
	{
		Component->BatchUpdateInstancesTransforms(0, TransformScratch,
			/*bWorldSpace=*/true, /*bMarkRenderStateDirty=*/true, /*bTeleport=*/true);
	}
}

void AMolecularMesoRenderer::UpdateFullDetail(TArray<FMesoInstance>& Instances)
{
	EnsureAtomPool();

	if (AtomPool.IsEmpty())
	{
		// Ohne Vorrat wandert alles eine Stufe tiefer, statt zu verschwinden.
		BackboneDetail.Append(Instances);
		Instances.Reset();
		return;
	}

	// Die naechsten zuerst: reicht der Vorrat nicht, sollen die Molekuele davon
	// profitieren, bei denen man den Unterschied ueberhaupt sieht.
	Instances.Sort([](const FMesoInstance& A, const FMesoInstance& B)
	{
		return A.DistanceSquared < B.DistanceSquared;
	});

	// Was ueber den Vorrat hinausgeht, wird als Instanz gezeichnet.
	if (Instances.Num() > AtomPool.Num())
	{
		for (int32 i = AtomPool.Num(); i < Instances.Num(); ++i)
		{
			BackboneDetail.Add(Instances[i]);
		}
		Instances.SetNum(AtomPool.Num(), EAllowShrinking::No);
	}

	for (int32 i = 0; i < AtomPool.Num(); ++i)
	{
		UMolecularAtomsComponent* Component = AtomPool[i];
		if (!Component)
		{
			continue;
		}

		if (i >= Instances.Num())
		{
			Component->SetVisibility(false);
			continue;
		}

		const FMesoInstance& Instance = Instances[i];

		UMolecularStructure* Structure = Species.IsValidIndex(Instance.SpeciesIndex)
			? Species[Instance.SpeciesIndex].Get()
			: nullptr;

		if (!Structure)
		{
			// Keine Struktur hinterlegt — dann kann diese Stufe nichts zeigen, und das
			// Molekuel gehoert in die naechstniedrigere statt zu verschwinden.
			Component->SetVisibility(false);
			BackboneDetail.Add(Instance);
			continue;
		}

		// Nur neu aufbauen, wenn sich die Struktur wirklich geaendert hat. Der Aufbau
		// kostet so viel wie das Laden einer Struktur und darf nicht je Bild anfallen.
		if (Component->Structure != Structure)
		{
			Component->UnitsPerAngstrom = UnitsPerAngstrom;
			Component->SetStructure(Structure);
		}

		Component->SetWorldTransform(Instance.Transform);
		Component->SetVisibility(true);
	}
}

void AMolecularMesoRenderer::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!MoleculeQuery.IsValid())
	{
		return;
	}

	UWorld* World = GetWorld();
	UMassEntitySubsystem* EntitySubsystem = World ? World->GetSubsystem<UMassEntitySubsystem>() : nullptr;
	if (!EntitySubsystem)
	{
		return;
	}

	FullDetail.Reset();
	BackboneDetail.Reset();
	BlobDetail.Reset();
	LastHiddenCount = 0;

	FVector ViewerLocation = FVector::ZeroVector;
	if (const APlayerController* Controller = World->GetFirstPlayerController())
	{
		FRotator ViewRotation = FRotator::ZeroRotator;
		Controller->GetPlayerViewPoint(ViewerLocation, ViewRotation);
	}

	FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
	FMassExecutionContext ExecutionContext(EntityManager, DeltaSeconds, /*bFlushDeferredCommands=*/false);

	MoleculeQuery->ForEachEntityChunk(ExecutionContext,
		[this, ViewerLocation](FMassExecutionContext& ChunkContext)
	{
		const TConstArrayView<FTransformFragment> Transforms =
			ChunkContext.GetFragmentView<FTransformFragment>();
		const TConstArrayView<FMolMoleculeFragment> Molecules =
			ChunkContext.GetFragmentView<FMolMoleculeFragment>();

		for (FMassExecutionContext::FEntityIterator It = ChunkContext.CreateEntityIterator(); It; ++It)
		{
			const FMolMoleculeFragment& Molecule = Molecules[It];

			if (Molecule.Detail == EMolMesoDetail::Hidden)
			{
				++LastHiddenCount;
				continue;
			}

			FMesoInstance Instance;
			Instance.Transform = Transforms[It].GetTransform();
			Instance.ContactRadius = Molecule.ContactRadius;
			Instance.SpeciesIndex = Molecule.SpeciesIndex;
			Instance.DistanceSquared =
				static_cast<float>(FVector::DistSquared(Instance.Transform.GetLocation(), ViewerLocation));

			switch (Molecule.Detail)
			{
			case EMolMesoDetail::Full:		FullDetail.Add(Instance); break;
			case EMolMesoDetail::Backbone:	BackboneDetail.Add(Instance); break;
			default:						BlobDetail.Add(Instance); break;
			}
		}
	});

	// Zuerst die nahe Stufe: sie schiebt ueberzaehlige Molekuele in die mittlere,
	// und die muss danach vollstaendig sein.
	UpdateFullDetail(FullDetail);

	UpdateInstancedLevel(BackboneInstances, BackboneDetail, /*bScaleByRadius=*/true);
	UpdateInstancedLevel(BlobInstances, BlobDetail, /*bScaleByRadius=*/true);
}
