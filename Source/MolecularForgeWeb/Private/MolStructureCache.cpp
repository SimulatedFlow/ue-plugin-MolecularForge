// Copyright Simulated Flow. All Rights Reserved.

#include "MolStructureCache.h"
#include "MolecularForgeTypes.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace MolecularForge
{
	FString GetCacheDirectory()
	{
		return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("MolecularForge"), TEXT("StructureCache"));
	}

	FString GetCacheFilePath(EMolFetchSource Source, const FString& Identifier)
	{
		const FString Normalized = NormalizeIdentifier(Source, Identifier);

		// Ohne gueltige Kennung wird kein Pfad gebaut. Damit kann aus dem Cache-Verzeichnis
		// weder heraus- noch hineingeschrieben werden, was der Aufrufer nicht vorgesehen hat.
		if (!IsValidIdentifier(Source, Normalized))
		{
			return FString();
		}

		const FString FileName = FString::Printf(TEXT("%s-%s.%s"),
			*GetSourceSlug(Source), *Normalized, *GetCacheFileExtension(Source));

		return FPaths::Combine(GetCacheDirectory(), FileName);
	}

	namespace
	{
		bool IsFresh(const FString& FilePath, int32 MaxAgeDays)
		{
			if (MaxAgeDays <= 0)
			{
				return true;
			}

			const FDateTime Modified = IFileManager::Get().GetTimeStamp(*FilePath);
			if (Modified == FDateTime::MinValue())
			{
				return false;
			}

			return (FDateTime::UtcNow() - Modified).GetTotalDays() < static_cast<double>(MaxAgeDays);
		}
	}

	bool HasCachedStructure(EMolFetchSource Source, const FString& Identifier, int32 MaxAgeDays)
	{
		const FString Path = GetCacheFilePath(Source, Identifier);
		if (Path.IsEmpty() || !FPaths::FileExists(Path))
		{
			return false;
		}
		return IsFresh(Path, MaxAgeDays);
	}

	bool ReadCachedStructure(EMolFetchSource Source, const FString& Identifier, FString& OutContent, int32 MaxAgeDays)
	{
		const FString Path = GetCacheFilePath(Source, Identifier);
		if (Path.IsEmpty() || !FPaths::FileExists(Path) || !IsFresh(Path, MaxAgeDays))
		{
			return false;
		}

		if (!FFileHelper::LoadFileToString(OutContent, *Path))
		{
			UE_LOG(LogMolecularForge, Warning, TEXT("Cache-Datei nicht lesbar: %s"), *Path);
			return false;
		}

		// Eine leere Datei im Cache ist ein abgebrochener Download, kein gueltiger Eintrag.
		if (OutContent.IsEmpty())
		{
			IFileManager::Get().Delete(*Path);
			return false;
		}

		return true;
	}

	bool WriteCachedStructure(EMolFetchSource Source, const FString& Identifier, const FString& Content)
	{
		if (Content.IsEmpty())
		{
			return false;
		}

		const FString Path = GetCacheFilePath(Source, Identifier);
		if (Path.IsEmpty())
		{
			return false;
		}

		IFileManager::Get().MakeDirectory(*GetCacheDirectory(), /*Tree=*/true);

		if (!FFileHelper::SaveStringToFile(Content, *Path))
		{
			UE_LOG(LogMolecularForge, Warning, TEXT("Cache-Datei nicht schreibbar: %s"), *Path);
			return false;
		}

		return true;
	}

	bool RemoveCachedStructure(EMolFetchSource Source, const FString& Identifier)
	{
		const FString Path = GetCacheFilePath(Source, Identifier);
		if (Path.IsEmpty())
		{
			return false;
		}
		if (!FPaths::FileExists(Path))
		{
			return true;
		}
		return IFileManager::Get().Delete(*Path);
	}

	int32 ClearStructureCache()
	{
		const FString Directory = GetCacheDirectory();
		if (!FPaths::DirectoryExists(Directory))
		{
			return 0;
		}

		TArray<FString> Files;
		IFileManager::Get().FindFiles(Files, *FPaths::Combine(Directory, TEXT("*.*")), true, false);

		int32 Deleted = 0;
		for (const FString& File : Files)
		{
			if (IFileManager::Get().Delete(*FPaths::Combine(Directory, File)))
			{
				++Deleted;
			}
		}

		UE_LOG(LogMolecularForge, Log, TEXT("Strukturcache geleert: %d Dateien entfernt."), Deleted);
		return Deleted;
	}

	int64 GetStructureCacheSize()
	{
		const FString Directory = GetCacheDirectory();
		if (!FPaths::DirectoryExists(Directory))
		{
			return 0;
		}

		TArray<FString> Files;
		IFileManager::Get().FindFiles(Files, *FPaths::Combine(Directory, TEXT("*.*")), true, false);

		int64 Total = 0;
		for (const FString& File : Files)
		{
			const int64 Size = IFileManager::Get().FileSize(*FPaths::Combine(Directory, File));
			if (Size > 0)
			{
				Total += Size;
			}
		}
		return Total;
	}
}
