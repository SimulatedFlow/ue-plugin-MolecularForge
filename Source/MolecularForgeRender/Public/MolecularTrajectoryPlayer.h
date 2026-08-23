// Copyright 2026 Silvan Teufel All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MolecularForgeTypes.h"
#include "MolecularTrajectoryPlayer.generated.h"

class UMolecularStructure;
class UMolecularTrajectory;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMolTrajectoryFrameApplied, float, FrameTime);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMolTrajectoryFinished);

/**
 * Spielt eine Trajektorie auf einer Struktur ab.
 *
 * Der Ablauf ist bewusst einfach gehalten: die Positionen der Struktur werden Bild fuer
 * Bild ueberschrieben, danach holen sich die Darstellungskomponenten die neue Geometrie.
 * Alles andere — Elemente, Bindungen, Auswahl, Faerbung — bleibt unberuehrt, weil sich
 * waehrend einer Molekuedynamik nur die Koordinaten aendern und nicht, was welches Atom ist.
 *
 * Wichtig fuer den Umgang: die Struktur wird dabei tatsaechlich veraendert. Wer die
 * Ausgangskoordinaten noch braucht, bekommt sie ueber `RestoreOriginalPositions` zurueck —
 * sie werden beim ersten Abspielen beiseitegelegt.
 *
 * Band und Oberflaeche werden standardmaessig *nicht* mitgefuehrt. Beide muessen bei jeder
 * Aenderung vollstaendig neu erzeugt werden, und das kostet bei einem mittleren Protein
 * mehr Zeit, als ein Bild bei fluessiger Wiedergabe zur Verfuegung hat. Wer es trotzdem
 * will, kann es einschalten — dann aber besser mit wenigen Bildern je Sekunde.
 */
UCLASS(ClassGroup = (MolecularForge), meta = (BlueprintSpawnableComponent))
class MOLECULARFORGERENDER_API UMolecularTrajectoryPlayer : public UActorComponent
{
	GENERATED_BODY()

public:
	UMolecularTrajectoryPlayer();

	/** Die Struktur, deren Positionen ueberschrieben werden. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MolecularForge")
	TObjectPtr<UMolecularStructure> Structure;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MolecularForge")
	TObjectPtr<UMolecularTrajectory> Trajectory;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MolecularForge")
	bool bPlayOnBeginPlay = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MolecularForge")
	bool bLoop = true;

	/** Bilder je Sekunde. Nicht die Simulationszeit, sondern das Abspieltempo. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MolecularForge", meta = (ClampMin = "0.1"))
	float FramesPerSecond = 30.f;

	/**
	 * Zwischen den Bildern interpolieren.
	 * Ausgeschaltet springt die Wiedergabe von Bild zu Bild — das zeigt ehrlicher, was in
	 * der Datei steht, ruckelt aber bei grossen Speicherabstaenden sichtbar.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MolecularForge")
	bool bInterpolate = true;

	/** Auch Band und Oberflaeche mitfuehren. Teuer — siehe Klassenbeschreibung. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MolecularForge", meta = (AdvancedDisplay))
	bool bUpdateExpensiveRepresentations = false;

	/** Wird nach jedem angewandten Bild ausgeloest. */
	UPROPERTY(BlueprintAssignable, Category = "MolecularForge")
	FMolTrajectoryFrameApplied OnFrameApplied;

	/** Wird ausgeloest, wenn eine Wiedergabe ohne Wiederholung das Ende erreicht. */
	UPROPERTY(BlueprintAssignable, Category = "MolecularForge")
	FMolTrajectoryFinished OnFinished;

	UFUNCTION(BlueprintCallable, Category = "MolecularForge")
	void Play();

	UFUNCTION(BlueprintCallable, Category = "MolecularForge")
	void Pause();

	/** Haelt an und springt zurueck auf das erste Bild. */
	UFUNCTION(BlueprintCallable, Category = "MolecularForge")
	void Stop();

	UFUNCTION(BlueprintPure, Category = "MolecularForge")
	bool IsPlaying() const { return bPlaying; }

	/** Setzt die Wiedergabeposition in Bildern. Nachkommastellen sind erlaubt. */
	UFUNCTION(BlueprintCallable, Category = "MolecularForge")
	void SetFrameTime(float NewFrameTime);

	UFUNCTION(BlueprintPure, Category = "MolecularForge")
	float GetFrameTime() const { return FrameTime; }

	/** Wiedergabeposition von 0 bis 1 — fuer einen Schieberegler im UI. */
	UFUNCTION(BlueprintCallable, Category = "MolecularForge")
	void SetNormalizedTime(float Normalized);

	UFUNCTION(BlueprintPure, Category = "MolecularForge")
	float GetNormalizedTime() const;

	/**
	 * Stellt die Koordinaten wieder her, mit denen die Struktur geladen wurde.
	 * Ohne das bliebe nach dem Abspielen der letzte Zeitschritt stehen.
	 */
	UFUNCTION(BlueprintCallable, Category = "MolecularForge")
	void RestoreOriginalPositions();

	/**
	 * Prueft, ob Trajektorie und Struktur zusammenpassen.
	 * Sie tun es nur, wenn beide gleich viele Atome in gleicher Reihenfolge haben —
	 * die Trajektorie enthaelt nur Koordinaten und keine Namen, sie kann sich also
	 * nicht selbst zuordnen.
	 */
	UFUNCTION(BlueprintPure, Category = "MolecularForge")
	bool IsTrajectoryCompatible(FString& OutReason) const;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;

private:
	/** Schreibt den Zustand bei `FrameTime` in die Struktur und frischt die Darstellung auf. */
	void ApplyCurrentFrame();

	void RefreshRepresentations();

	UPROPERTY(Transient)
	bool bPlaying = false;

	UPROPERTY(Transient)
	float FrameTime = 0.f;

	/** Die Koordinaten vor dem ersten Abspielen. */
	UPROPERTY(Transient)
	TArray<FVector3f> OriginalPositions;

	TArray<FVector3f> SampleBuffer;
};
