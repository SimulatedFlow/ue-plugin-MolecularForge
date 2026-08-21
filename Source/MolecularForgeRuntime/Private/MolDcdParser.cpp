// Copyright 2026 Simulated Flow All Rights Reserved.

#include "MolDcdParser.h"
#include "MolecularTrajectory.h"
#include "MolecularForgeTypes.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
	/**
	 * Groesse des Kopfblocks: vier Zeichen Kennung plus zwanzig Ganzzahlen.
	 * Der Wert steht als Laengenmarke am Anfang der Datei und ist damit gleichzeitig
	 * die Probe darauf, ob es sich ueberhaupt um eine DCD-Datei handelt und in welcher
	 * Bytereihenfolge sie geschrieben wurde.
	 */
	constexpr int32 GHeaderBlockSize = 84;

	/** Ein Zeitschritt mit Boxangaben traegt sechs doppelt genaue Werte. */
	constexpr int32 GUnitCellBlockSize = 48;

	/** Obergrenze fuer die Atomzahl, damit eine kaputte Datei nicht Gigabyte anfordert. */
	constexpr int32 GMaxPlausibleAtoms = 50 * 1000 * 1000;

	int32 SwapInt32(int32 Value)
	{
		const uint32 U = static_cast<uint32>(Value);
		return static_cast<int32>(
			((U & 0x000000FFu) << 24) |
			((U & 0x0000FF00u) << 8) |
			((U & 0x00FF0000u) >> 8) |
			((U & 0xFF000000u) >> 24));
	}

	float SwapFloat(float Value)
	{
		uint32 U;
		FMemory::Memcpy(&U, &Value, sizeof(U));
		U = ((U & 0x000000FFu) << 24) | ((U & 0x0000FF00u) << 8)
			| ((U & 0x00FF0000u) >> 8) | ((U & 0xFF000000u) >> 24);

		float Result;
		FMemory::Memcpy(&Result, &U, sizeof(Result));
		return Result;
	}

	double SwapDouble(double Value)
	{
		uint64 U;
		FMemory::Memcpy(&U, &Value, sizeof(U));
		U = ((U & 0x00000000000000FFull) << 56) | ((U & 0x000000000000FF00ull) << 40)
			| ((U & 0x0000000000FF0000ull) << 24) | ((U & 0x00000000FF000000ull) << 8)
			| ((U & 0x000000FF00000000ull) >> 8) | ((U & 0x0000FF0000000000ull) >> 24)
			| ((U & 0x00FF000000000000ull) >> 40) | ((U & 0xFF00000000000000ull) >> 56);

		double Result;
		FMemory::Memcpy(&Result, &U, sizeof(Result));
		return Result;
	}

	/**
	 * Liest fortlaufend aus einem Bytepuffer und prueft dabei jeden Zugriff auf die Grenze.
	 * Eine abgeschnittene Datei — und Trajektorien brechen beim Kopieren gern ab — muss
	 * zu einer Fehlermeldung fuehren und nicht zu einem Zugriff daneben.
	 */
	struct FDcdReader
	{
		TArrayView<const uint8> Data;
		int64 Pos = 0;
		bool bSwapBytes = false;

		bool CanRead(int64 Size) const { return Pos >= 0 && Pos + Size <= Data.Num(); }
		int64 Remaining() const { return Data.Num() - Pos; }

		bool ReadRaw(void* Dest, int64 Size)
		{
			if (!CanRead(Size))
			{
				return false;
			}
			FMemory::Memcpy(Dest, Data.GetData() + Pos, Size);
			Pos += Size;
			return true;
		}

		bool ReadInt32(int32& OutValue)
		{
			if (!ReadRaw(&OutValue, sizeof(int32)))
			{
				return false;
			}
			if (bSwapBytes)
			{
				OutValue = SwapInt32(OutValue);
			}
			return true;
		}

		bool ReadDouble(double& OutValue)
		{
			if (!ReadRaw(&OutValue, sizeof(double)))
			{
				return false;
			}
			if (bSwapBytes)
			{
				OutValue = SwapDouble(OutValue);
			}
			return true;
		}

		bool ReadFloats(float* Dest, int32 Count)
		{
			if (!ReadRaw(Dest, static_cast<int64>(Count) * sizeof(float)))
			{
				return false;
			}
			if (bSwapBytes)
			{
				for (int32 i = 0; i < Count; ++i)
				{
					Dest[i] = SwapFloat(Dest[i]);
				}
			}
			return true;
		}

		bool Skip(int64 Size)
		{
			if (!CanRead(Size))
			{
				return false;
			}
			Pos += Size;
			return true;
		}

		/**
		 * Liest die abschliessende Laengenmarke eines Blocks und vergleicht sie mit der
		 * einleitenden. Stimmen die beiden nicht ueberein, ist der Lesekopf verrutscht —
		 * und dann ist es besser, hier abzubrechen, als weiter Zahlen zu erfinden.
		 */
		bool ExpectBlockEnd(int32 ExpectedSize)
		{
			int32 Trailing = 0;
			if (!ReadInt32(Trailing))
			{
				return false;
			}
			return Trailing == ExpectedSize;
		}
	};
}

