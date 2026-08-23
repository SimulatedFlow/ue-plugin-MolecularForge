// Copyright 2026 Silvan Teufel All Rights Reserved.

#include "MolecularTrajectoryPlayer.h"
#include "MolecularAtomsComponent.h"
#include "MolecularBondsComponent.h"
#include "MolecularCartoonComponent.h"
#include "MolecularSurfaceComponent.h"
#include "MolecularStructure.h"
#include "MolecularTrajectory.h"
#include "GameFramework/Actor.h"

UMolecularTrajectoryPlayer::UMolecularTrajectoryPlayer()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UMolecularTrajectoryPlayer::BeginPlay()
{
	Super::BeginPlay();

	if (bPlayOnBeginPlay)
	{
		Play();
	}
}

bool UMolecularTrajectoryPlayer::IsTrajectoryCompatible(FString& OutReason) const
{
	if (!Structure || Structure->IsEmpty())
	{
		OutReason = TEXT("Es ist keine Struktur gesetzt.");
		return false;
	}
	if (!Trajectory || Trajectory->IsEmpty())
	{
		OutReason = TEXT("Es ist keine Trajektorie gesetzt.");
		return false;
	}

	if (Trajectory->NumAtoms != Structure->GetNumAtoms())
	{
		OutReason = FString::Printf(
			TEXT("Die Trajektorie hat %d Atome, die Struktur %d. Sie gehoeren nicht zusammen — "
				 "oder die Struktur wurde mit anderen Ladeoptionen gelesen als die Trajektorie erwartet, "
				 "etwa mit verworfenem Wasser."),
			Trajectory->NumAtoms, Structure->GetNumAtoms());
		return false;
	}

	OutReason.Empty();
	return true;
}

void UMolecularTrajectoryPlayer::Play()
{
	FString Reason;
	if (!IsTrajectoryCompatible(Reason))
	{
		UE_LOG(LogMolecularForge, Warning, TEXT("Trajektorie nicht abspielbar: %s"), *Reason);
		return;
	}

	bPlaying = true;
	SetComponentTickEnabled(true);
}

void UMolecularTrajectoryPlayer::Pause()
{
	bPlaying = false;
	SetComponentTickEnabled(false);
}

void UMolecularTrajectoryPlayer::Stop()
{
	Pause();
	SetFrameTime(0.f);
}

void UMolecularTrajectoryPlayer::SetFrameTime(float NewFrameTime)
{
	if (!Trajectory || Trajectory->IsEmpty())
	{
		return;
	}

	FrameTime = FMath::Clamp(NewFrameTime, 0.f, static_cast<float>(Trajectory->GetNumFrames() - 1));
	ApplyCurrentFrame();
}

void UMolecularTrajectoryPlayer::SetNormalizedTime(float Normalized)
{
	if (!Trajectory || Trajectory->IsEmpty())
	{
		return;
	}

	SetFrameTime(FMath::Clamp(Normalized, 0.f, 1.f) * (Trajectory->GetNumFrames() - 1));
}

float UMolecularTrajectoryPlayer::GetNormalizedTime() const
{
	if (!Trajectory || Trajectory->GetNumFrames() < 2)
	{
		return 0.f;
	}
	return FrameTime / (Trajectory->GetNumFrames() - 1);
}

void UMolecularTrajectoryPlayer::RestoreOriginalPositions()
{
	if (!Structure || OriginalPositions.Num() != Structure->GetNumAtoms())
	{
		return;
	}

	Structure->AtomPositions = OriginalPositions;
	Structure->FinalizeAfterLoad();
	RefreshRepresentations();
}

void UMolecularTrajectoryPlayer::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bPlaying || !Trajectory || Trajectory->GetNumFrames() < 2)
	{
		return;
	}

	const float LastFrame = static_cast<float>(Trajectory->GetNumFrames() - 1);
	FrameTime += DeltaTime * FramesPerSecond;

	if (FrameTime >= LastFrame)
	{
		if (bLoop)
		{
			// Modulo statt Zuruecksetzen, damit bei hohem Tempo keine Bilder verschluckt
			// werden und der Uebergang nicht stockt.
			FrameTime = FMath::Fmod(FrameTime, LastFrame);
		}
		else
		{
			FrameTime = LastFrame;
			bPlaying = false;
			SetComponentTickEnabled(false);
			ApplyCurrentFrame();
			OnFinished.Broadcast();
			return;
		}
	}

	ApplyCurrentFrame();
}

void UMolecularTrajectoryPlayer::ApplyCurrentFrame()
{
	if (!Structure || !Trajectory || Trajectory->IsEmpty())
	{
		return;
	}
	if (Trajectory->NumAtoms != Structure->GetNumAtoms())
	{
		return;
	}

	// Die Ausgangskoordinaten beiseitelegen, bevor zum ersten Mal ueberschrieben wird.
	// Bewusst hier und nicht in Play(): auch wer nur an einem Schieberegler zieht, ohne
	// je auf Abspielen zu druecken, veraendert die Struktur — und soll sie zurueckbekommen.
	if (OriginalPositions.Num() != Structure->GetNumAtoms())
	{
		OriginalPositions = Structure->AtomPositions;
	}

	if (bInterpolate)
	{
		Trajectory->SampleInto(FrameTime, SampleBuffer);
		if (SampleBuffer.Num() == Structure->GetNumAtoms())
		{
			FMemory::Memcpy(Structure->AtomPositions.GetData(), SampleBuffer.GetData(),
				SampleBuffer.Num() * sizeof(FVector3f));
		}
	}
	else
	{
		const TArrayView<const FVector3f> Frame = Trajectory->GetFrame(FMath::RoundToInt(FrameTime));
		if (Frame.Num() == Structure->GetNumAtoms())
		{
			FMemory::Memcpy(Structure->AtomPositions.GetData(), Frame.GetData(),
				Frame.Num() * sizeof(FVector3f));
		}
	}

	// Die Huelle muss mitwandern, sonst laufen Kamerafahrten und Culling ins Leere.
	// Die Bindungsliste bleibt dagegen, wie sie ist: welche Atome verbunden sind, aendert
	// sich in einer klassischen Molekuedynamik nicht — dort werden Bindungen nicht
	// gebrochen, sondern nur gedehnt.
	Structure->FinalizeAfterLoad();

	RefreshRepresentations();
	OnFrameApplied.Broadcast(FrameTime);
}

void UMolecularTrajectoryPlayer::RefreshRepresentations()
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	// Die guenstigen Darstellungen laufen immer mit: dort werden nur Instanztransformationen
	// neu geschrieben, was auch bei hunderttausend Atomen in ein Bild passt.
	Owner->ForEachComponent<UMolecularAtomsComponent>(false, [](UMolecularAtomsComponent* Atoms)
	{
		Atoms->RefreshTransformsFromStructure();
	});

	Owner->ForEachComponent<UMolecularBondsComponent>(false, [](UMolecularBondsComponent* Bonds)
	{
		if (Bonds->IsVisible())
		{
			Bonds->RefreshTransformsFromStructure();
		}
	});

	if (!bUpdateExpensiveRepresentations)
	{
		return;
	}

	Owner->ForEachComponent<UMolecularCartoonComponent>(false, [](UMolecularCartoonComponent* Cartoon)
	{
		if (Cartoon->IsVisible())
		{
			Cartoon->RebuildMesh();
		}
	});

	Owner->ForEachComponent<UMolecularSurfaceComponent>(false, [](UMolecularSurfaceComponent* Surface)
	{
		if (Surface->IsVisible())
		{
			Surface->RebuildMesh();
		}
	});
}
