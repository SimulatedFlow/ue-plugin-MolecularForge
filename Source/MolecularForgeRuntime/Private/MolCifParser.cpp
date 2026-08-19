// Copyright Silvan Teufel. All Rights Reserved.

#include "MolCifParser.h"
#include "MolecularStructure.h"
#include "MolElementTable.h"
#include "MolBondBuilder.h"
#include "MolStructureAssembler.h"
#include "MolTextUtils.h"
#include "Async/ParallelFor.h"
#include "Misc/FileHelper.h"

namespace
{
	using namespace MolecularForge::Text;

	constexpr int32 GParallelRowThreshold = 4096;

	bool IsCifWhitespace(TCHAR C)
	{
		return C == TEXT(' ') || C == TEXT('\t') || C == TEXT('\r') || C == TEXT('\n');
	}

	/** In CIF stehen `.` (nicht anwendbar) und `?` (unbekannt) fuer einen fehlenden Wert. */
	bool IsCifNull(FStringView Value)
	{
		return Value.Len() == 1 && (Value[0] == TEXT('.') || Value[0] == TEXT('?'));
	}

	bool StartsWithCaseless(FStringView View, const TCHAR* Prefix)
	{
		const int32 Len = FCString::Strlen(Prefix);
		return View.Len() >= Len && FCString::Strnicmp(View.GetData(), Prefix, Len) == 0;
	}

	/**
	 * Zerlegt CIF-Text in Token, ohne zu kopieren.
	 *
	 * Bewusst als kleiner Wertetyp gehalten: der parallele Zeilenlauf erzeugt pro Zeile
	 * einen eigenen Tokenizer auf demselben Text. Weil der gesamte Zustand aus zwei
	 * Zahlen besteht, ist das gratis und ohne geteilten Zustand.
	 */
	struct FCifTokenizer
	{
		FStringView Text;
		int32 Pos = 0;

		FCifTokenizer() = default;
		FCifTokenizer(FStringView InText, int32 InPos) : Text(InText), Pos(InPos) {}

		/** True, wenn Pos am Anfang einer Zeile steht — entscheidend fuer `;`-Textfelder. */
		bool IsAtLineStart() const
		{
			return Pos == 0 || (Pos <= Text.Len() && Text[Pos - 1] == TEXT('\n'));
		}

		void SkipWhitespaceAndComments()
		{
			while (Pos < Text.Len())
			{
				const TCHAR C = Text[Pos];
				if (IsCifWhitespace(C))
				{
					++Pos;
				}
				else if (C == TEXT('#'))
				{
					// Kommentar bis Zeilenende. Das Zeilenende selbst bleibt stehen,
					// damit die Erkennung des Zeilenanfangs weiter stimmt.
					while (Pos < Text.Len() && Text[Pos] != TEXT('\n'))
					{
						++Pos;
					}
				}
				else
				{
					break;
				}
			}
		}

		/** Naechstes Token. Gibt false am Dateiende zurueck. */
		bool Next(FStringView& OutToken, bool& bOutQuoted)
		{
			SkipWhitespaceAndComments();
			if (Pos >= Text.Len())
			{
				return false;
			}

			bOutQuoted = false;
			const TCHAR C = Text[Pos];

			// Mehrzeiliges Textfeld: ';' als erstes Zeichen einer Zeile, beendet durch
			// eine Zeile, die wieder mit ';' beginnt. So stehen lange Titel in mmCIF.
			if (C == TEXT(';') && IsAtLineStart())
			{
				const int32 ContentStart = Pos + 1;
				for (int32 Scan = ContentStart; Scan < Text.Len(); ++Scan)
				{
					if (Text[Scan] == TEXT('\n') && Scan + 1 < Text.Len() && Text[Scan + 1] == TEXT(';'))
					{
						OutToken = Text.Mid(ContentStart, Scan - ContentStart);
						Pos = Scan + 2;
						bOutQuoted = true;
						return true;
					}
				}
				OutToken = Text.Mid(ContentStart, Text.Len() - ContentStart);
				Pos = Text.Len();
				bOutQuoted = true;
				return true;
			}

			if (C == TEXT('\'') || C == TEXT('"'))
			{
				const TCHAR Quote = C;
				const int32 ContentStart = Pos + 1;
				for (int32 Scan = ContentStart; Scan < Text.Len(); ++Scan)
				{
					// Ein Anfuehrungszeichen beendet den Wert nur, wenn danach Leerraum
					// oder Dateiende kommt. Sonst bliebe von "5'-Ende" nur "5" uebrig.
					if (Text[Scan] == Quote &&
						(Scan + 1 >= Text.Len() || IsCifWhitespace(Text[Scan + 1])))
					{
						OutToken = Text.Mid(ContentStart, Scan - ContentStart);
						Pos = Scan + 1;
						bOutQuoted = true;
						return true;
					}
				}
				OutToken = Text.Mid(ContentStart, Text.Len() - ContentStart);
				Pos = Text.Len();
				bOutQuoted = true;
				return true;
			}

			const int32 Start = Pos;
			while (Pos < Text.Len() && !IsCifWhitespace(Text[Pos]))
			{
				++Pos;
			}
			OutToken = Text.Mid(Start, Pos - Start);
			return true;
		}
	};

