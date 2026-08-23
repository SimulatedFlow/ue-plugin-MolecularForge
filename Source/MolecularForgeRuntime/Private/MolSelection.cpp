// Copyright 2026 Silvan Teufel All Rights Reserved.

#include "MolSelection.h"
#include "MolecularStructure.h"
#include "MolElementTable.h"
#include "MolResidueTable.h"

namespace
{
	/** Ein Wort oder eine Klammer aus dem Ausdruck, mit seiner Position fuer Fehlermeldungen. */
	struct FMolToken
	{
		FString Text;
		int32 Position = 0;
		bool bIsParen = false;
	};

	void Tokenize(const FString& Expression, TArray<FMolToken>& OutTokens)
	{
		const int32 Length = Expression.Len();
		int32 i = 0;

		while (i < Length)
		{
			const TCHAR C = Expression[i];

			if (FChar::IsWhitespace(C))
			{
				++i;
				continue;
			}

			if (C == TEXT('(') || C == TEXT(')'))
			{
				OutTokens.Add(FMolToken{ FString(1, &C), i, true });
				++i;
				continue;
			}

			// Alles bis zum naechsten Leerzeichen oder zur naechsten Klammer bleibt
			// zusammen. Damit sind Wertelisten wie `1-50+60` ein einziges Wort und der
			// Bindestrich muss nicht vom Minuszeichen unterschieden werden.
			const int32 Start = i;
			while (i < Length && !FChar::IsWhitespace(Expression[i])
				&& Expression[i] != TEXT('(') && Expression[i] != TEXT(')'))
			{
				++i;
			}

			OutTokens.Add(FMolToken{ Expression.Mid(Start, i - Start), Start, false });
		}
	}

	/** Zerlegt eine Werteliste wie `A+B,C` in ihre Teile. */
	void SplitList(const FString& Value, TArray<FString>& OutParts)
	{
		FString Normalized = Value;
		Normalized.ReplaceInline(TEXT(","), TEXT("+"));
		Normalized.ParseIntoArray(OutParts, TEXT("+"), /*CullEmpty=*/true);
	}

	/**
	 * Liest eine Nummernliste mit Bereichen: `1-50+60+70-80`.
	 * Negative Residuennummern kommen vor (etwa bei Expressions-Tags), deshalb wird ein
	 * fuehrender Bindestrich als Vorzeichen und nicht als Bereichstrenner gelesen.
	 */
	bool ParseNumberRanges(const FString& Value, TArray<TPair<int32, int32>>& OutRanges)
	{
		TArray<FString> Parts;
		SplitList(Value, Parts);

		for (const FString& Part : Parts)
		{
			int32 SeparatorIndex = INDEX_NONE;
			for (int32 i = 1; i < Part.Len(); ++i)
			{
				if (Part[i] == TEXT('-'))
				{
					SeparatorIndex = i;
					break;
				}
			}

			if (SeparatorIndex == INDEX_NONE)
			{
				if (!Part.IsNumeric())
				{
					return false;
				}
				const int32 Single = FCString::Atoi(*Part);
				OutRanges.Add(TPair<int32, int32>(Single, Single));
			}
			else
			{
				const FString Low = Part.Left(SeparatorIndex);
				const FString High = Part.Mid(SeparatorIndex + 1);
				if (Low.IsEmpty() || High.IsEmpty())
				{
					return false;
				}
				OutRanges.Add(TPair<int32, int32>(FCString::Atoi(*Low), FCString::Atoi(*High)));
			}
		}

		return !OutRanges.IsEmpty();
	}

	/** Zustand des rekursiven Abstiegs. */
	struct FMolSelectionParser
	{
		const UMolecularStructure& Structure;
		const TArray<FMolToken>& Tokens;
		int32 Index = 0;

		FString Error;
		int32 ErrorPosition = INDEX_NONE;

		FMolSelectionParser(const UMolecularStructure& InStructure, const TArray<FMolToken>& InTokens)
			: Structure(InStructure), Tokens(InTokens)
		{
		}

		bool HasMore() const { return Index < Tokens.Num(); }
		const FMolToken& Peek() const { return Tokens[Index]; }

