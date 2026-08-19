// Copyright Simulated Flow. All Rights Reserved.

#include "MolBackboneSpline.h"
#include "MolecularStructure.h"

namespace
{
	/** Ein Ankerpunkt vor der Interpolation: gemessene Lage plus abgeleitete Querrichtung. */
	struct FMolGuidePoint
	{
		FVector3f Position = FVector3f::ZeroVector;
		FVector3f Right = FVector3f::ZeroVector;
		bool bHasCarbonyl = false;
		EMolSecondaryStructure SecondaryStructure = EMolSecondaryStructure::Coil;
		int32 ResidueIndex = INDEX_NONE;
	};

	FVector3f CatmullRom(const FVector3f& P0, const FVector3f& P1,
		const FVector3f& P2, const FVector3f& P3, float T)
	{
		const float T2 = T * T;
		const float T3 = T2 * T;

		return 0.5f * (
			(2.f * P1)
			+ (-P0 + P2) * T
			+ (2.f * P0 - 5.f * P1 + 4.f * P2 - P3) * T2
			+ (-P0 + 3.f * P1 - 3.f * P2 + P3) * T3);
	}

	FVector3f CatmullRomTangent(const FVector3f& P0, const FVector3f& P1,
		const FVector3f& P2, const FVector3f& P3, float T)
	{
		const float T2 = T * T;

		return 0.5f * (
			(-P0 + P2)
			+ 2.f * (2.f * P0 - 5.f * P1 + 4.f * P2 - P3) * T
			+ 3.f * (-P0 + 3.f * P1 - 3.f * P2 + P3) * T2);
	}

	/** Irgendein Vektor senkrecht zu Direction, moeglichst nah an Hint. */
	FVector3f MakePerpendicular(const FVector3f& Direction, const FVector3f& Hint)
	{
		FVector3f Candidate = Hint - Direction * FVector3f::DotProduct(Hint, Direction);

		if (Candidate.SizeSquared() < KINDA_SMALL_NUMBER)
		{
			// Hint lag parallel zur Richtung. Dann tut es eine Achse, die es nicht ist.
			const FVector3f Fallback = FMath::Abs(Direction.Z) < 0.9f
				? FVector3f::ZAxisVector
				: FVector3f::XAxisVector;
			Candidate = Fallback - Direction * FVector3f::DotProduct(Fallback, Direction);
		}

		return Candidate.GetSafeNormal();
	}

	/** Sammelt die Ankerpunkte einer Kette und schneidet sie an Luecken auseinander. */
	void CollectGuideSegments(const UMolecularStructure& Structure, int32 ChainIndex,
		float MaxGap, TArray<TArray<FMolGuidePoint>>& OutGuideSegments)
	{
		static const FName NameC(TEXT("C"));
		static const FName NameO(TEXT("O"));

		const FMolChain& Chain = Structure.Chains[ChainIndex];
		const float MaxGapSq = MaxGap * MaxGap;

		TArray<FMolGuidePoint> Current;

		const int32 LastResidue = Chain.FirstResidue + Chain.NumResidues;
		for (int32 r = Chain.FirstResidue; r < LastResidue && r < Structure.GetNumResidues(); ++r)
		{
			const FMolResidue& Residue = Structure.Residues[r];

			int32 AnchorAtom = INDEX_NONE;
			int32 CarbonylC = INDEX_NONE;
			int32 CarbonylO = INDEX_NONE;

			const int32 AtomEnd = Residue.FirstAtom + Residue.NumAtoms;
			for (int32 a = Residue.FirstAtom; a < AtomEnd && a < Structure.GetNumAtoms(); ++a)
			{
				if ((Structure.AtomFlags[a] & MolAtom_Anchor) != 0 && AnchorAtom == INDEX_NONE)
				{
					AnchorAtom = a;
				}
				if (Structure.AtomNames[a] == NameC) { CarbonylC = a; }
				else if (Structure.AtomNames[a] == NameO) { CarbonylO = a; }
			}

			if (AnchorAtom == INDEX_NONE)
			{
				// Kein Ankeratom — etwa ein Ligand mitten in der Kette. Der gehoert
				// nicht auf das Rueckgrat, unterbricht es aber auch nicht.
				continue;
			}

			FMolGuidePoint Guide;
			Guide.Position = Structure.AtomPositions[AnchorAtom];
			Guide.SecondaryStructure = Residue.SecondaryStructure;
			Guide.ResidueIndex = r;

			if (CarbonylC != INDEX_NONE && CarbonylO != INDEX_NONE)
			{
				// Vorlaeufig die Carbonylrichtung selbst ablegen; die endgueltige
				// Querrichtung braucht zusaetzlich die Laufrichtung und entsteht erst,
				// wenn der Nachfolger bekannt ist.
				Guide.Right = Structure.AtomPositions[CarbonylO] - Structure.AtomPositions[CarbonylC];
				Guide.bHasCarbonyl = !Guide.Right.IsNearlyZero();
			}

			// Luecke? Dann hier trennen statt darueber hinwegzuraten.
			if (!Current.IsEmpty() &&
				FVector3f::DistSquared(Current.Last().Position, Guide.Position) > MaxGapSq)
			{
				if (Current.Num() >= 2)
				{
					OutGuideSegments.Add(MoveTemp(Current));
				}
				Current.Reset();
			}

			Current.Add(Guide);
		}

		if (Current.Num() >= 2)
		{
			OutGuideSegments.Add(MoveTemp(Current));
		}
	}