	bool IsTagToken(FStringView Token, bool bQuoted)
	{
		return !bQuoted && Token.Len() > 1 && Token[0] == TEXT('_');
	}

	bool IsKeywordToken(FStringView Token, bool bQuoted, const TCHAR* Keyword)
	{
		return !bQuoted && Token.Equals(Keyword, ESearchCase::IgnoreCase);
	}

	/** True, wenn dieses Token einen Datenabschnitt beendet. */
	bool IsSectionBoundary(FStringView Token, bool bQuoted)
	{
		if (bQuoted)
		{
			return false;
		}
		return IsTagToken(Token, bQuoted)
			|| IsKeywordToken(Token, bQuoted, TEXT("loop_"))
			|| IsKeywordToken(Token, bQuoted, TEXT("stop_"))
			|| StartsWithCaseless(Token, TEXT("data_"))
			|| StartsWithCaseless(Token, TEXT("save_"));
	}

	/** `_atom_site.Cartn_x` -> Kategorie "atom_site", Element "cartn_x" (beides klein). */
	void SplitTag(FStringView Tag, FString& OutCategory, FString& OutItem)
	{
		const FStringView Body = Tag.Mid(1);
		int32 Dot = INDEX_NONE;
		for (int32 i = 0; i < Body.Len(); ++i)
		{
			if (Body[i] == TEXT('.'))
			{
				Dot = i;
				break;
			}
		}

		if (Dot == INDEX_NONE)
		{
			OutCategory = FString(Body).ToLower();
			OutItem.Empty();
		}
		else
		{
			OutCategory = FString(Body.Mid(0, Dot)).ToLower();
			OutItem = FString(Body.Mid(Dot + 1)).ToLower();
		}
	}

	/** Spaltenzuordnung der `atom_site`-Schleife. INDEX_NONE heisst "nicht vorhanden". */
	struct FAtomSiteColumns
	{
		int32 GroupPdb = INDEX_NONE;
		int32 TypeSymbol = INDEX_NONE;
		int32 LabelAtomId = INDEX_NONE;
		int32 AuthAtomId = INDEX_NONE;
		int32 LabelAltId = INDEX_NONE;
		int32 LabelCompId = INDEX_NONE;
		int32 AuthCompId = INDEX_NONE;
		int32 LabelAsymId = INDEX_NONE;
		int32 AuthAsymId = INDEX_NONE;
		int32 LabelSeqId = INDEX_NONE;
		int32 AuthSeqId = INDEX_NONE;
		int32 InsCode = INDEX_NONE;
		int32 X = INDEX_NONE;
		int32 Y = INDEX_NONE;
		int32 Z = INDEX_NONE;
		int32 Occupancy = INDEX_NONE;
		int32 BIso = INDEX_NONE;
		int32 ModelNum = INDEX_NONE;

		bool HasCoordinates() const { return X != INDEX_NONE && Y != INDEX_NONE && Z != INDEX_NONE; }
	};