namespace MolecularForge
{
	FMolTrajectoryResult ParseDcd(TArrayView<const uint8> Bytes,
		const FMolTrajectoryLoadOptions& Options, UMolecularTrajectory& OutTrajectory)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(MolecularForge_ParseDcd);
		const double StartTime = FPlatformTime::Seconds();

		FMolTrajectoryResult Result;
		OutTrajectory.Reset();

		auto Fail = [&Result](const FString& Message)
		{
			Result.bSuccess = false;
			Result.Error = Message;
			return Result;
		};

		FDcdReader Reader;
		Reader.Data = Bytes;

		// ---- Kopfblock ----

		int32 BlockSize = 0;
		if (!Reader.ReadInt32(BlockSize))
		{
			return Fail(TEXT("Die Datei ist zu kurz fuer einen DCD-Kopf."));
		}

		if (BlockSize != GHeaderBlockSize)
		{
			// Andere Bytereihenfolge? Dann muss die Marke nach dem Tausch stimmen.
			if (SwapInt32(BlockSize) == GHeaderBlockSize)
			{
				Reader.bSwapBytes = true;
				BlockSize = GHeaderBlockSize;
			}
			else
			{
				return Fail(FString::Printf(
					TEXT("Das ist keine DCD-Datei: erwartet wurde eine Blockmarke von %d, gelesen wurde %d."),
					GHeaderBlockSize, BlockSize));
			}
		}

		char Magic[4] = {};
		if (!Reader.ReadRaw(Magic, 4))
		{
			return Fail(TEXT("Die Datei bricht schon im Kopf ab."));
		}
		if (FMemory::Memcmp(Magic, "CORD", 4) != 0)
		{
			return Fail(TEXT("Die Kennung im Kopf lautet nicht 'CORD'. Koordinaten enthaelt die Datei damit nicht."));
		}

		int32 Control[20] = {};
		for (int32 i = 0; i < 20; ++i)
		{
			if (!Reader.ReadInt32(Control[i]))
			{
				return Fail(TEXT("Die Kopfdaten sind unvollstaendig."));
			}
		}

		if (!Reader.ExpectBlockEnd(GHeaderBlockSize))
		{
			return Fail(TEXT("Der Kopfblock ist nicht sauber abgeschlossen."));
		}

		const int32 NumFramesInFile = Control[0];
		const int32 NumFixedAtoms = Control[8];
		const int32 UnitCellFlag = Control[10];
		const int32 CharmmVersion = Control[19];

		// Der Zeitschritt steht als Gleitkommazahl an einer Stelle, die sonst Ganzzahlen
		// traegt — ein Erbe des Formats. Nur die CHARMM-Variante meint es so.
		//
		// Hier wird bewusst *nicht* noch einmal getauscht: der Wert kam ueber ReadInt32
		// herein und hat die Bytereihenfolge damit schon hinter sich. Ein zweiter Tausch
		// waere einer zu viel und ergaebe einen Zeitschritt in der Groessenordnung 10^-38.
		float TimeStep = 0.f;
		if (CharmmVersion != 0)
		{
			FMemory::Memcpy(&TimeStep, &Control[9], sizeof(float));
		}

		if (NumFixedAtoms > 0)
		{
			return Fail(FString::Printf(
				TEXT("Diese Datei haelt %d Atome fest. In dem Fall enthalten alle Bilder ausser dem ersten "
					 "nur die beweglichen Atome, und ohne diese Zuordnung waere die Struktur durcheinander. "
					 "Solche Dateien werden derzeit nicht gelesen."),
				NumFixedAtoms));
		}

		// ---- Titelblock ----

		int32 TitleBlockSize = 0;
		if (!Reader.ReadInt32(TitleBlockSize))
		{
			return Fail(TEXT("Der Titelblock fehlt."));
		}