	/**
	 * Rechnet die abgelegten Carbonylrichtungen in endgueltige Querrichtungen um und
	 * dreht dabei jede um, die gegen ihre Vorgaengerin zeigt.
	 */
	void ResolveGuideFrames(TArray<FMolGuidePoint>& Guides)
	{
		const int32 Num = Guides.Num();

		FVector3f PreviousRight = FVector3f::ZeroVector;

		for (int32 i = 0; i < Num; ++i)
		{
			// Laufrichtung: zum Nachfolger, am Ende vom Vorgaenger her.
			const FVector3f Forward = (i + 1 < Num)
				? (Guides[i + 1].Position - Guides[i].Position)
				: (Guides[i].Position - Guides[i - 1].Position);

			const FVector3f ForwardUnit = Forward.GetSafeNormal();

			FVector3f Right;
			if (Guides[i].bHasCarbonyl && !ForwardUnit.IsNearlyZero())
			{
				Right = FVector3f::CrossProduct(ForwardUnit, Guides[i].Right).GetSafeNormal();
			}

			if (Right.IsNearlyZero())
			{
				// Ohne Carbonylgruppe — Nukleinsaeuren, unvollstaendige Residuen — wird
				// die Querrichtung von der Vorgaengerin weitergetragen. Das ergibt eine
				// ruhige Kurve statt einer willkuerlichen Ausrichtung.
				const FVector3f Hint = PreviousRight.IsNearlyZero() ? FVector3f::ZAxisVector : PreviousRight;
				Right = MakePerpendicular(ForwardUnit, Hint);
			}

			// Der entscheidende Schritt: im Faltblatt wechseln die Carbonylgruppen die
			// Seite. Ohne diese Korrektur kippte das Band bei jedem Residuum um.
			if (!PreviousRight.IsNearlyZero() && FVector3f::DotProduct(Right, PreviousRight) < 0.f)
			{
				Right = -Right;
			}

			Guides[i].Right = Right;
			PreviousRight = Right;
		}
	}

