// Copyright 2026 Simulated Flow All Rights Reserved.

#include "MolFetchStructureAction.h"
#include "MolecularStructure.h"

UMolFetchStructureAction* UMolFetchStructureAction::FetchStructure(
	UObject* WorldContextObject,
	EMolFetchSource Source,
	const FString& Identifier,
	FMolLoadOptions LoadOptions,
	bool bUseCache)
{
	UMolFetchStructureAction* Action = NewObject<UMolFetchStructureAction>();
	Action->ContextObject = WorldContextObject;
	Action->Options.Source = Source;
	Action->Options.Identifier = Identifier;
	Action->Options.LoadOptions = LoadOptions;
	Action->Options.bUseCache = bUseCache;
	return Action;
}

void UMolFetchStructureAction::Activate()
{
	// Der Knoten haengt an keinem Graphen mehr, sobald Activate zurueckkehrt. Bis der
	// Abruf antwortet, muss er sich selbst halten.
	AddToRoot();

	FOnMolStructureFetched Callback;
	Callback.BindUObject(this, &UMolFetchStructureAction::HandleCompleted);

	MolecularForge::FetchStructure(ContextObject, Options, Callback);
}

void UMolFetchStructureAction::HandleCompleted(UMolecularStructure* Structure, const FMolFetchResult& Result)
{
	if (Result.bSuccess && Structure)
	{
		OnSuccess.Broadcast(Structure, Result);
	}
	else
	{
		OnFailure.Broadcast(nullptr, Result);
	}

	RemoveFromRoot();
	SetReadyToDestroy();
}