		int32 CurrentPosition() const
		{
			return HasMore() ? Tokens[Index].Position : (Tokens.Num() > 0 ? Tokens.Last().Position : 0);
		}

		void Fail(const FString& Message, int32 Position)
		{
			if (Error.IsEmpty())
			{
				Error = Message;
				ErrorPosition = Position;
			}
		}

		bool MatchWord(const TCHAR* Word) const
		{
			return HasMore() && !Peek().bIsParen && Peek().Text.Equals(Word, ESearchCase::IgnoreCase);
		}

		bool MatchAnyWord(std::initializer_list<const TCHAR*> Words) const
		{
			for (const TCHAR* Word : Words)
			{
				if (MatchWord(Word))
				{
					return true;
				}
			}
			return false;
		}

		TBitArray<> MakeMask(bool bValue) const
		{
			return TBitArray<>(bValue, Structure.GetNumAtoms());
		}

		/** Holt das Argument eines Schluesselworts. */
		bool TakeArgument(const TCHAR* Keyword, FString& OutValue)
		{
			if (!HasMore() || Peek().bIsParen)
			{
				Fail(FString::Printf(TEXT("Nach '%s' fehlt der Wert."), Keyword), CurrentPosition());
				return false;
			}
			OutValue = Tokens[Index].Text;
			++Index;
			return true;
		}

		TBitArray<> ParseExpression() { return ParseOr(); }

		TBitArray<> ParseOr()
		{
			TBitArray<> Left = ParseAnd();

			while (!Error.IsEmpty() ? false : MatchAnyWord({ TEXT("or"), TEXT("|") }))
			{
				++Index;
				TBitArray<> Right = ParseAnd();
				Left.CombineWithBitwiseOR(Right, EBitwiseOperatorFlags::MaintainSize);
			}

			return Left;
		}

		TBitArray<> ParseAnd()
		{
			TBitArray<> Left = ParseNot();

			// Zwei Ausdruecke ohne Verknuepfung nebeneinander gelten als 'and'. PyMOL macht
			// das ebenso, und `chain A resi 1-10` liest sich natuerlicher als mit 'and'.
			while (Error.IsEmpty() && HasMore()
				&& !MatchAnyWord({ TEXT("or"), TEXT("|") })
				&& !(Peek().bIsParen && Peek().Text == TEXT(")")))
			{
				if (MatchAnyWord({ TEXT("and"), TEXT("&") }))
				{
					++Index;
				}

				TBitArray<> Right = ParseNot();
				Left.CombineWithBitwiseAND(Right, EBitwiseOperatorFlags::MaintainSize);
			}

			return Left;
		}

		TBitArray<> ParseNot()
		{
			if (MatchAnyWord({ TEXT("not"), TEXT("!") }))
			{
				++Index;
				TBitArray<> Inner = ParseNot();
				Inner.BitwiseNOT();
				return Inner;
			}

			return ParsePrimary();
		}

		TBitArray<> ParsePrimary()
		{
			if (!HasMore())
			{
				Fail(TEXT("Der Ausdruck bricht unerwartet ab."), CurrentPosition());
				return MakeMask(false);
			}

			const FMolToken& Token = Peek();

			if (Token.bIsParen)
			{
				if (Token.Text == TEXT("("))
				{
					++Index;
					TBitArray<> Inner = ParseExpression();

					if (!HasMore() || Peek().Text != TEXT(")"))
					{
						Fail(TEXT("Es fehlt eine schliessende Klammer."), CurrentPosition());
						return MakeMask(false);
					}
					++Index;
					return Inner;
				}

				Fail(TEXT("Hier steht eine schliessende Klammer ohne oeffnende."), Token.Position);
				return MakeMask(false);
			}

			return ParseTerm();
		}

		TBitArray<> ParseTerm();

