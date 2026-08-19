// Copyright Simulated Flow. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MolMesoscaleMath.h"
#include "MolecularMesoRenderer.generated.h"

struct FMassEntityQuery;
class UInstancedStaticMeshComponent;
class UMolecularAtomsComponent;
class UMolecularStructure;
class UStaticMesh;

/**
 * Zeichnet die Molekuele der mesoskopischen Ebene, gestaffelt nach Abstand.
 *
 * Der Sinn der Staffelung: in einem Zellausschnitt stehen zehntausende Molekuele, aber nur
 * eine Handvoll ist nah genug, dass man einzelne Atome ueberhaupt unterscheiden koennte.
 * Alle gleich zu zeichnen hiesse, den Grossteil der Rechenzeit in Details zu stecken, die
 * kleiner als ein Bildpunkt sind.
 *
 *   nah:    echte Atomdarstellung ueber einen kleinen Vorrat an Kugelkomponenten
 *   mittel: eine Instanz je Molekuel mit einem einfachen Ersatzmesh
 *   fern:   eine Kugel in Molekuelgroesse
 *   sehr weit: gar nichts
 *
 * Der Vorrat fuer die nahe Stufe ist bewusst klein und fest: Komponenten je Bild zu
 * erzeugen und wieder wegzuwerfen waere teurer als alles, was man damit einspart. Sind
 * mehr Molekuele nah als der Vorrat hergibt, bekommen die naechsten den Vorzug und der
 * Rest wird als Instanz gezeichnet.
 */
UCLASS(ClassGroup = (MolecularForge))
class MOLECULARFORGEMASS_API AMolecularMesoRenderer : public AActor
{
	GENERATED_BODY()

public:
	AMolecularMesoRenderer();

	/**
	 * Die Molekuelarten, in derselben Reihenfolge wie der Artenindex der Entities.
	 * Leer heisst: es wird nur die Kugeldarstellung benutzt.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MolecularForge")
	TArray<TObjectPtr<UMolecularStructure>> Species;

	/** Ersatzmesh der mittleren Stufe. Ohne Angabe wird eine Engine-Kapsel benutzt. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MolecularForge")
	TSoftObjectPtr<UStaticMesh> BackboneMesh;

	/** Mesh der fernen Stufe. Ohne Angabe wird eine Engine-Kugel benutzt. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MolecularForge")
	TSoftObjectPtr<UStaticMesh> BlobMesh;

	/**
	 * Wie viele Molekuele hoechstens mit echten Atomen gezeichnet werden.
	 * Jedes davon kostet so viel wie eine einzelne geladene Struktur.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MolecularForge", meta = (ClampMin = "0", ClampMax = "64"))
	int32 MaxFullDetailMolecules = 8;

	/** Umrechnung Angstroem -> Unreal-Einheiten. Muss zu den Mass-Parametern passen. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MolecularForge", meta = (ClampMin = "0.01"))
	float UnitsPerAngstrom = 10.f;

	/** Zahl der zuletzt gezeichneten Molekuele je Stufe. Fuer Anzeige und Diagnose. */
	UFUNCTION(BlueprintPure, Category = "MolecularForge")
	void GetLastCounts(int32& OutFull, int32& OutBackbone, int32& OutBlob, int32& OutHidden) const;

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** Ein Molekuel, wie der Renderer es fuer dieses Bild braucht. */
	struct FMesoInstance
	{
		FTransform Transform;
		float ContactRadius = 0.f;
		int32 SpeciesIndex = 0;
		float DistanceSquared = 0.f;
	};

	void EnsureAtomPool();
	void UpdateInstancedLevel(UInstancedStaticMeshComponent* Component,
		const TArray<FMesoInstance>& Instances, bool bScaleByRadius);
	void UpdateFullDetail(TArray<FMesoInstance>& Instances);

	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> BackboneInstances;

	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> BlobInstances;

	UPROPERTY()
	TArray<TObjectPtr<UMolecularAtomsComponent>> AtomPool;

	TUniquePtr<FMassEntityQuery> MoleculeQuery;

	// Wiederverwendete Puffer, damit je Bild nichts neu angefordert wird.
	TArray<FMesoInstance> FullDetail;
	TArray<FMesoInstance> BackboneDetail;
	TArray<FMesoInstance> BlobDetail;
	TArray<FTransform> TransformScratch;

	int32 LastHiddenCount = 0;
};
