// Copyright 2026 Silvan Teufel All Rights Reserved.

#include "MolPdbParser.h"
#include "MolecularStructure.h"
#include "MolElementTable.h"
#include "MolResidueTable.h"
#include "MolBondBuilder.h"
#include "MolStructureAssembler.h"
#include "MolSecondaryStructure.h"
#include "MolTextUtils.h"
#include "Async/ParallelFor.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
	using namespace MolecularForge::Text;

	/** Ab dieser Zeilenzahl lohnt sich das Aufteilen auf Threads. */
	constexpr int32 GParallelLineThreshold = 4096;

	bool RecordIs(FStringView Line, const TCHAR* Record)
	{
		const int32 Len = FCString::Strlen(Record);
		if (Line.Len() < Len)
		{
			return false;
		}
		return FCString::Strnicmp(Line.GetData(), Record, Len) == 0;
	}

	/** Zerlegt den Text in Zeilen-Views ohne zu kopieren; \r am Zeilenende faellt weg. */
	void SplitIntoLines(FStringView Text, TArray<FStringView>& OutLines)
	{
		// Grobschaetzung: PDB-Zeilen sind 80 Zeichen breit. Spart das Nachwachsen des Arrays.
		OutLines.Reserve(Text.Len() / 78 + 16);

		int32 LineStart = 0;
		for (int32 i = 0; i < Text.Len(); ++i)
		{
			if (Text[i] == TEXT('\n'))
			{
				int32 LineEnd = i;
				if (LineEnd > LineStart && Text[LineEnd - 1] == TEXT('\r'))
				{
					--LineEnd;
				}
				OutLines.Add(Text.Mid(LineStart, LineEnd - LineStart));
				LineStart = i + 1;
			}
		}
		if (LineStart < Text.Len())
		{
			OutLines.Add(Text.Mid(LineStart, Text.Len() - LineStart));
		}
	}

	/**
	 * Eine ATOM-/HETATM-Zeile nach den festen PDB-Spalten lesen.
	 * Spaltenangaben nullbasiert, die Formatspezifikation zaehlt ab 1:
	 *   [0,6) Record | [6,11) Serial | [12,16) Atomname | [16] AltLoc | [17,20) Residuenname
	 *   [21] Ketten-ID | [22,26) Sequenznummer | [26] Insertion-Code
	 *   [30,38) X | [38,46) Y | [46,54) Z | [54,60) Besetzung | [60,66) Temperaturfaktor
	 *   [76,78) Element
	 */
	bool ParseAtomLine(FStringView Line, bool bHetatm, FMolRawAtom& Out)
	{
		float X, Y, Z;
		if (!ParseFixedFloat(Line, 30, 8, X) ||
			!ParseFixedFloat(Line, 38, 8, Y) ||
			!ParseFixedFloat(Line, 46, 8, Z))
		{
			return false;
		}

		Out.Position = FVector3f(X, Y, Z);
		Out.bHetatm = bHetatm;

		if (!ParseFixedFloat(Line, 54, 6, Out.Occupancy))
		{
			Out.Occupancy = 1.f;
		}
		if (!ParseFixedFloat(Line, 60, 6, Out.BFactor))
		{
			Out.BFactor = 0.f;
		}

		Out.Name = NameFromSlice(Line, 12, 4);
		Out.ResidueName = NameFromSlice(Line, 17, 3);
		Out.AltLoc = CharAtOrSpace(Line, 16);
		Out.ChainId = NameFromSlice(Line, 21, 1);
		Out.InsertionCode = CharAtOrSpace(Line, 26);

		if (!ParseFixedInt(Line, 22, 4, Out.ResidueSeq))
		{
			Out.ResidueSeq = 0;
		}

		// Elementspalte zuerst. Aeltere Dateien lassen sie leer — dann muss der
		// Atomname herhalten, und dort entscheidet die Spaltenposition.
		Out.Element = MolecularForge::AtomicNumberFromSymbol(SafeSlice(Line, 76, 2));
		if (Out.Element == 0)
		{
			Out.Element = MolecularForge::GuessAtomicNumberFromAtomName(SafeSlice(Line, 12, 4), bHetatm);
		}

		Out.bValid = true;
		return true;
	}
}

namespace MolecularForge
{
	FMolParseResult ParsePdb(FStringView PdbText, const FMolLoadOptions& Options, UMolecularStructure& OutStructure)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(MolecularForge_ParsePdb);
		const double StartTime = FPlatformTime::Seconds();

