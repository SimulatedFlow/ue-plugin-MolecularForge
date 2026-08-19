// Copyright Simulated Flow. All Rights Reserved.

#include "MolecularForgeLibrary.h"
#include "MolecularStructure.h"
#include "MolStructureIO.h"

namespace
{
	UMolecularStructure* FinishLoad(UMolecularStructure* Structure,
		const FMolParseResult& Result, FString& OutErrorMessage)
	{
		if (!Result.bSuccess)
		{
			OutErrorMessage = Result.Error;
			UE_LOG(LogMolecularForge, Warning, TEXT("Laden fehlgeschlagen: %s"), *Result.Error);
			return nullptr;
		}

		OutErrorMessage.Empty();
		return Structure;
	}
}

UMolecularStructure* UMolecularForgeLibrary::LoadStructureFromFile(
	UObject* Outer, const FString& FilePath, FMolLoadOptions Options, FString& OutErrorMessage)
{
	UObject* Owner = Outer ? Outer : GetTransientPackage();
	UMolecularStructure* Structure = NewObject<UMolecularStructure>(Owner);

	const FMolParseResult Result = MolecularForge::ParseStructureFile(FilePath, Options, *Structure);
	return FinishLoad(Structure, Result, OutErrorMessage);
}

UMolecularStructure* UMolecularForgeLibrary::LoadStructureFromText(
	UObject* Outer, const FString& Content, FMolLoadOptions Options, FString& OutErrorMessage)
{
	UObject* Owner = Outer ? Outer : GetTransientPackage();
	UMolecularStructure* Structure = NewObject<UMolecularStructure>(Owner);

	const FMolParseResult Result = MolecularForge::ParseStructureText(Content, Options, *Structure);
	return FinishLoad(Structure, Result, OutErrorMessage);
}

FString UMolecularForgeLibrary::GetRequiredAttribution(const UMolecularStructure* Structure)
{
	return Structure ? Structure->Meta.Attribution : FString();
}
