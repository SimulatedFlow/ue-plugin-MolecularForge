// Copyright Simulated Flow. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityQuery.h"
#include "MassProcessor.h"
#include "MassProcessor_MolecularBinding.generated.h"

/**
 * Laesst Molekuele aneinander andocken und sich wieder loesen.
 *
 * Zweistufig, wie es sich bei Nachbarschaftsproblemen bewaehrt hat:
 *   Stufe 1, seriell und billig: Positionen und Kennungen aller Molekuele in flache
 *            Arrays spiegeln und in ein Uniform-Grid einsortieren. Linear in der Zahl.
 *   Stufe 2, parallel und teuer: je Molekuel die Nachbarn aus dem *eingefrorenen*
 *            Abbild pruefen und entscheiden.
 *
 * Warum jede Entity nur in sich selbst schreibt: eine Bindung betrifft zwei Partner, und
 * beide gleichzeitig zu beschreiben waere aus mehreren Threads heraus ein Wettlauf. Also
 * merkt sich jedes Molekuel nur, woran *es* haengt. Wenn beide einander waehlen, sind sie
 * gegenseitig gebunden; waehlt nur eines, haengt es einseitig an — was fuer ein Bild
 * voellig genuegt und dem Fall entspricht, dass mehrere Liganden an demselben Protein
 * sitzen.
 *
 * Zur Einordnung: das ist keine Bindungskinetik. Echte Kinetik braucht Kraftfelder und
 * Zeitschritte im Femtosekundenbereich. Was hier entsteht, ist ein glaubwuerdiges Bild
 * von Andocken und Loesen mit einstellbaren Raten.
 */
UCLASS(meta = (DisplayName = "MolecularForge Molekuelbindung"))
class MOLECULARFORGEMASS_API UMassProcessor_MolecularBinding : public UMassProcessor
{
	GENERATED_BODY()

public:
	UMassProcessor_MolecularBinding();

	/** Wie viele Molekuele im letzten Bild gebunden waren. Fuer Anzeige und Diagnose. */
	int32 GetLastBoundCount() const { return LastBoundCount; }

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery SnapshotQuery;
	FMassEntityQuery BindingQuery;

	// ---- Abbild des laufenden Bildes (Struct-of-Arrays) ----
	TArray<FVector3f> SnapshotPositions;
	TArray<float> SnapshotRadii;
	TArray<FMassEntityHandle> SnapshotHandles;

	// ---- Uniform-Grid als verkettete Liste ----
	TMap<FIntVector, int32> CellHeadIndex;
	TArray<int32> NextInCell;

	float CachedCellSize = 0.f;

	int32 LastBoundCount = 0;

	void BuildGrid(float CellSize);
	FIntVector ComputeCell(const FVector3f& Position, float InverseCellSize) const;
};
