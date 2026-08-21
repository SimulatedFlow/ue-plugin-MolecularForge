// Copyright 2026 Simulated Flow All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MolecularForgeTypes.h"
#include "MolPdbParser.h"

class UMolecularStructure;

/** Erkanntes Dateiformat. */
enum class EMolFileFormat : uint8
{
	Unknown,
	Pdb,
	MmCif
};

namespace MolecularForge
{
	/**
	 * Bestimmt das Format aus Dateiendung und, wenn die nichts hergibt, aus dem Inhalt.
	 * Die Inhaltspruefung ist kein Luxus: Downloads landen regelmaessig als `.txt` oder
	 * ganz ohne Endung auf der Platte, und der Anwender soll sie trotzdem laden koennen.
	 */
	MOLECULARFORGERUNTIME_API EMolFileFormat DetectFormat(const FString& FilePath, FStringView Content);

	/** Wie DetectFormat, aber nur anhand des Inhalts. */
	MOLECULARFORGERUNTIME_API EMolFileFormat DetectFormatFromContent(FStringView Content);

	/** Laedt eine Struktur und waehlt den passenden Leser selbst. */
	MOLECULARFORGERUNTIME_API FMolParseResult ParseStructureFile(
		const FString& FilePath,
		const FMolLoadOptions& Options,
		UMolecularStructure& OutStructure);

	/** Wie ParseStructureFile, aber der Inhalt liegt bereits als Text vor. */
	MOLECULARFORGERUNTIME_API FMolParseResult ParseStructureText(
		FStringView Content,
		const FMolLoadOptions& Options,
		UMolecularStructure& OutStructure);
}