	FAtomSiteColumns MapAtomSiteColumns(const TArray<FString>& ItemNames)
	{
		FAtomSiteColumns Cols;
		for (int32 i = 0; i < ItemNames.Num(); ++i)
		{
			const FString& Item = ItemNames[i];
			if (Item == TEXT("group_pdb"))				{ Cols.GroupPdb = i; }
			else if (Item == TEXT("type_symbol"))		{ Cols.TypeSymbol = i; }
			else if (Item == TEXT("label_atom_id"))		{ Cols.LabelAtomId = i; }
			else if (Item == TEXT("auth_atom_id"))		{ Cols.AuthAtomId = i; }
			else if (Item == TEXT("label_alt_id"))		{ Cols.LabelAltId = i; }
			else if (Item == TEXT("label_comp_id"))		{ Cols.LabelCompId = i; }
			else if (Item == TEXT("auth_comp_id"))		{ Cols.AuthCompId = i; }
			else if (Item == TEXT("label_asym_id"))		{ Cols.LabelAsymId = i; }
			else if (Item == TEXT("auth_asym_id"))		{ Cols.AuthAsymId = i; }
			else if (Item == TEXT("label_seq_id"))		{ Cols.LabelSeqId = i; }
			else if (Item == TEXT("auth_seq_id"))		{ Cols.AuthSeqId = i; }
			else if (Item == TEXT("pdbx_pdb_ins_code"))	{ Cols.InsCode = i; }
			else if (Item == TEXT("cartn_x"))			{ Cols.X = i; }
			else if (Item == TEXT("cartn_y"))			{ Cols.Y = i; }
			else if (Item == TEXT("cartn_z"))			{ Cols.Z = i; }
			else if (Item == TEXT("occupancy"))			{ Cols.Occupancy = i; }
			else if (Item == TEXT("b_iso_or_equiv"))	{ Cols.BIso = i; }
			else if (Item == TEXT("pdbx_pdb_model_num")){ Cols.ModelNum = i; }
		}
		return Cols;
	}

	/**
	 * Eine Datenzeile der atom_site-Schleife lesen.
	 *
	 * Bei den Bezeichnern wird `auth_*` bevorzugt und `label_*` als Rueckfall benutzt.
	 * Das ist kein Detail: `auth_seq_id` ist die Nummerierung, die in der Literatur steht
	 * und die Anwender erwarten, waehrend `label_seq_id` eine fortlaufende interne Zaehlung
	 * ist. Wer "Rest 145 der Kette B" sucht, meint die auth-Nummer.
	 */
	void ParseAtomSiteRow(FStringView Text, int32 RowStart, int32 NumColumns,
		const FAtomSiteColumns& Cols, FMolRawAtom& Out, int32& OutModelNum)
	{
		TArray<FStringView, TInlineAllocator<32>> Values;
		Values.SetNum(NumColumns);

		FCifTokenizer Tok(Text, RowStart);
		for (int32 i = 0; i < NumColumns; ++i)
		{
			FStringView Token;
			bool bQuoted = false;
			if (!Tok.Next(Token, bQuoted))
			{
				return;
			}
			Values[i] = Token;
		}

		auto Get = [&Values, NumColumns](int32 Column) -> FStringView
		{
			if (Column == INDEX_NONE || Column >= NumColumns)
			{
				return FStringView();
			}
			const FStringView& Value = Values[Column];
			return IsCifNull(Value) ? FStringView() : Value;
		};

		// Erst die Koordinaten: ohne sie ist die Zeile wertlos.
		float X = 0.f, Y = 0.f, Z = 0.f;
		if (!ViewToFloat(Get(Cols.X), X) || !ViewToFloat(Get(Cols.Y), Y) || !ViewToFloat(Get(Cols.Z), Z))
		{
			return;
		}
		Out.Position = FVector3f(X, Y, Z);

		const FStringView Group = Get(Cols.GroupPdb);
		Out.bHetatm = !Group.IsEmpty() && Group.Equals(TEXT("HETATM"), ESearchCase::IgnoreCase);

		FStringView AtomName = Get(Cols.AuthAtomId);
		if (AtomName.IsEmpty()) { AtomName = Get(Cols.LabelAtomId); }
		Out.Name = NameFromView(AtomName);

		FStringView CompId = Get(Cols.AuthCompId);
		if (CompId.IsEmpty()) { CompId = Get(Cols.LabelCompId); }
		Out.ResidueName = NameFromView(CompId);

		FStringView AsymId = Get(Cols.AuthAsymId);
		if (AsymId.IsEmpty()) { AsymId = Get(Cols.LabelAsymId); }
		Out.ChainId = NameFromView(AsymId);

		int32 SeqId = 0;
		if (!ViewToInt(Get(Cols.AuthSeqId), SeqId))
		{
			ViewToInt(Get(Cols.LabelSeqId), SeqId);
		}
		Out.ResidueSeq = SeqId;

		const FStringView AltId = Get(Cols.LabelAltId);
		Out.AltLoc = AltId.IsEmpty() ? ' ' : static_cast<uint8>(AltId[0]);

		const FStringView InsCode = Get(Cols.InsCode);
		Out.InsertionCode = InsCode.IsEmpty() ? ' ' : static_cast<uint8>(InsCode[0]);

		if (!ViewToFloat(Get(Cols.Occupancy), Out.Occupancy))
		{
			Out.Occupancy = 1.f;
		}
		if (!ViewToFloat(Get(Cols.BIso), Out.BFactor))
		{
			Out.BFactor = 0.f;
		}

		Out.Element = MolecularForge::AtomicNumberFromSymbol(Get(Cols.TypeSymbol));
		if (Out.Element == 0)
		{
			// mmCIF-Atomnamen sind nicht spaltenausgerichtet; die Heuristik kommt damit
			// zurecht, weil sie bei nicht-leerem erstem Zeichen ohnehin ueber das
			// HETATM-Kennzeichen entscheidet.
			Out.Element = MolecularForge::GuessAtomicNumberFromAtomName(AtomName, Out.bHetatm);
		}

		OutModelNum = 1;
		ViewToInt(Get(Cols.ModelNum), OutModelNum);

		Out.bValid = true;
	}

