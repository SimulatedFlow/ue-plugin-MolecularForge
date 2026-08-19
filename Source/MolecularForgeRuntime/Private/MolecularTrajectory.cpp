// Copyright Simulated Flow. All Rights Reserved.

#include "MolecularTrajectory.h"

FVector UMolecularTrajectory::GetAtomPosition(int32 Frame, int32 Atom) const
{
	if (NumAtoms <= 0 || Atom < 0 || Atom >= NumAtoms)
	{
		return FVector::ZeroVector;
	}

	const int64 Index = static_cast<int64>(Frame) * NumAtoms + Atom;
	if (Index < 0 || Index >= Positions.Num())
	{
		return FVector::ZeroVector;
	}

	return FVector(Positions[static_cast<int32>(Index)]);
}

TArrayView<const FVector3f> UMolecularTrajectory::GetFrame(int32 Frame) const
{
	if (NumAtoms <= 0 || Frame < 0 || Frame >= GetNumFrames())
	{
		return TArrayView<const FVector3f>();
	}

	return TArrayView<const FVector3f>(Positions.GetData() + static_cast<int64>(Frame) * NumAtoms, NumAtoms);
}

FString UMolecularTrajectory::GetSummary() const
{
	return FString::Printf(TEXT("%d Bilder, %d Atome%s"),
		GetNumFrames(), NumAtoms,
		TimeStepPicoseconds > 0.f
			? *FString::Printf(TEXT(", %.3f ps je Bild (%.1f ps gesamt)"),
				TimeStepPicoseconds, GetDurationPicoseconds())
			: TEXT(""));
}

void UMolecularTrajectory::Reset()
{
	NumAtoms = 0;
	Positions.Reset();
	UnitCellSizes.Reset();
	TimeStepPicoseconds = 0.f;
	SourceFile.Empty();
}

void UMolecularTrajectory::SampleInto(float FrameTime, TArray<FVector3f>& OutPositions) const
{
	const int32 NumFrames = GetNumFrames();
	if (NumFrames == 0 || NumAtoms == 0)
	{
		OutPositions.Reset();
		return;
	}

	OutPositions.SetNumUninitialized(NumAtoms);

	const float Clamped = FMath::Clamp(FrameTime, 0.f, static_cast<float>(NumFrames - 1));
	const int32 FrameA = FMath::FloorToInt(Clamped);
	const int32 FrameB = FMath::Min(FrameA + 1, NumFrames - 1);
	const float Alpha = Clamped - FrameA;

	const TArrayView<const FVector3f> A = GetFrame(FrameA);

	// Genau auf einem Bild oder am Ende: nichts zu mischen, nur kopieren.
	if (FrameA == FrameB || Alpha <= UE_SMALL_NUMBER)
	{
		FMemory::Memcpy(OutPositions.GetData(), A.GetData(), NumAtoms * sizeof(FVector3f));
		return;
	}

	const TArrayView<const FVector3f> B = GetFrame(FrameB);

	for (int32 i = 0; i < NumAtoms; ++i)
	{
		OutPositions[i] = FMath::Lerp(A[i], B[i], Alpha);
	}
}
