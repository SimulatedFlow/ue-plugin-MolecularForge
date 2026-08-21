// Copyright 2026 Simulated Flow All Rights Reserved.

#include "MolMeasurement.h"
#include "MolecularStructure.h"
#include "MolElementTable.h"

namespace
{
	bool AreValid(const UMolecularStructure& Structure, std::initializer_list<int32> Indices)
	{
		for (int32 Index : Indices)
		{
			if (!Structure.AtomPositions.IsValidIndex(Index))
			{
				return false;
			}
		}
		return true;
	}
}

namespace MolecularForge
{
	float MeasureDistance(const UMolecularStructure& Structure, int32 AtomA, int32 AtomB)
	{
		if (!AreValid(Structure, { AtomA, AtomB }))
		{
			return -1.f;
		}
		return FVector3f::Dist(Structure.AtomPositions[AtomA], Structure.AtomPositions[AtomB]);
	}

	float MeasureAngle(const UMolecularStructure& Structure, int32 AtomA, int32 AtomB, int32 AtomC)
	{
		if (!AreValid(Structure, { AtomA, AtomB, AtomC }))
		{
			return -1.f;
		}

		const FVector3f BA = (Structure.AtomPositions[AtomA] - Structure.AtomPositions[AtomB]).GetSafeNormal();
		const FVector3f BC = (Structure.AtomPositions[AtomC] - Structure.AtomPositions[AtomB]).GetSafeNormal();

		if (BA.IsNearlyZero() || BC.IsNearlyZero())
		{
			return -1.f;
		}

		const float Cosine = FMath::Clamp(FVector3f::DotProduct(BA, BC), -1.f, 1.f);
		return FMath::RadiansToDegrees(FMath::Acos(Cosine));
	}

	float MeasureDihedral(const UMolecularStructure& Structure,
		int32 AtomA, int32 AtomB, int32 AtomC, int32 AtomD)
	{
		if (!AreValid(Structure, { AtomA, AtomB, AtomC, AtomD }))
		{
			return 0.f;
		}

		const FVector3f P0 = Structure.AtomPositions[AtomA];
		const FVector3f P1 = Structure.AtomPositions[AtomB];
		const FVector3f P2 = Structure.AtomPositions[AtomC];
		const FVector3f P3 = Structure.AtomPositions[AtomD];

		const FVector3f B1 = P1 - P0;
		const FVector3f B2 = P2 - P1;
		const FVector3f B3 = P3 - P2;

		// Normalen der beiden Ebenen, die von je drei Punkten aufgespannt werden.
		const FVector3f N1 = FVector3f::CrossProduct(B1, B2);
		const FVector3f N2 = FVector3f::CrossProduct(B2, B3);

		if (N1.IsNearlyZero() || N2.IsNearlyZero())
		{
			// Alle vier Punkte auf einer Geraden — dann gibt es keinen Torsionswinkel.
			return 0.f;
		}

		// Der Umweg ueber atan2 statt ueber acos des Skalarprodukts ist noetig, weil
		// acos das Vorzeichen verliert. Bei Torsionen ist es aber die halbe Aussage:
		// phi = -57 Grad ist eine rechtsgaengige Helix, phi = +57 eine linksgaengige.
		const FVector3f M = FVector3f::CrossProduct(N1, B2.GetSafeNormal());

		const float X = FVector3f::DotProduct(N1, N2);
		const float Y = FVector3f::DotProduct(M, N2);

		// Das Minus gehoert zur IUPAC-Konvention und ist keine Geschmacksfrage: ohne es
		// kaeme eine rechtsgaengige Alpha-Helix mit phi = +57 heraus, und damit stuende
		// jede Angabe im Widerspruch zur gesamten Fachliteratur. Der Vorzeichenfehler
		// war zuerst drin und fiel erst auf, als eine gebaute Helix zurueckgemessen wurde.
		return -FMath::RadiansToDegrees(FMath::Atan2(Y, X));
	}

	FVector ComputeCentroid(const UMolecularStructure& Structure, const TBitArray<>& Mask)
	{
		FVector3f Sum = FVector3f::ZeroVector;
		int32 Count = 0;

		const int32 Num = FMath::Min(Mask.Num(), Structure.GetNumAtoms());
		for (int32 a = 0; a < Num; ++a)
		{
			if (Mask[a])
			{
				Sum += Structure.AtomPositions[a];
				++Count;
			}
		}

		return Count > 0 ? FVector(Sum / static_cast<float>(Count)) : FVector::ZeroVector;
	}