		TBitArray<> SelectByResiduePredicate(TFunctionRef<bool(const FMolResidue&)> Predicate) const
		{
			TBitArray<> Mask = MakeMask(false);

			for (int32 a = 0; a < Structure.GetNumAtoms(); ++a)
			{
				const int32 ResidueIndex = Structure.AtomResidueIndices[a];
				if (Structure.Residues.IsValidIndex(ResidueIndex) && Predicate(Structure.Residues[ResidueIndex]))
				{
					Mask[a] = true;
				}
			}

			return Mask;
		}

		TBitArray<> SelectByResidueClass(std::initializer_list<EMolResidueClass> Classes) const
		{
			TArray<EMolResidueClass, TInlineAllocator<4>> Wanted(Classes);

			return SelectByResiduePredicate([&Wanted](const FMolResidue& Residue)
			{
				return Wanted.Contains(MolecularForge::ClassifyResidue(Residue.Name));
			});
		}

		TBitArray<> SelectByAtomFlag(uint8 Flag, bool bSet = true) const
		{
			TBitArray<> Mask = MakeMask(false);
			for (int32 a = 0; a < Structure.GetNumAtoms(); ++a)
			{
				Mask[a] = ((Structure.AtomFlags[a] & Flag) != 0) == bSet;
			}
			return Mask;
		}

		/** Alle Atome, die naeher als `Distance` an einem Atom der Untermenge liegen. */
		TBitArray<> SelectWithin(float Distance, const TBitArray<>& Inner) const;
	};

	TBitArray<> FMolSelectionParser::SelectWithin(float Distance, const TBitArray<>& Inner) const
	{
		TBitArray<> Mask = MakeMask(false);

		TArray<int32> Seeds;
		for (int32 a = 0; a < Structure.GetNumAtoms(); ++a)
		{
			if (Inner[a])
			{
				Seeds.Add(a);
			}
		}

		if (Seeds.IsEmpty() || Distance <= 0.f)
		{
			return Mask;
		}

		// Nachbarschaftsgitter ueber die Ausgangsatome. Ohne das waere die Suche das
		// Produkt beider Mengen — bei einer Ligandenumgebung in einem grossen Komplex
		// sind das schnell hunderte Millionen Vergleiche fuer eine Handvoll Treffer.
		const float CellSize = FMath::Max(Distance, 1.f);
		const float DistanceSq = Distance * Distance;

		FBox3f SeedBounds(ForceInit);
		for (int32 Seed : Seeds)
		{
			SeedBounds += Structure.AtomPositions[Seed];
		}

		const FVector3f GridMin = SeedBounds.Min;
		const FVector3f Extent = SeedBounds.Max - GridMin;
		const FIntVector Dim(
			FMath::Max(1, FMath::CeilToInt(Extent.X / CellSize) + 1),
			FMath::Max(1, FMath::CeilToInt(Extent.Y / CellSize) + 1),
			FMath::Max(1, FMath::CeilToInt(Extent.Z / CellSize) + 1));

		auto CellOf = [&](const FVector3f& Position)
		{
			const FVector3f Local = (Position - GridMin) / CellSize;
			return FIntVector(
				FMath::FloorToInt(Local.X),
				FMath::FloorToInt(Local.Y),
				FMath::FloorToInt(Local.Z));
		};

		TMultiMap<int32, int32> Buckets;
		Buckets.Reserve(Seeds.Num());

		auto IndexOf = [&Dim](const FIntVector& Cell)
		{
			return (Cell.Z * Dim.Y + Cell.Y) * Dim.X + Cell.X;
		};

		for (int32 Seed : Seeds)
		{
			const FIntVector Cell = CellOf(Structure.AtomPositions[Seed]);
			Buckets.Add(IndexOf(Cell), Seed);
		}

		TArray<int32> Nearby;

		for (int32 a = 0; a < Structure.GetNumAtoms(); ++a)
		{
			const FVector3f Position = Structure.AtomPositions[a];
			const FIntVector Cell = CellOf(Position);

			bool bFound = false;

			for (int32 dz = -1; dz <= 1 && !bFound; ++dz)
			{
				for (int32 dy = -1; dy <= 1 && !bFound; ++dy)
				{
					for (int32 dx = -1; dx <= 1 && !bFound; ++dx)
					{
						const FIntVector Neighbour(Cell.X + dx, Cell.Y + dy, Cell.Z + dz);
						if (Neighbour.X < 0 || Neighbour.X >= Dim.X
							|| Neighbour.Y < 0 || Neighbour.Y >= Dim.Y
							|| Neighbour.Z < 0 || Neighbour.Z >= Dim.Z)
						{
							continue;
						}

						Nearby.Reset();
						Buckets.MultiFind(IndexOf(Neighbour), Nearby);

						for (int32 Seed : Nearby)
						{
							if (FVector3f::DistSquared(Position, Structure.AtomPositions[Seed]) <= DistanceSq)
							{
								bFound = true;
								break;
							}
						}
					}
				}
			}

			Mask[a] = bFound;
		}

		return Mask;
	}

