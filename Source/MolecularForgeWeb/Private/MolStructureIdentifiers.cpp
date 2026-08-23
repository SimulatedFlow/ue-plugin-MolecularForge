// Copyright 2026 Silvan Teufel All Rights Reserved.

#include "MolStructureIdentifiers.h"

namespace
{
	bool IsUpperAscii(TCHAR C) { return C >= TEXT('A') && C <= TEXT('Z'); }
	bool IsDigitAscii(TCHAR C) { return C >= TEXT('0') && C <= TEXT('9'); }
	bool IsAlnumAscii(TCHAR C) { return IsUpperAscii(C) || IsDigitAscii(C); }

	const TCHAR* GRcsbDownloadBase = TEXT("https://files.rcsb.org/download/");
	const TCHAR* GAlphaFoldApiBase = TEXT("https://alphafold.ebi.ac.uk/api/prediction/");
}

namespace MolecularForge
{
	bool IsValidPdbIdentifier(const FString& Identifier)
	{
		const FString Upper = Identifier.ToUpper();

		// Klassische Form: 1CRN, 4HHB, 6VXX.
		if (Upper.Len() == 4)
		{
			if (!(Upper[0] >= TEXT('1') && Upper[0] <= TEXT('9')))
			{
				return false;
			}
			for (int32 i = 1; i < 4; ++i)
			{
				if (!IsAlnumAscii(Upper[i]))
				{
					return false;
				}
			}
			return true;
		}

		// Erweiterte Form: PDB_00001CRN.
		if (Upper.Len() == 12 && Upper.StartsWith(TEXT("PDB_"), ESearchCase::CaseSensitive))
		{
			for (int32 i = 4; i < 12; ++i)
			{
				if (!IsAlnumAscii(Upper[i]))
				{
					return false;
				}
			}
			return true;
		}

		return false;
	}

	bool IsValidUniProtAccession(const FString& Accession)
	{
		const FString Upper = Accession.ToUpper();
		const int32 Len = Upper.Len();

		if (Len != 6 && Len != 10)
		{
			return false;
		}

		// Muster 1: [OPQ][0-9][A-Z0-9]{3}[0-9] — die aelteren, sechsstelligen Accessions.
		if (Len == 6)
		{
			const TCHAR First = Upper[0];
			if ((First == TEXT('O') || First == TEXT('P') || First == TEXT('Q'))
				&& IsDigitAscii(Upper[1])
				&& IsAlnumAscii(Upper[2]) && IsAlnumAscii(Upper[3]) && IsAlnumAscii(Upper[4])
				&& IsDigitAscii(Upper[5]))
			{
				return true;
			}
		}

		// Muster 2: [A-NR-Z][0-9]([A-Z][A-Z0-9]{2}[0-9]){1,2} — sechs oder zehn Stellen.
		const TCHAR First = Upper[0];
		if (!IsUpperAscii(First) || First == TEXT('O') || First == TEXT('P') || First == TEXT('Q'))
		{
			return false;
		}
		if (!IsDigitAscii(Upper[1]))
		{
			return false;
		}

		const int32 NumGroups = (Len - 2) / 4;
		if (NumGroups * 4 != Len - 2 || NumGroups < 1 || NumGroups > 2)
		{
			return false;
		}

		for (int32 Group = 0; Group < NumGroups; ++Group)
		{
			const int32 Base = 2 + Group * 4;
			if (!IsUpperAscii(Upper[Base])
				|| !IsAlnumAscii(Upper[Base + 1])
				|| !IsAlnumAscii(Upper[Base + 2])
				|| !IsDigitAscii(Upper[Base + 3]))
			{
				return false;
			}
		}

		return true;
	}

	bool IsValidIdentifier(EMolFetchSource Source, const FString& Identifier)
	{
		switch (Source)
		{
		case EMolFetchSource::RcsbPdb:		return IsValidPdbIdentifier(Identifier);
		case EMolFetchSource::AlphaFoldDb:	return IsValidUniProtAccession(Identifier);
		default:							return false;
		}
	}

	FString NormalizeIdentifier(EMolFetchSource Source, const FString& Identifier)
	{
		return Identifier.TrimStartAndEnd().ToUpper();
	}

	FString BuildRequestUrl(EMolFetchSource Source, const FString& Identifier)
	{
		const FString Normalized = NormalizeIdentifier(Source, Identifier);
		if (!IsValidIdentifier(Source, Normalized))
		{
			return FString();
		}

		switch (Source)
		{
		case EMolFetchSource::RcsbPdb:
			return FString::Printf(TEXT("%s%s.cif"), GRcsbDownloadBase, *Normalized);

		case EMolFetchSource::AlphaFoldDb:
			return FString::Printf(TEXT("%s%s"), GAlphaFoldApiBase, *Normalized);

		default:
			return FString();
		}
	}

	FString GetCacheFileExtension(EMolFetchSource Source)
	{
		// Beide Quellen liefern mmCIF: bei RCSB, weil grosse Strukturen im PDB-Format nicht
		// mehr darstellbar sind; bei AlphaFold, weil die cif-Variante die Konfidenzangaben
		// in maschinenlesbarer Form mitbringt.
		return TEXT("cif");
	}

	FString GetSourceSlug(EMolFetchSource Source)
	{
		switch (Source)
		{
		case EMolFetchSource::RcsbPdb:		return TEXT("rcsb");
		case EMolFetchSource::AlphaFoldDb:	return TEXT("alphafold");
		default:							return TEXT("unbekannt");
		}
	}
}