	float ComputeTotalMass(const UMolecularStructure& Structure, const TBitArray<>& Mask)
	{
		float Total = 0.f;

		const int32 Num = FMath::Min(Mask.Num(), Structure.GetNumAtoms());
		for (int32 a = 0; a < Num; ++a)
		{
			if (Mask[a])
			{
				Total += GetAtomicMass(Structure.AtomElements[a]);
			}
		}

		return Total;
	}

	FVector ComputeCenterOfMass(const UMolecularStructure& Structure, const TBitArray<>& Mask)
	{
		FVector3f WeightedSum = FVector3f::ZeroVector;
		float TotalMass = 0.f;

		const int32 Num = FMath::Min(Mask.Num(), Structure.GetNumAtoms());
		for (int32 a = 0; a < Num; ++a)
		{
			if (!Mask[a])
			{
				continue;
			}

			const float Mass = GetAtomicMass(Structure.AtomElements[a]);
			WeightedSum += Structure.AtomPositions[a] * Mass;
			TotalMass += Mass;
		}

		// Ohne bekannte Massen — etwa bei durchgehend unerkannten Elementen — waere die
		// Rechnung sinnlos. Dann ist der geometrische Mittelpunkt die ehrlichere Antwort.
		if (TotalMass <= UE_SMALL_NUMBER)
		{
			return ComputeCentroid(Structure, Mask);
		}

		return FVector(WeightedSum / TotalMass);
	}

	float ComputeRadiusOfGyration(const UMolecularStructure& Structure, const TBitArray<>& Mask)
	{
		const FVector3f Center = FVector3f(ComputeCenterOfMass(Structure, Mask));

		float WeightedSquareSum = 0.f;
		float TotalMass = 0.f;

		const int32 Num = FMath::Min(Mask.Num(), Structure.GetNumAtoms());
		for (int32 a = 0; a < Num; ++a)
		{
			if (!Mask[a])
			{
				continue;
			}

			const float Mass = FMath::Max(GetAtomicMass(Structure.AtomElements[a]), UE_KINDA_SMALL_NUMBER);
			WeightedSquareSum += Mass * FVector3f::DistSquared(Structure.AtomPositions[a], Center);
			TotalMass += Mass;
		}

		return TotalMass > UE_SMALL_NUMBER ? FMath::Sqrt(WeightedSquareSum / TotalMass) : 0.f;
	}

	FBox ComputeSelectionBounds(const UMolecularStructure& Structure, const TBitArray<>& Mask)
	{
		FBox Bounds(ForceInit);

		const int32 Num = FMath::Min(Mask.Num(), Structure.GetNumAtoms());
		for (int32 a = 0; a < Num; ++a)
		{
			if (Mask[a])
			{
				Bounds += FVector(Structure.AtomPositions[a]);
			}
		}

		return Bounds;
	}

	bool ComputeRmsd(TArrayView<const FVector3f> A, TArrayView<const FVector3f> B, float& OutRmsd)
	{
		if (A.Num() == 0 || A.Num() != B.Num())
		{
			OutRmsd = 0.f;
			return false;
		}

		double SquareSum = 0.0;
		for (int32 i = 0; i < A.Num(); ++i)
		{
			SquareSum += FVector3f::DistSquared(A[i], B[i]);
		}

		OutRmsd = static_cast<float>(FMath::Sqrt(SquareSum / A.Num()));
		return true;
	}

	bool ComputeRmsdMasked(TArrayView<const FVector3f> A, TArrayView<const FVector3f> B,
		const TBitArray<>& Mask, float& OutRmsd)
	{
		if (A.Num() == 0 || A.Num() != B.Num())
		{
			OutRmsd = 0.f;
			return false;
		}

		double SquareSum = 0.0;
		int32 Count = 0;

		const int32 Num = FMath::Min3(A.Num(), B.Num(), Mask.Num());
		for (int32 i = 0; i < Num; ++i)
		{
			if (Mask[i])
			{
				SquareSum += FVector3f::DistSquared(A[i], B[i]);
				++Count;
			}
		}

		if (Count == 0)
		{
			OutRmsd = 0.f;
			return false;
		}

		OutRmsd = static_cast<float>(FMath::Sqrt(SquareSum / Count));
		return true;
	}
}