	TBitArray<> FMolSelectionParser::ParseTerm()
	{
		using namespace MolecularForge;

		const FMolToken Token = Peek();
		const FString Word = Token.Text.ToLower();
		++Index;

		if (Word == TEXT("all"))		{ return MakeMask(true); }
		if (Word == TEXT("none"))		{ return MakeMask(false); }

		if (Word == TEXT("protein"))	{ return SelectByResidueClass({ EMolResidueClass::AminoAcid }); }
		if (Word == TEXT("dna"))		{ return SelectByResidueClass({ EMolResidueClass::DeoxyNucleotide }); }
		if (Word == TEXT("rna"))		{ return SelectByResidueClass({ EMolResidueClass::Nucleotide }); }
		if (Word == TEXT("nucleic"))
		{
			return SelectByResidueClass({ EMolResidueClass::DeoxyNucleotide, EMolResidueClass::Nucleotide });
		}
		if (Word == TEXT("water"))		{ return SelectByAtomFlag(MolAtom_Water); }
		if (Word == TEXT("hetero") || Word == TEXT("hetatm")) { return SelectByAtomFlag(MolAtom_Hetatm); }
		if (Word == TEXT("backbone"))	{ return SelectByAtomFlag(MolAtom_Backbone); }

		if (Word == TEXT("ligand"))
		{
			// Ligand heisst: Heterogruppe, aber kein Wasser. Wasser ist zwar formal
			// HETATM, aber niemand meint es, wenn er nach dem Liganden sucht.
			TBitArray<> Mask = SelectByAtomFlag(MolAtom_Hetatm);
			TBitArray<> Water = SelectByAtomFlag(MolAtom_Water);
			Water.BitwiseNOT();
			Mask.CombineWithBitwiseAND(Water, EBitwiseOperatorFlags::MaintainSize);
			return Mask;
		}

		if (Word == TEXT("sidechain"))
		{
			// Seitenkette: Aminosaeure ohne Rueckgrat.
			TBitArray<> Mask = SelectByResidueClass({ EMolResidueClass::AminoAcid });
			TBitArray<> NotBackbone = SelectByAtomFlag(MolAtom_Backbone, /*bSet=*/false);
			Mask.CombineWithBitwiseAND(NotBackbone, EBitwiseOperatorFlags::MaintainSize);
			return Mask;
		}

		if (Word == TEXT("chain"))
		{
			FString Value;
			if (!TakeArgument(TEXT("chain"), Value))
			{
				return MakeMask(false);
			}

			TArray<FString> Parts;
			SplitList(Value, Parts);

			TSet<FName> Wanted;
			for (const FString& Part : Parts)
			{
				Wanted.Add(FName(*Part.ToUpper()));
			}

			TBitArray<> Mask = MakeMask(false);
			for (int32 a = 0; a < Structure.GetNumAtoms(); ++a)
			{
				const int32 ResidueIndex = Structure.AtomResidueIndices[a];
				if (!Structure.Residues.IsValidIndex(ResidueIndex))
				{
					continue;
				}
				const int32 ChainIndex = Structure.Residues[ResidueIndex].ChainIndex;
				if (Structure.Chains.IsValidIndex(ChainIndex)
					&& Wanted.Contains(FName(*Structure.Chains[ChainIndex].Id.ToString().ToUpper())))
				{
					Mask[a] = true;
				}
			}
			return Mask;
		}

		if (Word == TEXT("resi") || Word == TEXT("resid"))
		{
			FString Value;
			if (!TakeArgument(TEXT("resi"), Value))
			{
				return MakeMask(false);
			}

			TArray<TPair<int32, int32>> Ranges;
			if (!ParseNumberRanges(Value, Ranges))
			{
				Fail(FString::Printf(TEXT("'%s' ist keine gueltige Nummernliste."), *Value), Token.Position);
				return MakeMask(false);
			}

			return SelectByResiduePredicate([&Ranges](const FMolResidue& Residue)
			{
				for (const TPair<int32, int32>& Range : Ranges)
				{
					if (Residue.SequenceNumber >= Range.Key && Residue.SequenceNumber <= Range.Value)
					{
						return true;
					}
				}
				return false;
			});
		}

		if (Word == TEXT("resn") || Word == TEXT("resname"))
		{
			FString Value;
			if (!TakeArgument(TEXT("resn"), Value))
			{
				return MakeMask(false);
			}

			TArray<FString> Parts;
			SplitList(Value, Parts);

			TSet<FName> Wanted;
			for (const FString& Part : Parts)
			{
				Wanted.Add(FName(*Part.ToUpper()));
			}

			return SelectByResiduePredicate([&Wanted](const FMolResidue& Residue)
			{
				return Wanted.Contains(Residue.Name);
			});
		}

		if (Word == TEXT("name"))
		{
			FString Value;
			if (!TakeArgument(TEXT("name"), Value))
			{
				return MakeMask(false);
			}

			TArray<FString> Parts;
			SplitList(Value, Parts);

			TSet<FName> Wanted;
			for (const FString& Part : Parts)
			{
				Wanted.Add(FName(*Part.ToUpper()));
			}

			TBitArray<> Mask = MakeMask(false);
			for (int32 a = 0; a < Structure.GetNumAtoms(); ++a)
			{
				Mask[a] = Wanted.Contains(Structure.AtomNames[a]);
			}
			return Mask;
		}

		if (Word == TEXT("element") || Word == TEXT("elem"))
		{
			FString Value;
			if (!TakeArgument(TEXT("element"), Value))
			{
				return MakeMask(false);
			}

			TArray<FString> Parts;
			SplitList(Value, Parts);

			TSet<uint8> Wanted;
			for (const FString& Part : Parts)
			{
				const uint8 Number = AtomicNumberFromSymbol(Part);
				if (Number == 0)
				{
					Fail(FString::Printf(TEXT("'%s' ist kein bekanntes Elementsymbol."), *Part), Token.Position);
					return MakeMask(false);
				}
				Wanted.Add(Number);
			}

			TBitArray<> Mask = MakeMask(false);
			for (int32 a = 0; a < Structure.GetNumAtoms(); ++a)
			{
				Mask[a] = Wanted.Contains(Structure.AtomElements[a]);
			}
			return Mask;
		}

		if (Word == TEXT("ss"))
		{
			FString Value;
			if (!TakeArgument(TEXT("ss"), Value))
			{
				return MakeMask(false);
			}

			TArray<FString> Parts;
			SplitList(Value, Parts);

			TSet<EMolSecondaryStructure> Wanted;
			for (const FString& Part : Parts)
			{
				const FString Upper = Part.ToUpper();
				if (Upper == TEXT("H"))			{ Wanted.Add(EMolSecondaryStructure::Helix); }
				else if (Upper == TEXT("S"))	{ Wanted.Add(EMolSecondaryStructure::Sheet); }
				else if (Upper == TEXT("T"))	{ Wanted.Add(EMolSecondaryStructure::Turn); }
				else if (Upper == TEXT("C"))	{ Wanted.Add(EMolSecondaryStructure::Coil); }
				else
				{
					Fail(FString::Printf(
						TEXT("'%s' ist keine Sekundaerstruktur. Erlaubt sind H, S, T und C."), *Part),
						Token.Position);
					return MakeMask(false);
				}
			}

			return SelectByResiduePredicate([&Wanted](const FMolResidue& Residue)
			{
				return Wanted.Contains(Residue.SecondaryStructure);
			});
		}

		if (Word == TEXT("b") || Word == TEXT("bfactor") || Word == TEXT("plddt"))
		{
			FString Operator;
			if (!TakeArgument(TEXT("b"), Operator))
			{
				return MakeMask(false);
			}

			const bool bGreater = Operator == TEXT(">");
			const bool bLess = Operator == TEXT("<");
			if (!bGreater && !bLess)
			{
				Fail(FString::Printf(TEXT("Nach 'b' wird '<' oder '>' erwartet, nicht '%s'."), *Operator),
					Token.Position);
				return MakeMask(false);
			}

			FString Threshold;
			if (!TakeArgument(TEXT("b"), Threshold))
			{
				return MakeMask(false);
			}

			const float Value = FCString::Atof(*Threshold);

			TBitArray<> Mask = MakeMask(false);
			for (int32 a = 0; a < Structure.GetNumAtoms(); ++a)
			{
				const float B = Structure.AtomBFactors[a];
				Mask[a] = bGreater ? (B > Value) : (B < Value);
			}
			return Mask;
		}

		if (Word == TEXT("within"))
		{
			FString DistanceText;
			if (!TakeArgument(TEXT("within"), DistanceText))
			{
				return MakeMask(false);
			}

			const float Distance = FCString::Atof(*DistanceText);
			if (Distance <= 0.f)
			{
				Fail(FString::Printf(TEXT("'%s' ist kein brauchbarer Abstand."), *DistanceText), Token.Position);
				return MakeMask(false);
			}

			if (!MatchWord(TEXT("of")))
			{
				Fail(TEXT("Nach dem Abstand wird 'of' erwartet."), CurrentPosition());
				return MakeMask(false);
			}
			++Index;

			const TBitArray<> Inner = ParsePrimary();
			if (!Error.IsEmpty())
			{
				return MakeMask(false);
			}

			return SelectWithin(Distance, Inner);
		}

		Fail(FString::Printf(TEXT("'%s' wird nicht verstanden."), *Token.Text), Token.Position);
		return MakeMask(false);
	}
}