	EMolSecondaryStructure SecondaryKindFromConfType(FStringView ConfType)
	{
		if (StartsWithCaseless(ConfType, TEXT("HELX")))	{ return EMolSecondaryStructure::Helix; }
		if (StartsWithCaseless(ConfType, TEXT("STRN")))	{ return EMolSecondaryStructure::Sheet; }
		if (StartsWithCaseless(ConfType, TEXT("SHEET"))){ return EMolSecondaryStructure::Sheet; }
		if (StartsWithCaseless(ConfType, TEXT("TURN")))	{ return EMolSecondaryStructure::Turn; }
		if (StartsWithCaseless(ConfType, TEXT("BEND")))	{ return EMolSecondaryStructure::Turn; }
		return EMolSecondaryStructure::Coil;
	}

	/** Liest die Werte einer kleinen Schleife vollstaendig ein (sequenziell, ohne Offsets). */
	void ReadSmallLoopValues(FCifTokenizer& Tok, TArray<FStringView>& OutValues)
	{
		while (true)
		{
			const int32 Before = Tok.Pos;
			FStringView Token;
			bool bQuoted = false;
			if (!Tok.Next(Token, bQuoted))
			{
				break;
			}
			if (IsSectionBoundary(Token, bQuoted))
			{
				Tok.Pos = Before;
				break;
			}
			OutValues.Add(Token);
		}
	}

	/** Ueberspringt die Datenzeilen einer Schleife, die uns nicht interessiert. */
	void SkipLoopValues(FCifTokenizer& Tok)
	{
		TArray<FStringView> Ignored;
		ReadSmallLoopValues(Tok, Ignored);
	}

	int32 FindColumn(const TArray<FString>& Items, const TCHAR* Name)
	{
		return Items.IndexOfByPredicate([Name](const FString& Item) { return Item == Name; });
	}
}

namespace MolecularForge
{
	FMolParseResult ParseCif(FStringView CifText, const FMolLoadOptions& Options, UMolecularStructure& OutStructure)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(MolecularForge_ParseCif);
		const double StartTime = FPlatformTime::Seconds();

		FMolParseResult Result;
		OutStructure.Reset();

		if (CifText.IsEmpty())
		{
			Result.Error = TEXT("Die Datei ist leer.");
			return Result;
		}

		// ---- Stufe 1: sequenzieller Lauf durch den Token-Strom ----

		FCifTokenizer Tok(CifText, 0);

		TMap<FString, FString> KeyValues;
		TArray<FMolSecondaryRange> SecondaryRanges;

