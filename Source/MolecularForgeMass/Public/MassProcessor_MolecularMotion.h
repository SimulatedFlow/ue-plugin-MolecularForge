// Copyright 2026 Silvan Teufel All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityQuery.h"
#include "MassProcessor.h"
#include "MassProcessor_MolecularMotion.generated.h"

/**
 * Bewegt die Molekuele und bestimmt ihre Darstellungsstufe.
 *
 * Diffusion, Randbedingung und Detailstufe sind in einem Prozessor zusammengefasst, weil
 * alle drei dieselben Daten anfassen: Position und Geschwindigkeit. Drei getrennte
 * Durchlaeufe wuerden dieselben Chunks dreimal durch den Cache ziehen.
 *
 * Der Lauf ist vollstaendig parallelisierbar, weil jede Entity nur in sich selbst
 * schreibt und ihre eigene Zufallsquelle mitbringt. Damit haengt das Ergebnis nicht
 * davon ab, wie die Chunks auf die Threads fallen — bei gleichem Startwert kommt
 * zweimal dasselbe heraus, und ein aufgezeichnetes Video laesst sich wiederholen.
 */
UCLASS(meta = (DisplayName = "MolecularForge Molekuelbewegung"))
class MOLECULARFORGEMASS_API UMassProcessor_MolecularMotion : public UMassProcessor
{
	GENERATED_BODY()

public:
	UMassProcessor_MolecularMotion();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery MotionQuery;

	/**
	 * Blickpunkt fuer die Detailstufe.
	 *
	 * Bewusst nur ein Betrachter: die Engine kann mit mehreren umgehen, aber in den
	 * Anwendungsfaellen dieses Plugins — Bildaufnahme, Video, Praesentation — gibt es
	 * genau eine Kamera. Ein Mehrspieler-Aufbau braeuchte hier die LOD-Verwaltung
	 * der Engine, und das waere Aufwand ohne Abnehmer.
	 */
	FVector GetViewerLocation(const UWorld* World) const;
};