namespace MolecularForge
{
	FMolSelectionResult SelectAtoms(const UMolecularStructure& Structure, const FString& Expression)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(MolecularForge_SelectAtoms);

		FMolSelectionResult Result;
		Result.Mask.Init(false, Structure.GetNumAtoms());

		if (Structure.IsEmpty())
		{
			Result.Error = TEXT("Die Struktur enthaelt keine Atome.");
			return Result;
		}

		TArray<FMolToken> Tokens;
		Tokenize(Expression, Tokens);

		if (Tokens.IsEmpty())
		{
			Result.Error = TEXT("Der Ausdruck ist leer.");
			return Result;
		}

		FMolSelectionParser Parser(Structure, Tokens);
		TBitArray<> Mask = Parser.ParseExpression();

		if (!Parser.Error.IsEmpty())
		{
			Result.Error = Parser.Error;
			Result.ErrorPosition = Parser.ErrorPosition;
			return Result;
		}

		if (Parser.HasMore())
		{
			const FMolToken& Leftover = Parser.Peek();

			Result.Error = Leftover.Text == TEXT(")")
				? TEXT("Hier steht eine schliessende Klammer, zu der die oeffnende fehlt.")
				: FString::Printf(TEXT("Nach dem Ausdruck steht noch '%s'."), *Leftover.Text);

			Result.ErrorPosition = Leftover.Position;
			return Result;
		}

		Result.Mask = MoveTemp(Mask);
		Result.NumSelected = Result.Mask.CountSetBits();
		Result.bSuccess = true;
		return Result;
	}

	void MaskToIndices(const TBitArray<>& Mask, TArray<int32>& OutIndices)
	{
		OutIndices.Reset();
		OutIndices.Reserve(Mask.CountSetBits());

		for (TConstSetBitIterator<> It(Mask); It; ++It)
		{
			OutIndices.Add(It.GetIndex());
		}
	}
}
