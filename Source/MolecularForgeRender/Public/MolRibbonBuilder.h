// Copyright 2026 Silvan Teufel All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MolecularForgeTypes.h"
#include "MolBackboneSpline.h"

class UMolecularStructure;

/**
 * Rohe Meshdaten, ohne Bindung an einen Komponententyp.
 *
 * Bewusst als schlichte Arrays und nicht als FDynamicMesh3 oder Procedural-Mesh-Abschnitt:
 * so laesst sich die Erzeugung ohne Renderer und ohne Editor pruefen, und der Zielcontainer
 * bleibt austauschbar. Positionen stehen bereits in Unreal-Einheiten — anders als sonst im
 * Plugin, weil ein Mesh im Gegensatz zu einer Instanztransformation keine eigene Skalierung
 * mitbringt.
 */
struct FMolMeshData
{
	TArray<FVector3f> Positions;
	TArray<FVector3f> Normals;
	TArray<FVector2f> UVs;
	TArray<FColor> Colors;
	TArray<int32> Triangles;

	int32 NumVertices() const { return Positions.Num(); }
	int32 NumTriangles() const { return Triangles.Num() / 3; }
	bool IsEmpty() const { return Triangles.IsEmpty(); }

	void Reset()
	{
		Positions.Reset();
		Normals.Reset();
		UVs.Reset();
		Colors.Reset();
		Triangles.Reset();
	}
};

/** Masse und Aussehen des Bandes. Alle Laengen in Angstroem. */
struct FMolRibbonOptions
{
	float UnitsPerAngstrom = 10.f;

	/** Stuetzpunkte je Querschnitt. 12 reicht fuer ein glattes Band, 8 spart Dreiecke. */
	int32 RingResolution = 12;

	EMolColorScheme ColorScheme = EMolColorScheme::SecondaryStructure;
	FLinearColor UniformColor = FLinearColor::White;

	/** Rundes Profil fuer Schleifen. */
	float CoilRadius = 0.25f;

	/** Turn wird etwas dicker gezeichnet als Coil, damit er sich absetzt. */
	float TurnRadius = 0.35f;

	/** Flaches Band fuer Helices: breit in Querrichtung, duenn in der Normalen. */
	float HelixHalfWidth = 0.90f;
	float HelixHalfThickness = 0.18f;

	/** Faltblattkoerper — etwas breiter als die Helix, damit beide unterscheidbar bleiben. */
	float SheetHalfWidth = 1.00f;
	float SheetHalfThickness = 0.18f;

	/** Breite an der Basis der Pfeilspitze. */
	float ArrowHalfWidth = 1.60f;

	/** Laenge der Pfeilspitze in Spline-Punkten. */
	int32 ArrowLengthInPoints = 8;

	/** Enden schliessen. Ohne das ist das Mesh offen und wirft keine sauberen Schatten. */
	bool bGenerateCaps = true;
};

namespace MolecularForge
{
	/**
	 * Zieht ein Querschnittsprofil entlang der Rueckgrat-Abschnitte und erzeugt daraus ein Mesh.
	 *
	 * Das Profil richtet sich nach der Sekundaerstruktur: ein Rundprofil fuer Schleifen, ein
	 * flaches Band fuer Helices, ein flaches Band mit Pfeilspitze fuer Faltblaetter. Diese
	 * Formensprache ist seit Jane Richardsons Zeichnungen der Standard — sie zu aendern hiesse,
	 * jedem Fachanwender das Bild fremd zu machen.
	 *
	 * Zwischen zwei Sekundaerstrukturen werden die Profilmasse ueber einige Punkte hinweg
	 * geglaettet. Ohne das saesse an jeder Grenze eine Stufe im Mesh, weil ein Rundprofil von
	 * 0,25 A unvermittelt auf ein Band von 1,8 A Breite traefe. Die Pfeilspitze wird erst
	 * danach aufgesetzt und bleibt deshalb scharf — ihr abrupter Ansatz ist gewollt.
	 *
	 * Ausgegeben wird ein geschlossenes, mannigfaltiges Mesh: jede Kante gehoert zu genau zwei
	 * Dreiecken. Das ist keine Formsache, sondern Voraussetzung dafuer, dass Schattenwurf und
	 * eine spaetere Umwandlung in ein Static Mesh funktionieren.
	 */
	MOLECULARFORGERENDER_API void BuildRibbonMesh(
		const UMolecularStructure& Structure,
		const TArray<FMolBackboneSegment>& Segments,
		const FMolRibbonOptions& Options,
		FMolMeshData& OutMesh);
}