		int32 NumTitleLines = 0;
		if (!Reader.ReadInt32(NumTitleLines))
		{
			return Fail(TEXT("Der Titelblock ist unvollstaendig."));
		}

		if (NumTitleLines < 0 || TitleBlockSize != 4 + NumTitleLines * 80)
		{
			return Fail(TEXT("Der Titelblock hat eine unstimmige Groesse."));
		}

		if (!Reader.Skip(static_cast<int64>(NumTitleLines) * 80))
		{
			return Fail(TEXT("Der Titelblock bricht ab."));
		}
		if (!Reader.ExpectBlockEnd(TitleBlockSize))
		{
			return Fail(TEXT("Der Titelblock ist nicht sauber abgeschlossen."));
		}

		// ---- Atomzahl ----

		int32 AtomBlockSize = 0;
		if (!Reader.ReadInt32(AtomBlockSize) || AtomBlockSize != 4)
		{
			return Fail(TEXT("Der Block mit der Atomzahl fehlt oder hat die falsche Groesse."));
		}

		int32 NumAtoms = 0;
		if (!Reader.ReadInt32(NumAtoms))
		{
			return Fail(TEXT("Die Atomzahl liess sich nicht lesen."));
		}
		if (!Reader.ExpectBlockEnd(4))
		{
			return Fail(TEXT("Der Block mit der Atomzahl ist nicht sauber abgeschlossen."));
		}

		if (NumAtoms <= 0 || NumAtoms > GMaxPlausibleAtoms)
		{
			return Fail(FString::Printf(TEXT("Die Atomzahl %d ist nicht plausibel."), NumAtoms));
		}

		Result.NumAtoms = NumAtoms;
		Result.NumFramesInFile = NumFramesInFile;

		// ---- Bilder ----

		const int32 Stride = FMath::Max(1, Options.FrameStride);
		const int32 CoordinateBlockSize = NumAtoms * static_cast<int32>(sizeof(float));

		OutTrajectory.NumAtoms = NumAtoms;
		OutTrajectory.TimeStepPicoseconds = TimeStep;

		// Der Kopf nennt eine Bildzahl, aber sie stimmt bei abgebrochenen Laeufen nicht.
		// Deshalb wird sie nur zum Vorreservieren benutzt und nicht als Abbruchbedingung.
		if (NumFramesInFile > 0 && NumFramesInFile < 1000000)
		{
			const int32 Expected = Options.MaxFrames > 0
				? FMath::Min(Options.MaxFrames, NumFramesInFile / Stride + 1)
				: NumFramesInFile / Stride + 1;
			OutTrajectory.Positions.Reserve(static_cast<int64>(Expected) * NumAtoms);
		}

		TArray<float> ComponentBuffer;
		ComponentBuffer.SetNumUninitialized(NumAtoms);

		int32 FrameIndexInFile = 0;
		int32 FramesLoaded = 0;