	void TessellateSegment(const TArray<FMolGuidePoint>& Guides, int32 ChainIndex,
		int32 SegmentsPerResidue, FMolBackboneSegment& OutSegment)
	{
		const int32 Num = Guides.Num();
		const int32 Steps = FMath::Max(1, SegmentsPerResidue);

		OutSegment.ChainIndex = ChainIndex;
		OutSegment.Points.Reserve((Num - 1) * Steps + 1);

		auto GuideAt = [&Guides, Num](int32 Index) -> const FMolGuidePoint&
		{
			return Guides[FMath::Clamp(Index, 0, Num - 1)];
		};

		const float TotalSpans = static_cast<float>(Num - 1);

		for (int32 Span = 0; Span < Num - 1; ++Span)
		{
			const FMolGuidePoint& G0 = GuideAt(Span - 1);
			const FMolGuidePoint& G1 = GuideAt(Span);
			const FMolGuidePoint& G2 = GuideAt(Span + 1);
			const FMolGuidePoint& G3 = GuideAt(Span + 2);

			// Der letzte Punkt eines Spans ist der erste des naechsten; nur der
			// allerletzte Span nimmt ihn mit, sonst gaebe es Doppelpunkte.
			const int32 StepCount = (Span == Num - 2) ? Steps + 1 : Steps;

			for (int32 Step = 0; Step < StepCount; ++Step)
			{
				const float T = static_cast<float>(Step) / static_cast<float>(Steps);

				FMolBackbonePoint& Point = OutSegment.Points.AddDefaulted_GetRef();
				Point.Position = CatmullRom(G0.Position, G1.Position, G2.Position, G3.Position, T);

				const FVector3f Tangent =
					CatmullRomTangent(G0.Position, G1.Position, G2.Position, G3.Position, T);
				Point.Forward = Tangent.GetSafeNormal();

				if (Point.Forward.IsNearlyZero())
				{
					Point.Forward = (G2.Position - G1.Position).GetSafeNormal();
				}

				// Querrichtung zwischen den Stuetzpunkten mischen und anschliessend
				// wieder senkrecht zur Laufrichtung stellen — sonst waere das
				// Koordinatensystem nach dem Mischen schief.
				const FVector3f BlendedRight = FMath::Lerp(G1.Right, G2.Right, T);
				Point.Right = MakePerpendicular(Point.Forward, BlendedRight);
				Point.Up = FVector3f::CrossProduct(Point.Forward, Point.Right).GetSafeNormal();

				// Der Wechsel der Sekundaerstruktur liegt in der Mitte zwischen zwei
				// Residuen, nicht an einem der beiden — so springt das Profil dort,
				// wo ohnehin niemand eine scharfe Grenze erwartet.
				const FMolGuidePoint& Nearest = (T < 0.5f) ? G1 : G2;
				Point.SecondaryStructure = Nearest.SecondaryStructure;
				Point.ResidueIndex = Nearest.ResidueIndex;

				Point.Alpha = (TotalSpans > 0.f)
					? (static_cast<float>(Span) + T) / TotalSpans
					: 0.f;
			}
		}
	}
}

namespace MolecularForge
{
	void BuildBackboneSegments(const UMolecularStructure& Structure,
		const FMolBackboneOptions& Options, TArray<FMolBackboneSegment>& OutSegments)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(MolecularForge_BuildBackboneSegments);

		OutSegments.Reset();

		for (int32 ChainIndex = 0; ChainIndex < Structure.GetNumChains(); ++ChainIndex)
		{
			const EMolChainKind Kind = Structure.Chains[ChainIndex].Kind;
			if (Kind != EMolChainKind::Protein && Kind != EMolChainKind::Dna && Kind != EMolChainKind::Rna)
			{
				// Liganden, Ionen und Wasser haben kein Rueckgrat.
				continue;
			}

			TArray<TArray<FMolGuidePoint>> GuideSegments;
			CollectGuideSegments(Structure, ChainIndex, Options.MaxAnchorGapAngstrom, GuideSegments);

			for (TArray<FMolGuidePoint>& Guides : GuideSegments)
			{
				ResolveGuideFrames(Guides);

				FMolBackboneSegment Segment;
				TessellateSegment(Guides, ChainIndex, Options.SegmentsPerResidue, Segment);

				if (Segment.Points.Num() >= 2)
				{
					OutSegments.Add(MoveTemp(Segment));
				}
			}
		}

		UE_LOG(LogMolecularForge, Verbose, TEXT("Rueckgrat aufgebaut: %d Abschnitte."), OutSegments.Num());
	}
}
