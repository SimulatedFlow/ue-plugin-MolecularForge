// Copyright Silvan Teufel. All Rights Reserved.

#include "MolStructureIO.h"
#include "MolecularStructure.h"
#include "MolPdbParser.h"
#include "MolCifParser.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
	/**
	 * Sucht in den ersten Zeilen nach einem eindeutigen Kennzeichen.
	 * Nur der Anfang wird geprueft — eine mmCIF-Datei kann irgendwo im Text das Wort
	 * "ATOM" enthalten, aber ihr Kopf faengt immer mit `data_` an.
	 */
	constexpr int32 GSniffLength = 4096;
}

namespace MolecularForge
{
	EMolFileFormat DetectFormatFromContent(FStringView Content)
	{
		const int32 Length = FMath::Min(Content.Len(), GSniffLength);
		const FString Head(Content.Mid(0, Length));

		// mmCIF-Kennzeichen sind eindeutig und koennen im PDB-Format nicht vorkommen.
		if (Head.Contains(TEXT("_atom_site.")) || Head.Contains(TEXT("loop_")))
		{
			return EMolFileFormat::MmCif;
		}
		if (Head.StartsWith(TEXT("data_")))
		{
			return EMolFileFormat::MmCif;
		}

		if (Head.Contains(TEXT("\nATOM  ")) || Head.StartsWith(TEXT("ATOM  "))
			|| Head.Contains(TEXT("\nHETATM")) || Head.StartsWith(TEXT("HETATM"))
			|| Head.StartsWith(TEXT("HEADER")))
		{
			return EMolFileFormat::Pdb;
		}

		return EMolFileFormat::Unknown;
	}

	EMolFileFormat DetectFormat(const FString& FilePath, FStringView Content)
	{
		const FString Extension = FPaths::GetExtension(FilePath).ToLower();

		if (Extension == TEXT("cif") || Extension == TEXT("mmcif") || Extension == TEXT("bcif"))
		{
			return EMolFileFormat::MmCif;
		}
		if (Extension == TEXT("pdb") || Extension == TEXT("ent"))
		{
			return EMolFileFormat::Pdb;
		}

		return DetectFormatFromContent(Content);
	}

	FMolParseResult ParseStructureText(FStringView Content, const FMolLoadOptions& Options, UMolecularStructure& OutStructure)
	{
		switch (DetectFormatFromContent(Content))
		{
		case EMolFileFormat::MmCif:
			return ParseCif(Content, Options, OutStructure);

		case EMolFileFormat::Pdb:
			return ParsePdb(Content, Options, OutStructure);

		default:
		{
			FMolParseResult Result;
			Result.Error = TEXT("Format nicht erkannt. Erwartet wird PDB oder mmCIF.");
			return Result;
		}
		}
	}

	FMolParseResult ParseStructureFile(const FString& FilePath, const FMolLoadOptions& Options, UMolecularStructure& OutStructure)
	{
		FMolParseResult Result;

		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *FilePath))
		{
			Result.Error = FString::Printf(TEXT("Datei nicht lesbar: %s"), *FilePath);
			UE_LOG(LogMolecularForge, Warning, TEXT("%s"), *Result.Error);
			return Result;
		}

		switch (DetectFormat(FilePath, Text))
		{
		case EMolFileFormat::MmCif:
			Result = ParseCif(Text, Options, OutStructure);
			break;

		case EMolFileFormat::Pdb:
			Result = ParsePdb(Text, Options, OutStructure);
			break;

		default:
			Result.Error = FString::Printf(
				TEXT("Format von '%s' nicht erkannt. Erwartet wird PDB oder mmCIF."),
				*FPaths::GetCleanFilename(FilePath));
			UE_LOG(LogMolecularForge, Warning, TEXT("%s"), *Result.Error);
			return Result;
		}

		if (Result.bSuccess && OutStructure.Meta.Identifier.IsEmpty())
		{
			OutStructure.Meta.Identifier = FPaths::GetBaseFilename(FilePath);
		}
		if (Result.bSuccess && OutStructure.Meta.Source == EMolStructureSource::Unknown)
		{
			OutStructure.Meta.Source = EMolStructureSource::LocalFile;
		}

		return Result;
	}
}