		TArray<int32> AtomRowStarts;
		int32 AtomNumColumns = 0;
		FAtomSiteColumns AtomColumns;
		bool bHaveAtomSite = false;
		bool bSawQualityMetric = false;

		while (true)
		{
			FStringView Token;
			bool bQuoted = false;
			if (!Tok.Next(Token, bQuoted))
			{
				break;
			}

			if (IsKeywordToken(Token, bQuoted, TEXT("loop_")))
			{
				// Kopf der Schleife: alle folgenden Tags gehoeren zur Spaltenliste.
				TArray<FString> ItemNames;
				FString Category;
				int32 AfterLastTag = Tok.Pos;

				while (true)
				{
					const int32 Before = Tok.Pos;
					FStringView TagToken;
					bool bTagQuoted = false;
					if (!Tok.Next(TagToken, bTagQuoted))
					{
						AfterLastTag = Tok.Pos;
						break;
					}
					if (!IsTagToken(TagToken, bTagQuoted))
					{
						// Ein Token zu weit gelesen — das ist bereits der erste Datenwert.
						Tok.Pos = Before;
						AfterLastTag = Before;
						break;
					}

					FString TagCategory, TagItem;
					SplitTag(TagToken, TagCategory, TagItem);
					if (Category.IsEmpty())
					{
						Category = TagCategory;
					}
					ItemNames.Add(TagItem);
					AfterLastTag = Tok.Pos;
				}

				Tok.Pos = AfterLastTag;

				if (Category == TEXT("ma_qa_metric"))
				{
					bSawQualityMetric = true;
				}

				if (ItemNames.IsEmpty())
				{
					continue;
				}

				if (Category == TEXT("atom_site") && !bHaveAtomSite)
				{
					AtomNumColumns = ItemNames.Num();
					AtomColumns = MapAtomSiteColumns(ItemNames);

					if (!AtomColumns.HasCoordinates())
					{
						Result.Error = TEXT("Die atom_site-Schleife enthaelt keine Koordinatenspalten.");
						return Result;
					}

					// Nur die Zeilenanfaenge merken. Siehe Begruendung im Header.
					int32 TokenCount = 0;
					while (true)
					{
						Tok.SkipWhitespaceAndComments();
						const int32 TokenStart = Tok.Pos;

						FStringView ValueToken;
						bool bValueQuoted = false;
						if (!Tok.Next(ValueToken, bValueQuoted))
						{
							break;
						}
						if (IsSectionBoundary(ValueToken, bValueQuoted))
						{
							Tok.Pos = TokenStart;
							break;
						}
						if (TokenCount % AtomNumColumns == 0)
						{
							AtomRowStarts.Add(TokenStart);
						}
						++TokenCount;
					}

					// Eine angefangene letzte Zeile ist unbrauchbar und fliegt raus.
					const int32 NumCompleteRows = TokenCount / AtomNumColumns;
					if (AtomRowStarts.Num() > NumCompleteRows)
					{
						AtomRowStarts.SetNum(NumCompleteRows);
					}

					bHaveAtomSite = true;
					continue;
				}

				if (Category == TEXT("struct_conf"))
				{
					TArray<FStringView> Values;
					ReadSmallLoopValues(Tok, Values);

					const int32 NumCols = ItemNames.Num();
					const int32 ColType = FindColumn(ItemNames, TEXT("conf_type_id"));
					int32 ColBegAsym = FindColumn(ItemNames, TEXT("beg_auth_asym_id"));
					if (ColBegAsym == INDEX_NONE) { ColBegAsym = FindColumn(ItemNames, TEXT("beg_label_asym_id")); }
					int32 ColBegSeq = FindColumn(ItemNames, TEXT("beg_auth_seq_id"));
					if (ColBegSeq == INDEX_NONE) { ColBegSeq = FindColumn(ItemNames, TEXT("beg_label_seq_id")); }
					int32 ColEndSeq = FindColumn(ItemNames, TEXT("end_auth_seq_id"));
					if (ColEndSeq == INDEX_NONE) { ColEndSeq = FindColumn(ItemNames, TEXT("end_label_seq_id")); }

					if (ColBegAsym != INDEX_NONE && ColBegSeq != INDEX_NONE && ColEndSeq != INDEX_NONE)
					{
						for (int32 Row = 0; Row + NumCols <= Values.Num(); Row += NumCols)
						{
							FMolSecondaryRange Range;
							Range.Kind = (ColType != INDEX_NONE)
								? SecondaryKindFromConfType(Values[Row + ColType])
								: EMolSecondaryStructure::Helix;
							Range.ChainId = NameFromView(Values[Row + ColBegAsym]);
							ViewToInt(Values[Row + ColBegSeq], Range.FirstSeq);
							ViewToInt(Values[Row + ColEndSeq], Range.LastSeq);

							if (Range.Kind != EMolSecondaryStructure::Coil)
							{
								SecondaryRanges.Add(Range);
							}
						}
					}
					continue;
				}

				if (Category == TEXT("struct_sheet_range"))
				{
					TArray<FStringView> Values;
					ReadSmallLoopValues(Tok, Values);

					const int32 NumCols = ItemNames.Num();
					int32 ColBegAsym = FindColumn(ItemNames, TEXT("beg_auth_asym_id"));
					if (ColBegAsym == INDEX_NONE) { ColBegAsym = FindColumn(ItemNames, TEXT("beg_label_asym_id")); }
					int32 ColBegSeq = FindColumn(ItemNames, TEXT("beg_auth_seq_id"));
					if (ColBegSeq == INDEX_NONE) { ColBegSeq = FindColumn(ItemNames, TEXT("beg_label_seq_id")); }
					int32 ColEndSeq = FindColumn(ItemNames, TEXT("end_auth_seq_id"));
					if (ColEndSeq == INDEX_NONE) { ColEndSeq = FindColumn(ItemNames, TEXT("end_label_seq_id")); }

					if (ColBegAsym != INDEX_NONE && ColBegSeq != INDEX_NONE && ColEndSeq != INDEX_NONE)
					{
						for (int32 Row = 0; Row + NumCols <= Values.Num(); Row += NumCols)
						{
							FMolSecondaryRange Range;
							Range.Kind = EMolSecondaryStructure::Sheet;
							Range.ChainId = NameFromView(Values[Row + ColBegAsym]);
							ViewToInt(Values[Row + ColBegSeq], Range.FirstSeq);
							ViewToInt(Values[Row + ColEndSeq], Range.LastSeq);
							SecondaryRanges.Add(Range);
						}
					}
					continue;
				}

				SkipLoopValues(Tok);
				continue;
			}

			if (IsTagToken(Token, bQuoted))
			{
				FString Category, Item;
				SplitTag(Token, Category, Item);
				if (Category == TEXT("ma_qa_metric"))
				{
					bSawQualityMetric = true;
				}

				FStringView Value;
				bool bValueQuoted = false;
				if (Tok.Next(Value, bValueQuoted))
				{
					if (!IsCifNull(Value))
					{
						KeyValues.Add(Category + TEXT(".") + Item, FString(TrimView(Value)));
					}
				}
				continue;
			}

			// `data_...`, `save_...` und alles Uebrige: keine Bedeutung fuer uns.
		}