		FMolParseResult Result;
		OutStructure.Reset();

		if (PdbText.IsEmpty())
		{
			Result.Error = TEXT("Die Datei ist leer.");
			return Result;
		}

		TArray<FStringView> Lines;
		SplitIntoLines(PdbText, Lines);

		// ---- Stufe 1: sequenzielle Klassifizierung ----
		// Nur die ersten Zeichen anschauen, Kopfdaten mitnehmen, Atomzeilen vormerken.

		TArray<int32> AtomLineIndices;
		TArray<uint8> AtomIsHetatm;
		TArray<uint8> ChainBreakBefore;
		TArray<FMolSecondaryRange> SecondaryRanges;

		AtomLineIndices.Reserve(Lines.Num());
		AtomIsHetatm.Reserve(Lines.Num());
		ChainBreakBefore.Reserve(Lines.Num());

		int32 CurrentModel = 1;
		int32 NumModels = 1;
		bool bSawModelRecord = false;
		bool bPendingChainBreak = false;
		bool bLooksLikeAlphaFold = false;

		for (int32 LineIndex = 0; LineIndex < Lines.Num(); ++LineIndex)
		{
			const FStringView& Line = Lines[LineIndex];
			if (Line.Len() < 6)
			{
				continue;
			}

			const bool bIsAtom = RecordIs(Line, TEXT("ATOM  "));
			const bool bIsHetatm = !bIsAtom && RecordIs(Line, TEXT("HETATM"));

			if (bIsAtom || bIsHetatm)
			{
				// Nur Modell 1 uebernehmen. NMR-Ensembles wuerden sonst als ein
				// einziges verschmiertes Knaeuel erscheinen.
				if (CurrentModel != 1)
				{
					continue;
				}
				AtomLineIndices.Add(LineIndex);
				AtomIsHetatm.Add(bIsHetatm ? 1 : 0);
				ChainBreakBefore.Add(bPendingChainBreak ? 1 : 0);
				bPendingChainBreak = false;
				continue;
			}

			if (RecordIs(Line, TEXT("TER")))
			{
				bPendingChainBreak = true;
				continue;
			}

			if (RecordIs(Line, TEXT("MODEL ")))
			{
				bSawModelRecord = true;
				int32 ModelNumber = 1;
				ParseFixedInt(Line, 10, 4, ModelNumber);
				CurrentModel = ModelNumber;
				NumModels = FMath::Max(NumModels, ModelNumber);
				bPendingChainBreak = true;
				continue;
			}

			if (RecordIs(Line, TEXT("ENDMDL")))
			{
				// Ohne folgendes MODEL waere alles Weitere sonst wieder Modell 1.
				CurrentModel = bSawModelRecord ? CurrentModel + 1 : 1;
				continue;
			}

			if (RecordIs(Line, TEXT("HEADER")))
			{
				// Der PDB-Code steht in [62,66).
				const FStringView Code = TrimView(SafeSlice(Line, 62, 4));
				if (!Code.IsEmpty())
				{
					OutStructure.Meta.Identifier = FString(Code);
				}
				continue;
			}

			if (RecordIs(Line, TEXT("TITLE ")))
			{
				const FStringView Part = TrimView(SafeSlice(Line, 10, 70));
				if (!Part.IsEmpty())
				{
					if (!OutStructure.Meta.Title.IsEmpty())
					{
						OutStructure.Meta.Title.AppendChar(TEXT(' '));
					}
					OutStructure.Meta.Title.Append(Part.GetData(), Part.Len());
				}
				continue;
			}

			if (RecordIs(Line, TEXT("EXPDTA")))
			{
				OutStructure.Meta.ExperimentalMethod = FString(TrimView(SafeSlice(Line, 10, 70)));
				continue;
			}

			if (RecordIs(Line, TEXT("REMARK")))
			{
				// "REMARK   2 RESOLUTION.    1.50 ANGSTROMS."
				int32 RemarkNumber = 0;
				ParseFixedInt(Line, 7, 3, RemarkNumber);
				if (RemarkNumber == 2)
				{
					const FString RestString(SafeSlice(Line, 11, 60));
					if (RestString.Contains(TEXT("RESOLUTION")))
					{
						float Resolution = 0.f;
						if (ParseFixedFloat(Line, 23, 8, Resolution) && Resolution > 0.f)
						{
							OutStructure.Meta.ResolutionAngstrom = Resolution;
						}
					}
				}

				if (!bLooksLikeAlphaFold && FString(Line).Contains(TEXT("ALPHAFOLD")))
				{
					bLooksLikeAlphaFold = true;
				}
				continue;
			}

			if (RecordIs(Line, TEXT("HELIX ")))
			{
				FMolSecondaryRange Range;
				Range.Kind = EMolSecondaryStructure::Helix;
				Range.ChainId = NameFromSlice(Line, 19, 1);
				ParseFixedInt(Line, 21, 4, Range.FirstSeq);
				ParseFixedInt(Line, 33, 4, Range.LastSeq);
				SecondaryRanges.Add(Range);
				continue;
			}

			if (RecordIs(Line, TEXT("SHEET ")))
			{
				// Achtung: SHEET hat andere Spalten fuer die Sequenznummern als HELIX.
				FMolSecondaryRange Range;
				Range.Kind = EMolSecondaryStructure::Sheet;
				Range.ChainId = NameFromSlice(Line, 21, 1);
				ParseFixedInt(Line, 22, 4, Range.FirstSeq);
				ParseFixedInt(Line, 33, 4, Range.LastSeq);
				SecondaryRanges.Add(Range);
				continue;
			}
		}

