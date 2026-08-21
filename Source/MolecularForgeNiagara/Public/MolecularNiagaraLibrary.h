// Copyright 2026 Simulated Flow All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MolNiagaraArrays.h"
#include "MolecularNiagaraLibrary.generated.h"

class UMolecularStructure;
class UNiagaraComponent;

/**
 * Uebergibt eine Struktur an ein Niagara-System.
 *
 * Damit wird das Molekuel zur Partikelquelle: Aufloesen, Zusammenfalten, Andocken,
 * Stroemungen entlang der Oberflaeche. Das ist der Teil, den ein Python-Werkzeug nicht
 * nachmachen kann — nicht weil die Rechnung schwer waere, sondern weil dort kein
 * Partikelsystem daneben liegt.
 *
 * Zur Umsetzung: die Daten gehen ueber Array-Parameter ins System und nicht ueber ein
 * eigenes Data Interface. Ein eigenes Interface koennte ohne Umkopieren direkt auf die
 * Strukturdaten zugreifen und waere bei staendig wechselnden Daten im Vorteil. Es braucht
 * dafuer aber eigenen Shadercode fuer den GPU-Pfad, und der laesst sich nicht headless
 * pruefen — ein Fehler darin faellt erst im laufenden Effekt auf. Array-Parameter kosten
 * ein einmaliges Umkopieren beim Setzen, laufen dafuer auf CPU- und GPU-Simulationen
 * unveraendert und sind vollstaendig testbar. Bei einer statischen Struktur wird ohnehin
 * einmal gesetzt und danach nur noch gelesen.
 */
UCLASS()
class MOLECULARFORGENIAGARA_API UMolecularNiagaraLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Schreibt Atompositionen, Farben und Radien in die Array-Parameter des Systems.
	 *
	 * Das System muss dafuer Benutzerparameter mit diesen Namen besitzen:
	 *   `MolAtomPositions` (Vector-Array), `MolAtomColors` (Color-Array),
	 *   `MolAtomRadii` (Float-Array), `MolAtomCount` (Int).
	 * Mit Bindungen zusaetzlich `MolBondStarts`, `MolBondEnds` (Vector-Arrays)
	 * und `MolBondCount` (Int).
	 *
	 * @return Zahl der uebergebenen Atome, oder 0 bei Misserfolg.
	 */
	UFUNCTION(BlueprintCallable, Category = "MolecularForge|Niagara",
		meta = (DisplayName = "Struktur an Niagara uebergeben", AdvancedDisplay = "Options"))
	static int32 SetStructureArrays(
		UNiagaraComponent* NiagaraComponent,
		const UMolecularStructure* Structure,
		FMolNiagaraOptions Options);

	/**
	 * Wie viele Partikel bei diesen Einstellungen entstehen wuerden — ohne etwas zu setzen.
	 * Gedacht fuer eine Vorschau im UI, bevor jemand versehentlich 400.000 Partikel anfordert.
	 */
	UFUNCTION(BlueprintPure, Category = "MolecularForge|Niagara")
	static int32 CountNiagaraParticles(const UMolecularStructure* Structure, FMolNiagaraOptions Options);

	/** Parameternamen zum Nachschlagen im UI. */
	UFUNCTION(BlueprintPure, Category = "MolecularForge|Niagara")
	static void GetNiagaraParameterNames(
		FName& OutAtomPositions,
		FName& OutAtomColors,
		FName& OutAtomRadii,
		FName& OutAtomCount);
};
