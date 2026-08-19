// Copyright Simulated Flow. All Rights Reserved.

#include "MolecularNiagaraLibrary.h"
#include "MolecularStructure.h"
#include "NiagaraComponent.h"
#include "NiagaraDataInterfaceArrayFunctionLibrary.h"

int32 UMolecularNiagaraLibrary::SetStructureArrays(
	UNiagaraComponent* NiagaraComponent,
	const UMolecularStructure* Structure,
	FMolNiagaraOptions Options)
{
	using namespace MolecularForge;

	if (!NiagaraComponent)
	{
		UE_LOG(LogMolecularForge, Warning, TEXT("Struktur an Niagara: keine Komponente angegeben."));
		return 0;
	}
	if (!Structure || Structure->IsEmpty())
	{
		UE_LOG(LogMolecularForge, Warning, TEXT("Struktur an Niagara: keine Struktur angegeben."));
		return 0;
	}

	FMolNiagaraArrays Arrays;
	BuildNiagaraArrays(*Structure, Options, Arrays);

	if (Arrays.IsEmpty())
	{
		UE_LOG(LogMolecularForge, Warning,
			TEXT("Struktur an Niagara: nach den Filtern bleibt kein Atom uebrig."));
		return 0;
	}

	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector(
		NiagaraComponent, NiagaraParameterNames::AtomPositions, Arrays.Positions);
	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayColor(
		NiagaraComponent, NiagaraParameterNames::AtomColors, Arrays.Colors);
	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayFloat(
		NiagaraComponent, NiagaraParameterNames::AtomRadii, Arrays.Radii);

	// Die Anzahl zusaetzlich als einfacher Parameter: im Emitter laesst sich damit die
	// Spawnrate setzen, ohne erst die Arraylaenge abfragen zu muessen.
	NiagaraComponent->SetVariableInt(NiagaraParameterNames::AtomCount, Arrays.Num());

	if (Options.bIncludeBonds)
	{
		UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector(
			NiagaraComponent, NiagaraParameterNames::BondStarts, Arrays.BondStarts);
		UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector(
			NiagaraComponent, NiagaraParameterNames::BondEnds, Arrays.BondEnds);
		NiagaraComponent->SetVariableInt(NiagaraParameterNames::BondCount, Arrays.BondStarts.Num());
	}

	if (Arrays.Num() < Arrays.NumAtomsBeforeLimit)
	{
		UE_LOG(LogMolecularForge, Log,
			TEXT("Struktur an Niagara: auf %d von %d Atomen ausgeduennt."),
			Arrays.Num(), Arrays.NumAtomsBeforeLimit);
	}

	return Arrays.Num();
}

int32 UMolecularNiagaraLibrary::CountNiagaraParticles(
	const UMolecularStructure* Structure, FMolNiagaraOptions Options)
{
	if (!Structure || Structure->IsEmpty())
	{
		return 0;
	}

	FMolNiagaraArrays Arrays;
	MolecularForge::BuildNiagaraArrays(*Structure, Options, Arrays);
	return Arrays.Num();
}

void UMolecularNiagaraLibrary::GetNiagaraParameterNames(
	FName& OutAtomPositions, FName& OutAtomColors, FName& OutAtomRadii, FName& OutAtomCount)
{
	using namespace MolecularForge;

	OutAtomPositions = NiagaraParameterNames::AtomPositions;
	OutAtomColors = NiagaraParameterNames::AtomColors;
	OutAtomRadii = NiagaraParameterNames::AtomRadii;
	OutAtomCount = NiagaraParameterNames::AtomCount;
}
