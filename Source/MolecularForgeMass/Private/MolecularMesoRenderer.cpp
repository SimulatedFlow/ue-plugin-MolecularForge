// Copyright 2026 Simulated Flow All Rights Reserved.

#include "MolecularMesoRenderer.h"
#include "MolMesoscaleFragments.h"
#include "MolecularAtomsComponent.h"
#include "MolecularStructure.h"
#include "MolecularForgeTypes.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"
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

	// Die beiden Instanzkomponenten entstehen nicht hier, sondern erst beim Spielstart —
	// siehe CreateInstanceComponent().
}

UInstancedStaticMeshComponent* AMolecularMesoRenderer::CreateInstanceComponent(FName Name)
{
	// Bewusst die Atomkugel-Komponente des Plugins und keine nackte Instanzkomponente:
	// sie ist selbst eine Instanzkomponente, bringt Kugelmesh, Materialbelegung und die
	// vier Instanzwerte fertig mit, und ohne zugewiesene Struktur baut sie von sich aus
	// nichts. Eine nackte Komponente blieb in allen Versuchen grau, obwohl Material und
	// Instanzdaten nachweislich richtig gesetzt waren.
	UMolecularAtomsComponent* Component =
		NewObject<UMolecularAtomsComponent>(this, UMolecularAtomsComponent::StaticClass(), Name);

	Component->UnitsPerAngstrom = UnitsPerAngstrom;

	// Beweglich, weil die Molekuele jedes Bild an anderer Stelle stehen. Die Voreinstellung
	// einer Instanzkomponente ist unbeweglich, und das passt zu einer Population, die
	// ununterbrochen diffundiert, ersichtlich nicht.
	Component->SetMobility(EComponentMobility::Movable);

	// Belegung wie bei den Atomkugeln: 0..2 Farbe, 3 Radius in Welteinheiten. Ueber den
	// Setter und nicht ueber das Feld: nur der Setter passt den Instanzpuffer mit an.
	Component->SetNumCustomDataFloats(4);
	Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Component->SetGenerateOverlapEvents(false);
	Component->SetCanEverAffectNavigation(false);

	if (UMaterialInterface* Material = InstanceMaterial.LoadSynchronous())
	{
		Component->AtomMaterial = Material;
	}

	Component->SetupAttachment(GetRootComponent());
	Component->RegisterComponent();
	AddInstanceComponent(Component);
	return Component;
}