		if (!bHaveAtomSite || AtomRowStarts.IsEmpty())
		{
			Result.Error = TEXT("Keine atom_site-Daten gefunden. Ist das wirklich eine mmCIF-Datei?");
			return Result;
		}

		// ---- Stufe 2: parallele Zeilenauswertung ----

		const int32 NumRows = AtomRowStarts.Num();
		TArray<FMolRawAtom> RawAtoms;
		RawAtoms.SetNum(NumRows);

		std::atomic<int32> MalformedCount{ 0 };
		std::atomic<int32> MaxModelNum{ 1 };

		ParallelFor(NumRows, [&](int32 RowIndex)
		{
			int32 ModelNum = 1;
			ParseAtomSiteRow(CifText, AtomRowStarts[RowIndex], AtomNumColumns,
				AtomColumns, RawAtoms[RowIndex], ModelNum);

			if (!RawAtoms[RowIndex].bValid)
			{
				MalformedCount.fetch_add(1, std::memory_order_relaxed);
				return;
			}

			// Nur Modell 1 uebernehmen — bei Ensembles laegen sonst alle Modelle uebereinander.
			if (ModelNum != 1)
			{
				RawAtoms[RowIndex].bValid = false;

				int32 Previous = MaxModelNum.load(std::memory_order_relaxed);
				while (ModelNum > Previous &&
					!MaxModelNum.compare_exchange_weak(Previous, ModelNum, std::memory_order_relaxed))
				{
				}
			}
		}, NumRows < GParallelRowThreshold ? EParallelForFlags::ForceSingleThread : EParallelForFlags::None);

