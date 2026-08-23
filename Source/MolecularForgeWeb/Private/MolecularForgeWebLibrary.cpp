// Copyright 2026 Silvan Teufel All Rights Reserved.

#include "MolecularForgeWebLibrary.h"
#include "MolStructureCache.h"

bool UMolecularForgeWebLibrary::IsValidStructureIdentifier(EMolFetchSource Source, const FString& Identifier)
{
	return MolecularForge::IsValidIdentifier(Source, MolecularForge::NormalizeIdentifier(Source, Identifier));
}

FString UMolecularForgeWebLibrary::GetStructureRequestUrl(EMolFetchSource Source, const FString& Identifier)
{
	return MolecularForge::BuildRequestUrl(Source, Identifier);
}

bool UMolecularForgeWebLibrary::IsStructureCached(EMolFetchSource Source, const FString& Identifier)
{
	return MolecularForge::HasCachedStructure(Source, Identifier);
}

FString UMolecularForgeWebLibrary::GetStructureCacheDirectory()
{
	return MolecularForge::GetCacheDirectory();
}

float UMolecularForgeWebLibrary::GetStructureCacheSizeMB()
{
	return static_cast<float>(MolecularForge::GetStructureCacheSize()) / (1024.f * 1024.f);
}

int32 UMolecularForgeWebLibrary::ClearStructureCache()
{
	return MolecularForge::ClearStructureCache();
}