void AMolecularMesoRenderer::BeginPlay()
{
	Super::BeginPlay();

	// Voreingestellt ist auch die mittlere Stufe eine Kugel. Ein Zylinder waere leichter
	// von der fernen Stufe zu unterscheiden, sieht als Molekuel aber falsch aus — das Bild
	// soll ein Zellinneres zeigen und kein Lager voller Fassdauben. Wer eine eigene
	// Ersatzform hat, traegt sie oben ein.
	BackboneInstances = CreateInstanceComponent(TEXT("BackboneInstances"));
	BlobInstances = CreateInstanceComponent(TEXT("BlobInstances"));

	// Eine eigene Ersatzform kommt erst nach dem Registrieren zum Zug. Der Meshwechsel
	// wirft das Material der Komponente weg, deshalb wird es unmittelbar danach neu
	// gesetzt und der Zeichenzustand erneuert.
	auto ApplyCustomMesh = [this](UInstancedStaticMeshComponent* Component,
		const TSoftObjectPtr<UStaticMesh>& Soft)
	{
		UStaticMesh* Custom = Soft.LoadSynchronous();
		if (!Component || !Custom)
		{
			return;
		}

		Component->SetStaticMesh(Custom);
		if (UMaterialInterface* Material = ResolveInstanceMaterial())
		{
			Component->SetMaterial(0, Material);
		}
		Component->RecreateRenderState_Concurrent();
	};

	ApplyCustomMesh(BackboneInstances, BackboneMesh);
	ApplyCustomMesh(BlobInstances, BlobMesh);

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

UMaterialInterface* AMolecularMesoRenderer::ResolveInstanceMaterial() const
{
	UMaterialInterface* Material = InstanceMaterial.LoadSynchronous();
	if (!Material)
	{
		Material = LoadObject<UMaterialInterface>(nullptr,
			TEXT("/MolecularForge/MolecularForge/Materials/M_MF_Atoms.M_MF_Atoms"));
	}

	if (!Material)
	{
		UE_LOG(LogMolecularForge, Warning,
			TEXT("Mesoskala: kein Instanzmaterial gefunden — die Molekuele bleiben einfarbig."));
	}

	return Material;
}

FLinearColor AMolecularMesoRenderer::GetSpeciesColor(int32 SpeciesIndex) const
{
	if (SpeciesColors.IsValidIndex(SpeciesIndex))
	{
		return SpeciesColors[SpeciesIndex];
	}

	// Ersatzreihe fuer den Fall, dass niemand Farben eingetragen hat. Die Toene sind im
	// Farbkreis weit auseinander gelegt, damit auch benachbarte Arten im Bild sofort
	// auseinanderfallen — eine durchlaufende Skala saehe bei zwei Arten fast gleich aus.
	static const FLinearColor Fallback[] = {
		FLinearColor(0.25f, 0.62f, 0.95f),   // Blau
		FLinearColor(0.95f, 0.55f, 0.20f),   // Orange
		FLinearColor(0.40f, 0.85f, 0.45f),   // Gruen
		FLinearColor(0.90f, 0.35f, 0.60f),   // Magenta
		FLinearColor(0.85f, 0.82f, 0.30f),   // Gelb
		FLinearColor(0.60f, 0.45f, 0.90f),   // Violett
	};

	const int32 Count = UE_ARRAY_COUNT(Fallback);
	return Fallback[((SpeciesIndex % Count) + Count) % Count];
}

void AMolecularMesoRenderer::UpdateInstancedLevel(UInstancedStaticMeshComponent* Component,
	const TArray<FMesoInstance>& Instances, bool bScaleByRadius)
{
	if (!Component || !Component->GetStaticMesh())
	{
		return;
	}

	TransformScratch.Reset(Instances.Num());
	CustomDataScratch.Reset(Instances.Num() * 4);

	for (const FMesoInstance& Instance : Instances)
	{
		FTransform Transform = Instance.Transform;

		const float RadiusUnits = Instance.ContactRadius * UnitsPerAngstrom;
		if (bScaleByRadius)
		{
			const float Scale = RadiusUnits / GEngineSphereRadius;
			Transform.SetScale3D(FVector(FMath::Max(Scale, UE_KINDA_SMALL_NUMBER)));
		}

		TransformScratch.Add(Transform);

		const FLinearColor Color = GetSpeciesColor(Instance.SpeciesIndex);
		CustomDataScratch.Add(Color.R);
		CustomDataScratch.Add(Color.G);
		CustomDataScratch.Add(Color.B);
		CustomDataScratch.Add(RadiusUnits);
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

	// Die Instanzdaten muessen jedes Bild neu geschrieben werden, obwohl sich die Farben
	// nie aendern: die Liste ist nach Abstand sortiert, also steht in Platz n von Bild zu
	// Bild ein anderes Molekuel. Vier Fliesskommazahlen je Instanz sind billig, aber es
	// ist Arbeit, die man sich sparen koennte, wenn man je Art eine eigene Komponente
	// fuehrte. Bei wenigen Arten waere das der schlechtere Tausch.
	if (Component->NumCustomDataFloats == 4 && Component->GetInstanceCount() == TransformScratch.Num())
	{
		for (int32 Index = 0; Index < TransformScratch.Num(); ++Index)
		{
			const float* Values = CustomDataScratch.GetData() + Index * 4;
			Component->SetCustomDataValue(Index, 0, Values[0], /*bMarkRenderStateDirty=*/false);
			Component->SetCustomDataValue(Index, 1, Values[1], false);
			Component->SetCustomDataValue(Index, 2, Values[2], false);
			Component->SetCustomDataValue(Index, 3, Values[3], false);
		}

		if (!TransformScratch.IsEmpty())
		{
			Component->MarkRenderStateDirty();
		}
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

	// Einmal melden, wie sich die Molekuele auf die Stufen verteilen. Ohne diese Zeile
	// liesse sich am Bild nicht unterscheiden, ob die Staffelung arbeitet oder ob alles
	// zufaellig in derselben Stufe gelandet ist — beide Stufen zeichnen Kugeln.
	if (!bLoggedDistribution && (FullDetail.Num() + BackboneDetail.Num() + BlobDetail.Num()) > 0)
	{
		bLoggedDistribution = true;
		UE_LOG(LogMolecularForge, Log,
			TEXT("Mesoskala-Darstellung: %d nah, %d mittel, %d fern, %d ausgeblendet."),
			FullDetail.Num(), BackboneDetail.Num(), BlobDetail.Num(), LastHiddenCount);

	}
}