		Result.NumMalformedLines = MalformedCount.load(std::memory_order_relaxed);

		// ---- Stufe 3: verdichten und gruppieren ----
		// mmCIF kennt keinen TER-Record; Kettenwechsel ergeben sich aus der asym-ID.

		FMolAssembleStats Stats;
		AssembleStructure(RawAtoms, TArray<uint8>(), SecondaryRanges, Options, OutStructure, Stats);

		if (Stats.NumAtomsKept == 0)
		{
			Result.Error = TEXT("Nach Anwendung der Ladeoptionen ist kein Atom uebrig geblieben.");
			return Result;
		}

		// ---- Metadaten ----

		if (const FString* Entry = KeyValues.Find(TEXT("entry.id")))
		{
			OutStructure.Meta.Identifier = *Entry;
		}
		if (const FString* Title = KeyValues.Find(TEXT("struct.title")))
		{
			OutStructure.Meta.Title = *Title;
		}
		if (const FString* Method = KeyValues.Find(TEXT("exptl.method")))
		{
			OutStructure.Meta.ExperimentalMethod = *Method;
		}
		if (const FString* Resolution = KeyValues.Find(TEXT("refine.ls_d_res_high")))
		{
			OutStructure.Meta.ResolutionAngstrom = FCString::Atof(**Resolution);
		}

		OutStructure.Meta.NumModelsInFile = MaxModelNum.load(std::memory_order_relaxed);

		// AlphaFold-Dateien tragen die Kennung "AF-<Accession>-F<n>" und liefern eine
		// ma_qa_metric-Kategorie mit dem pLDDT-Wert, der im B-Faktor-Feld landet.
		const bool bLooksLikeAlphaFold =
			bSawQualityMetric
			|| OutStructure.Meta.Identifier.StartsWith(TEXT("AF-"))
			|| OutStructure.Meta.Title.ToUpper().Contains(TEXT("ALPHAFOLD"));

		OutStructure.Meta.bBFactorIsPLDDT = bLooksLikeAlphaFold;
		if (bLooksLikeAlphaFold)
		{
			OutStructure.Meta.Source = EMolStructureSource::AlphaFoldDb;
			OutStructure.Meta.Attribution = GetAlphaFoldAttribution();
		}

		OutStructure.FinalizeAfterLoad();

		if (Options.bDeriveBonds)
		{
			BuildBondsByDistance(OutStructure);
		}

		if (Options.bCenterOnOrigin)
		{
			OutStructure.CenterOnOrigin();
		}

		Result.bSuccess = true;
		Result.NumAtomsParsed = Stats.NumAtomsKept;
		Result.NumAtomsDiscarded = Stats.NumAtomsDiscarded;
		Result.ParseSeconds = FPlatformTime::Seconds() - StartTime;

		UE_LOG(LogMolecularForge, Log, TEXT("mmCIF geladen: %s (%.1f ms, %d verworfen, %d fehlerhafte Zeilen)"),
			*OutStructure.GetSummary(), Result.ParseSeconds * 1000.0,
			Stats.NumAtomsDiscarded, Result.NumMalformedLines);

		return Result;
	}

	FMolParseResult ParseCifFile(const FString& FilePath, const FMolLoadOptions& Options, UMolecularStructure& OutStructure)
	{
		FMolParseResult Result;

		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *FilePath))
		{
			Result.Error = FString::Printf(TEXT("Datei nicht lesbar: %s"), *FilePath);
			UE_LOG(LogMolecularForge, Warning, TEXT("%s"), *Result.Error);
			return Result;
		}

		Result = ParseCif(Text, Options, OutStructure);

		if (Result.bSuccess && OutStructure.Meta.Identifier.IsEmpty())
		{
			OutStructure.Meta.Identifier = FPaths::GetBaseFilename(FilePath);
		}
		if (Result.bSuccess && OutStructure.Meta.Source == EMolStructureSource::Unknown)
		{
			OutStructure.Meta.Source = EMolStructureSource::LocalFile;
		}

		return Result;
	}
}