		while (Reader.Remaining() > 0)
		{
			const bool bKeepThisFrame = (FrameIndexInFile % Stride) == 0
				&& (Options.MaxFrames <= 0 || FramesLoaded < Options.MaxFrames);

			// Boxabmessungen, falls vorhanden.
			FVector3f UnitCell = FVector3f::ZeroVector;
			if (UnitCellFlag != 0)
			{
				int32 CellBlockSize = 0;
				if (!Reader.ReadInt32(CellBlockSize))
				{
					break;	// sauberes Dateiende
				}
				if (CellBlockSize != GUnitCellBlockSize)
				{
					return Fail(FString::Printf(
						TEXT("Bild %d: der Block mit den Boxangaben hat die Groesse %d statt %d."),
						FrameIndexInFile, CellBlockSize, GUnitCellBlockSize));
				}

				double Cell[6] = {};
				for (int32 i = 0; i < 6; ++i)
				{
					if (!Reader.ReadDouble(Cell[i]))
					{
						return Fail(FString::Printf(
							TEXT("Bild %d: die Boxangaben brechen ab."), FrameIndexInFile));
					}
				}
				if (!Reader.ExpectBlockEnd(GUnitCellBlockSize))
				{
					return Fail(FString::Printf(
						TEXT("Bild %d: der Block mit den Boxangaben ist nicht sauber abgeschlossen."),
						FrameIndexInFile));
				}

				// Die Reihenfolge im Format ist A, gamma, B, beta, alpha, C — die Winkel
				// stehen zwischen den Kantenlaengen. Uns interessieren nur die Laengen.
				UnitCell = FVector3f(
					static_cast<float>(Cell[0]),
					static_cast<float>(Cell[2]),
					static_cast<float>(Cell[5]));
			}

			// Erst wenn hier noch etwas kommt, ist das Bild wirklich vorhanden.
			if (Reader.Remaining() <= 0)
			{
				break;
			}

			const int32 FrameStart = bKeepThisFrame ? OutTrajectory.Positions.Num() : INDEX_NONE;
			if (bKeepThisFrame)
			{
				OutTrajectory.Positions.AddUninitialized(NumAtoms);
			}

			// X, Y und Z stehen jeweils als eigener Block hintereinander, nicht verschraenkt.
			for (int32 Axis = 0; Axis < 3; ++Axis)
			{
				int32 CoordSize = 0;
				if (!Reader.ReadInt32(CoordSize))
				{
					// Mitten im Bild zu Ende: das letzte Bild ist unvollstaendig und faellt weg.
					if (bKeepThisFrame)
					{
						OutTrajectory.Positions.SetNum(FrameStart, EAllowShrinking::No);
					}
					Result.bSuccess = FramesLoaded > 0;
					if (!Result.bSuccess)
					{
						return Fail(TEXT("Die Datei enthaelt kein vollstaendiges Bild."));
					}
					goto Finished;
				}

				if (CoordSize != CoordinateBlockSize)
				{
					return Fail(FString::Printf(
						TEXT("Bild %d, Achse %d: der Koordinatenblock ist %d Byte gross, erwartet wurden %d. "
							 "Entweder passt die Atomzahl nicht oder die Datei ist beschaedigt."),
						FrameIndexInFile, Axis, CoordSize, CoordinateBlockSize));
				}

				if (!Reader.ReadFloats(ComponentBuffer.GetData(), NumAtoms))
				{
					if (bKeepThisFrame)
					{
						OutTrajectory.Positions.SetNum(FrameStart, EAllowShrinking::No);
					}
					Result.bSuccess = FramesLoaded > 0;
					if (!Result.bSuccess)
					{
						return Fail(TEXT("Die Koordinaten des ersten Bildes sind unvollstaendig."));
					}
					goto Finished;
				}

				if (!Reader.ExpectBlockEnd(CoordinateBlockSize))
				{
					return Fail(FString::Printf(
						TEXT("Bild %d, Achse %d: der Koordinatenblock ist nicht sauber abgeschlossen."),
						FrameIndexInFile, Axis));
				}

				if (bKeepThisFrame)
				{
					FVector3f* Target = OutTrajectory.Positions.GetData() + FrameStart;
					for (int32 i = 0; i < NumAtoms; ++i)
					{
						Target[i][Axis] = ComponentBuffer[i];
					}
				}
			}

			if (bKeepThisFrame)
			{
				++FramesLoaded;
				if (Options.bLoadUnitCell && UnitCellFlag != 0)
				{
					OutTrajectory.UnitCellSizes.Add(UnitCell);
				}
			}

			++FrameIndexInFile;

			// Genug geladen? Dann nicht weiter durch die Datei laufen.
			if (Options.MaxFrames > 0 && FramesLoaded >= Options.MaxFrames)
			{
				break;
			}
		}

		Result.bSuccess = FramesLoaded > 0;
		if (!Result.bSuccess)
		{
			return Fail(TEXT("Die Datei enthaelt kein einziges vollstaendiges Bild."));
		}

	Finished:
		Result.NumFramesLoaded = FramesLoaded;
		Result.ParseSeconds = FPlatformTime::Seconds() - StartTime;

		UE_LOG(LogMolecularForge, Log, TEXT("DCD geladen: %s (%.1f ms)"),
			*OutTrajectory.GetSummary(), Result.ParseSeconds * 1000.0);

		return Result;
	}

	FMolTrajectoryResult ParseDcdFile(const FString& FilePath,
		const FMolTrajectoryLoadOptions& Options, UMolecularTrajectory& OutTrajectory)
	{
		FMolTrajectoryResult Result;

		TArray<uint8> Bytes;
		if (!FFileHelper::LoadFileToArray(Bytes, *FilePath))
		{
			Result.Error = FString::Printf(TEXT("Datei nicht lesbar: %s"), *FilePath);
			UE_LOG(LogMolecularForge, Warning, TEXT("%s"), *Result.Error);
			return Result;
		}

		Result = ParseDcd(Bytes, Options, OutTrajectory);

		if (Result.bSuccess)
		{
			OutTrajectory.SourceFile = FPaths::GetCleanFilename(FilePath);
		}

		return Result;
	}
}