		if (AtomLineIndices.IsEmpty())
		{
			Result.Error = TEXT("Kein einziger ATOM- oder HETATM-Record gefunden. Ist das wirklich eine PDB-Datei?");
			return Result;
		}

		if (!bLooksLikeAlphaFold && OutStructure.Meta.Title.Contains(TEXT("ALPHAFOLD")))
		{
			bLooksLikeAlphaFold = true;
		}

		// ---- Stufe 2: parallele Zeilenauswertung ----

		const int32 NumRaw = AtomLineIndices.Num();
		TArray<FMolRawAtom> RawAtoms;
		RawAtoms.SetNum(NumRaw);

		std::atomic<int32> MalformedCount{ 0 };

		ParallelFor(NumRaw, [&](int32 Index)
		{
			const FStringView& Line = Lines[AtomLineIndices[Index]];
			if (!ParseAtomLine(Line, AtomIsHetatm[Index] != 0, RawAtoms[Index]))
			{
				RawAtoms[Index].bValid = false;
				MalformedCount.fetch_add(1, std::memory_order_relaxed);
			}
		}, NumRaw < GParallelLineThreshold ? EParallelForFlags::ForceSingleThread : EParallelForFlags::None);

		Result.NumMalformedLines = MalformedCount.load(std::memory_order_relaxed);

		// ---- Stufe 3: verdichten und gruppieren ----
		// Formatunabhaengig, deshalb im gemeinsamen Assembler.

		FMolAssembleStats Stats;
		AssembleStructure(RawAtoms, ChainBreakBefore, SecondaryRanges, Options, OutStructure, Stats);

		if (Stats.NumAtomsKept == 0)
		{
			Result.Error = TEXT("Nach Anwendung der Ladeoptionen ist kein Atom uebrig geblieben.");
			return Result;
		}

		// ---- Metadaten abschliessen ----

		OutStructure.Meta.NumModelsInFile = NumModels;
		OutStructure.Meta.bBFactorIsPLDDT = bLooksLikeAlphaFold;
		if (bLooksLikeAlphaFold)
		{
			OutStructure.Meta.Source = EMolStructureSource::AlphaFoldDb;
			OutStructure.Meta.Attribution = GetAlphaFoldAttribution();
		}

		OutStructure.FinalizeAfterLoad();

		ApplySecondaryStructurePolicy(OutStructure, Options, !SecondaryRanges.IsEmpty());

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

		UE_LOG(LogMolecularForge, Log, TEXT("PDB geladen: %s (%.1f ms, %d verworfen, %d fehlerhafte Zeilen)"),
			*OutStructure.GetSummary(), Result.ParseSeconds * 1000.0,
			Stats.NumAtomsDiscarded, Result.NumMalformedLines);

		return Result;
	}

	FMolParseResult ParsePdbFile(const FString& FilePath, const FMolLoadOptions& Options, UMolecularStructure& OutStructure)
	{
		FMolParseResult Result;

		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *FilePath))
		{
			Result.Error = FString::Printf(TEXT("Datei nicht lesbar: %s"), *FilePath);
			UE_LOG(LogMolecularForge, Warning, TEXT("%s"), *Result.Error);
			return Result;
		}

		Result = ParsePdb(Text, Options, OutStructure);

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
