// Copyright Silvan Teufel. All Rights Reserved.

#include "MolResidueTable.h"

namespace
{
	struct FMolResidueInfo
	{
		EMolResidueClass Class;
		uint8 OneLetter;
	};

	const TMap<FName, FMolResidueInfo>& GetResidueLookup()
	{
		static const TMap<FName, FMolResidueInfo> Lookup = []()
		{
			TMap<FName, FMolResidueInfo> Map;

			auto AddAll = [&Map](std::initializer_list<TPair<const TCHAR*, uint8>> Entries, EMolResidueClass Class)
			{
				for (const TPair<const TCHAR*, uint8>& Entry : Entries)
				{
					Map.Add(FName(Entry.Key), FMolResidueInfo{ Class, Entry.Value });
				}
			};

			// Die 20 proteinogenen Aminosaeuren plus Selenocystein und Pyrrolysin.
			AddAll({
				{ TEXT("ALA"), 'A' }, { TEXT("ARG"), 'R' }, { TEXT("ASN"), 'N' }, { TEXT("ASP"), 'D' },
				{ TEXT("CYS"), 'C' }, { TEXT("GLN"), 'Q' }, { TEXT("GLU"), 'E' }, { TEXT("GLY"), 'G' },
				{ TEXT("HIS"), 'H' }, { TEXT("ILE"), 'I' }, { TEXT("LEU"), 'L' }, { TEXT("LYS"), 'K' },
				{ TEXT("MET"), 'M' }, { TEXT("PHE"), 'F' }, { TEXT("PRO"), 'P' }, { TEXT("SER"), 'S' },
				{ TEXT("THR"), 'T' }, { TEXT("TRP"), 'W' }, { TEXT("TYR"), 'Y' }, { TEXT("VAL"), 'V' },
				{ TEXT("SEC"), 'U' }, { TEXT("PYL"), 'O' },
				// Varianten aus Kraftfeldern und modifizierte Residuen, die im Polymer stehen.
				{ TEXT("MSE"), 'M' },										// Selenomethionin
				{ TEXT("HSD"), 'H' }, { TEXT("HSE"), 'H' }, { TEXT("HSP"), 'H' },	// CHARMM-Histidin
				{ TEXT("HID"), 'H' }, { TEXT("HIE"), 'H' }, { TEXT("HIP"), 'H' },	// AMBER-Histidin
				{ TEXT("CYX"), 'C' }, { TEXT("CYM"), 'C' },						// Cystein, verbrueckt/deprotoniert
				{ TEXT("ASH"), 'D' }, { TEXT("GLH"), 'E' }, { TEXT("LYN"), 'K' },
				{ TEXT("ASX"), 'B' }, { TEXT("GLX"), 'Z' }, { TEXT("UNK"), 'X' },
				{ TEXT("PCA"), 'E' }, { TEXT("HYP"), 'P' }, { TEXT("SEP"), 'S' },
				{ TEXT("TPO"), 'T' }, { TEXT("PTR"), 'Y' }, { TEXT("MLY"), 'K' }
			}, EMolResidueClass::AminoAcid);

			AddAll({
				{ TEXT("DA"), 'A' }, { TEXT("DC"), 'C' }, { TEXT("DG"), 'G' },
				{ TEXT("DT"), 'T' }, { TEXT("DU"), 'U' }, { TEXT("DI"), 'I' },
				{ TEXT("ADE"), 'A' }, { TEXT("CYT"), 'C' }, { TEXT("GUA"), 'G' }, { TEXT("THY"), 'T' }
			}, EMolResidueClass::DeoxyNucleotide);

			AddAll({
				{ TEXT("A"), 'A' }, { TEXT("C"), 'C' }, { TEXT("G"), 'G' },
				{ TEXT("U"), 'U' }, { TEXT("I"), 'I' },
				{ TEXT("RA"), 'A' }, { TEXT("RC"), 'C' }, { TEXT("RG"), 'G' }, { TEXT("RU"), 'U' },
				{ TEXT("URA"), 'U' }, { TEXT("PSU"), 'U' }, { TEXT("5MC"), 'C' }, { TEXT("7MG"), 'G' }
			}, EMolResidueClass::Nucleotide);

			AddAll({
				{ TEXT("HOH"), 'X' }, { TEXT("DOD"), 'X' }, { TEXT("WAT"), 'X' },
				{ TEXT("H2O"), 'X' }, { TEXT("SOL"), 'X' }, { TEXT("TIP"), 'X' }, { TEXT("TIP3"), 'X' }
			}, EMolResidueClass::Water);

			return Map;
		}();
		return Lookup;
	}

	const TSet<FName>& GetProteinBackboneNames()
	{
		static const TSet<FName> Names = { FName("N"), FName("CA"), FName("C"), FName("O"), FName("OXT") };
		return Names;
	}

	const TSet<FName>& GetNucleicBackboneNames()
	{
		static const TSet<FName> Names = {
			FName("P"), FName("OP1"), FName("OP2"), FName("O1P"), FName("O2P"),
			FName("O5'"), FName("C5'"), FName("C4'"), FName("C3'"), FName("O3'"),
			FName("C2'"), FName("C1'"), FName("O4'")
		};
		return Names;
	}
}

namespace MolecularForge
{
	EMolResidueClass ClassifyResidue(FName ResidueName)
	{
		if (const FMolResidueInfo* Found = GetResidueLookup().Find(ResidueName))
		{
			return Found->Class;
		}
		return EMolResidueClass::Other;
	}

	uint8 ResidueOneLetterCode(FName ResidueName)
	{
		if (const FMolResidueInfo* Found = GetResidueLookup().Find(ResidueName))
		{
			return Found->OneLetter;
		}
		return 'X';
	}

	bool IsBackboneAtomName(FName AtomName, EMolResidueClass ResidueClass)
	{
		switch (ResidueClass)
		{
		case EMolResidueClass::AminoAcid:
			return GetProteinBackboneNames().Contains(AtomName);
		case EMolResidueClass::DeoxyNucleotide:
		case EMolResidueClass::Nucleotide:
			return GetNucleicBackboneNames().Contains(AtomName);
		default:
			return false;
		}
	}

	bool IsAnchorAtomName(FName AtomName, EMolResidueClass ResidueClass)
	{
		switch (ResidueClass)
		{
		case EMolResidueClass::AminoAcid:
			return AtomName == FName("CA");
		case EMolResidueClass::DeoxyNucleotide:
		case EMolResidueClass::Nucleotide:
			return AtomName == FName("C1'") || AtomName == FName("P");
		default:
			return false;
		}
	}
}
