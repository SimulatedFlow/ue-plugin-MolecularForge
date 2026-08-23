// Copyright 2026 Silvan Teufel All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MolMesoscaleFragments.h"
#include "MolecularMesoSpawner.generated.h"

/** Eine Molekuelart der Population. */
USTRUCT(BlueprintType)
struct MOLECULARFORGEMASS_API FMolMesoSpecies
{
	GENERATED_BODY()

	/** Name fuer die Anzeige. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MolecularForge")
	FName Name = TEXT("Molekuel");

	/** Wie viele davon. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MolecularForge", meta = (ClampMin = "0"))
	int32 Count = 200;

	/** Kontaktradius in Angstroem — zugleich die Groesse der Kugeldarstellung. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MolecularForge", meta = (ClampMin = "0.1"))
	float ContactRadiusAngstrom = 12.f;
};

/**
 * Setzt eine Molekuelpopulation als Mass-Entities in die Welt.
 *
 * Der uebliche Weg zu Mass-Entities fuehrt ueber Konfigurations-Assets und einen
 * Mass-Spawner. Das ist maechtig, aber wer nur ein Zellinneres fuellen will, soll dafuer
 * nicht erst das Entity-System lernen muessen. Dieser Actor nimmt eine Artenliste und
 * einen Raum und erledigt den Rest.
 *
 * Was danach mit den Molekuelen geschieht, machen die Prozessoren: Diffusion und
 * Randbedingung im Bewegungsprozessor, Andocken und Loesen im Bindungsprozessor. Gezeichnet
 * werden sie vom Mesoskala-Renderer, der dafuer im selben Level stehen muss.
 */
UCLASS(ClassGroup = (MolecularForge))
class MOLECULARFORGEMASS_API AMolecularMesoSpawner : public AActor
{
	GENERATED_BODY()

public:
	AMolecularMesoSpawner();

	/** Die Arten, aus denen die Population besteht. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MolecularForge")
	TArray<FMolMesoSpecies> Species;

	/**
	 * Gemeinsame Parameter — Diffusion, Grenzen, Bindungsraten, Detailstufen.
	 * Nur im Editor einstellbar und nicht in Blueprint: Mass-Shared-Fragmente sind
	 * unveraenderlich, sobald sie einmal angelegt sind, und eine Blueprint-Anbindung
	 * wuerde eine Aenderbarkeit vorspiegeln, die es nicht gibt.
	 */
	UPROPERTY(EditAnywhere, Category = "MolecularForge")
	FMolMesoscaleParameters Parameters;

	/**
	 * Startwert der Zufallsverteilung. Ein fester Wert setzt die Molekuele bei jedem
	 * Start an dieselben Stellen — noetig, wenn eine Aufnahme sich wiederholen soll.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MolecularForge")
	int32 RandomSeed = 1337;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MolecularForge")
	bool bSpawnOnBeginPlay = true;

	/** Setzt die Population. Vorhandene Entities dieses Spawners werden vorher entfernt. */
	UFUNCTION(BlueprintCallable, Category = "MolecularForge")
	int32 SpawnPopulation();

	/** Entfernt alle von diesem Spawner gesetzten Molekuele. */
	UFUNCTION(BlueprintCallable, Category = "MolecularForge")
	void ClearPopulation();

	UFUNCTION(BlueprintPure, Category = "MolecularForge")
	int32 GetSpawnedCount() const { return SpawnedEntities.Num(); }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	TArray<FMassEntityHandle> SpawnedEntities;
};
