// Copyright 2026 Silvan Teufel All Rights Reserved.

#include "MolElementTable.h"

namespace
{
	/**
	 * Ordnungszahl -> Stammdaten. Index entspricht der Ordnungszahl, Index 0 ist "unbekannt".
	 *
	 * Werteherkunft: kovalente Radien nach Cordero et al. (2008), Van-der-Waals-Radien nach
	 * Bondi (1964) mit den Ergaenzungen von Alvarez (2013), Farben nach dem Jmol-CPK-Schema.
	 * Fuer die biologisch relevanten Elemente (H..Se, plus Metalle wie Fe, Zn, Mg, Ca, Pt, Au)
	 * sind das die tatsaechlichen Literaturwerte. Fuer die exotischen schweren Elemente,
	 * die in Proteinstrukturen praktisch nie vorkommen, stehen konsistente Naeherungen —
	 * gut genug fuer Darstellung und Bindungsableitung, nicht fuer Chemierechnungen.
	 */
	const FMolElementInfo GElementTable[MolecularForge::MaxAtomicNumber + 1] =
	{
		{ TEXT("X"),  TEXT("Unbekannt"),		1.80f, 1.50f, 255, 105, 180 },
		{ TEXT("H"),  TEXT("Wasserstoff"),	1.20f, 0.31f, 255, 255, 255 },
		{ TEXT("He"), TEXT("Helium"),		1.40f, 0.28f, 217, 255, 255 },
		{ TEXT("Li"), TEXT("Lithium"),		1.82f, 1.28f, 204, 128, 255 },
		{ TEXT("Be"), TEXT("Beryllium"),	1.53f, 0.96f, 194, 255,   0 },
		{ TEXT("B"),  TEXT("Bor"),			1.92f, 0.84f, 255, 181, 181 },
		{ TEXT("C"),  TEXT("Kohlenstoff"),	1.70f, 0.76f, 144, 144, 144 },
		{ TEXT("N"),  TEXT("Stickstoff"),	1.55f, 0.71f,  48,  80, 248 },
		{ TEXT("O"),  TEXT("Sauerstoff"),	1.52f, 0.66f, 255,  13,  13 },
		{ TEXT("F"),  TEXT("Fluor"),		1.47f, 0.57f, 144, 224,  80 },
		{ TEXT("Ne"), TEXT("Neon"),			1.54f, 0.58f, 179, 227, 245 },
		{ TEXT("Na"), TEXT("Natrium"),		2.27f, 1.66f, 171,  92, 242 },
		{ TEXT("Mg"), TEXT("Magnesium"),	1.73f, 1.41f, 138, 255,   0 },
		{ TEXT("Al"), TEXT("Aluminium"),	1.84f, 1.21f, 191, 166, 166 },
		{ TEXT("Si"), TEXT("Silizium"),		2.10f, 1.11f, 240, 200, 160 },
		{ TEXT("P"),  TEXT("Phosphor"),		1.80f, 1.07f, 255, 128,   0 },
		{ TEXT("S"),  TEXT("Schwefel"),		1.80f, 1.05f, 255, 255,  48 },
		{ TEXT("Cl"), TEXT("Chlor"),		1.75f, 1.02f,  31, 240,  31 },
		{ TEXT("Ar"), TEXT("Argon"),		1.88f, 1.06f, 128, 209, 227 },
		{ TEXT("K"),  TEXT("Kalium"),		2.75f, 2.03f, 143,  64, 212 },
		{ TEXT("Ca"), TEXT("Calcium"),		2.31f, 1.76f,  61, 255,   0 },
		{ TEXT("Sc"), TEXT("Scandium"),		2.11f, 1.70f, 230, 230, 230 },
		{ TEXT("Ti"), TEXT("Titan"),		1.87f, 1.60f, 191, 194, 199 },
		{ TEXT("V"),  TEXT("Vanadium"),		1.79f, 1.53f, 166, 166, 171 },
		{ TEXT("Cr"), TEXT("Chrom"),		1.89f, 1.39f, 138, 153, 199 },
		{ TEXT("Mn"), TEXT("Mangan"),		1.97f, 1.39f, 156, 122, 199 },
		{ TEXT("Fe"), TEXT("Eisen"),		1.94f, 1.32f, 224, 102,  51 },
		{ TEXT("Co"), TEXT("Cobalt"),		1.92f, 1.26f, 240, 144, 160 },
		{ TEXT("Ni"), TEXT("Nickel"),		1.84f, 1.24f,  80, 208,  80 },
		{ TEXT("Cu"), TEXT("Kupfer"),		1.86f, 1.32f, 200, 128,  51 },
		{ TEXT("Zn"), TEXT("Zink"),			2.10f, 1.22f, 125, 128, 176 },
		{ TEXT("Ga"), TEXT("Gallium"),		1.87f, 1.22f, 194, 143, 143 },
		{ TEXT("Ge"), TEXT("Germanium"),	2.11f, 1.20f, 102, 143, 143 },
		{ TEXT("As"), TEXT("Arsen"),		1.85f, 1.19f, 189, 128, 227 },
		{ TEXT("Se"), TEXT("Selen"),		1.90f, 1.20f, 255, 161,   0 },
		{ TEXT("Br"), TEXT("Brom"),			1.85f, 1.20f, 166,  41,  41 },
		{ TEXT("Kr"), TEXT("Krypton"),		2.02f, 1.16f,  92, 184, 209 },
		{ TEXT("Rb"), TEXT("Rubidium"),		3.03f, 2.20f, 112,  46, 176 },
		{ TEXT("Sr"), TEXT("Strontium"),	2.49f, 1.95f,   0, 255,   0 },
		{ TEXT("Y"),  TEXT("Yttrium"),		2.32f, 1.90f, 148, 255, 255 },
		{ TEXT("Zr"), TEXT("Zirconium"),	2.23f, 1.75f, 148, 224, 224 },
		{ TEXT("Nb"), TEXT("Niob"),			2.18f, 1.64f, 115, 194, 201 },
		{ TEXT("Mo"), TEXT("Molybdaen"),	2.17f, 1.54f,  84, 181, 181 },
		{ TEXT("Tc"), TEXT("Technetium"),	2.16f, 1.47f,  59, 158, 158 },
		{ TEXT("Ru"), TEXT("Ruthenium"),	2.13f, 1.46f,  36, 143, 143 },
		{ TEXT("Rh"), TEXT("Rhodium"),		2.10f, 1.42f,  10, 125, 140 },
		{ TEXT("Pd"), TEXT("Palladium"),	2.10f, 1.39f,   0, 105, 133 },
		{ TEXT("Ag"), TEXT("Silber"),		2.11f, 1.45f, 192, 192, 192 },
		{ TEXT("Cd"), TEXT("Cadmium"),		2.18f, 1.44f, 255, 217, 143 },
		{ TEXT("In"), TEXT("Indium"),		1.93f, 1.42f, 166, 117, 115 },
		{ TEXT("Sn"), TEXT("Zinn"),			2.17f, 1.39f, 102, 128, 128 },
		{ TEXT("Sb"), TEXT("Antimon"),		2.06f, 1.39f, 158,  99, 181 },
		{ TEXT("Te"), TEXT("Tellur"),		2.06f, 1.38f, 212, 122,   0 },
		{ TEXT("I"),  TEXT("Iod"),			1.98f, 1.39f, 148,   0, 148 },
		{ TEXT("Xe"), TEXT("Xenon"),		2.16f, 1.40f,  66, 158, 176 },
		{ TEXT("Cs"), TEXT("Caesium"),		3.43f, 2.44f,  87,  23, 143 },
		{ TEXT("Ba"), TEXT("Barium"),		2.68f, 2.15f,   0, 201,   0 },
		{ TEXT("La"), TEXT("Lanthan"),		2.43f, 2.07f, 112, 212, 255 },
		{ TEXT("Ce"), TEXT("Cer"),			2.42f, 2.04f, 255, 255, 199 },
		{ TEXT("Pr"), TEXT("Praseodym"),	2.40f, 2.03f, 217, 255, 199 },
		{ TEXT("Nd"), TEXT("Neodym"),		2.39f, 2.01f, 199, 255, 199 },
		{ TEXT("Pm"), TEXT("Promethium"),	2.38f, 1.99f, 163, 255, 199 },
		{ TEXT("Sm"), TEXT("Samarium"),		2.36f, 1.98f, 143, 255, 199 },
		{ TEXT("Eu"), TEXT("Europium"),		2.35f, 1.98f,  97, 255, 199 },
		{ TEXT("Gd"), TEXT("Gadolinium"),	2.34f, 1.96f,  69, 255, 199 },
		{ TEXT("Tb"), TEXT("Terbium"),		2.33f, 1.94f,  48, 255, 199 },
		{ TEXT("Dy"), TEXT("Dysprosium"),	2.31f, 1.92f,  31, 255, 199 },
		{ TEXT("Ho"), TEXT("Holmium"),		2.30f, 1.92f,   0, 255, 156 },
		{ TEXT("Er"), TEXT("Erbium"),		2.29f, 1.89f,   0, 230, 117 },
		{ TEXT("Tm"), TEXT("Thulium"),		2.27f, 1.90f,   0, 212,  82 },
		{ TEXT("Yb"), TEXT("Ytterbium"),	2.26f, 1.87f,   0, 191,  56 },
		{ TEXT("Lu"), TEXT("Lutetium"),		2.24f, 1.87f,   0, 171,  36 },
		{ TEXT("Hf"), TEXT("Hafnium"),		2.23f, 1.75f,  77, 194, 255 },
		{ TEXT("Ta"), TEXT("Tantal"),		2.22f, 1.70f,  77, 166, 255 },
		{ TEXT("W"),  TEXT("Wolfram"),		2.18f, 1.62f,  33, 148, 214 },
		{ TEXT("Re"), TEXT("Rhenium"),		2.16f, 1.51f,  38, 125, 171 },
		{ TEXT("Os"), TEXT("Osmium"),		2.16f, 1.44f,  38, 102, 150 },
		{ TEXT("Ir"), TEXT("Iridium"),		2.13f, 1.41f,  22,  84, 135 },
		{ TEXT("Pt"), TEXT("Platin"),		2.13f, 1.36f, 208, 208, 224 },
		{ TEXT("Au"), TEXT("Gold"),			2.14f, 1.36f, 255, 209,  35 },
		{ TEXT("Hg"), TEXT("Quecksilber"),	2.23f, 1.32f, 184, 184, 208 },
		{ TEXT("Tl"), TEXT("Thallium"),		2.14f, 1.45f, 166,  84,  77 },
		{ TEXT("Pb"), TEXT("Blei"),			2.43f, 1.46f,  87,  89,  97 },
		{ TEXT("Bi"), TEXT("Bismut"),		2.40f, 1.48f, 158,  79, 181 },
		{ TEXT("Po"), TEXT("Polonium"),		2.40f, 1.40f, 171,  92,   0 },
		{ TEXT("At"), TEXT("Astat"),		2.40f, 1.50f, 117,  79,  69 },
		{ TEXT("Rn"), TEXT("Radon"),		2.40f, 1.50f,  66, 130, 150 },
		{ TEXT("Fr"), TEXT("Francium"),		3.48f, 2.60f,  66,   0, 102 },
		{ TEXT("Ra"), TEXT("Radium"),		2.83f, 2.21f,   0, 125,   0 },
		{ TEXT("Ac"), TEXT("Actinium"),		2.47f, 2.15f, 112, 171, 250 },
		{ TEXT("Th"), TEXT("Thorium"),		2.45f, 2.06f,   0, 186, 255 },
		{ TEXT("Pa"), TEXT("Protactinium"),	2.43f, 2.00f,   0, 161, 255 },
		{ TEXT("U"),  TEXT("Uran"),			2.41f, 1.96f,   0, 143, 255 },
		{ TEXT("Np"), TEXT("Neptunium"),	2.39f, 1.90f,   0, 128, 255 },
		{ TEXT("Pu"), TEXT("Plutonium"),	2.43f, 1.87f,   0, 107, 255 },
		{ TEXT("Am"), TEXT("Americium"),	2.44f, 1.80f,  84,  92, 242 },
		{ TEXT("Cm"), TEXT("Curium"),		2.45f, 1.69f, 120,  92, 227 },
		{ TEXT("Bk"), TEXT("Berkelium"),	2.44f, 1.68f, 138,  79, 227 },
		{ TEXT("Cf"), TEXT("Californium"),	2.45f, 1.68f, 161,  54, 212 },
		{ TEXT("Es"), TEXT("Einsteinium"),	2.45f, 1.65f, 179,  31, 212 },
		{ TEXT("Fm"), TEXT("Fermium"),		2.45f, 1.67f, 179,  31, 186 },
		{ TEXT("Md"), TEXT("Mendelevium"),	2.46f, 1.73f, 179,  13, 166 },
		{ TEXT("No"), TEXT("Nobelium"),		2.46f, 1.76f, 189,  13, 135 },
		{ TEXT("Lr"), TEXT("Lawrencium"),	2.46f, 1.61f, 199,   0, 102 },
		{ TEXT("Rf"), TEXT("Rutherfordium"),2.46f, 1.57f, 204,   0,  89 },
		{ TEXT("Db"), TEXT("Dubnium"),		2.46f, 1.49f, 209,   0,  79 },
		{ TEXT("Sg"), TEXT("Seaborgium"),	2.46f, 1.43f, 217,   0,  69 },
		{ TEXT("Bh"), TEXT("Bohrium"),		2.46f, 1.41f, 224,   0,  56 },
		{ TEXT("Hs"), TEXT("Hassium"),		2.46f, 1.34f, 230,   0,  46 },
		{ TEXT("Mt"), TEXT("Meitnerium"),	2.46f, 1.29f, 235,   0,  38 },
		{ TEXT("Ds"), TEXT("Darmstadtium"),	2.46f, 1.28f, 240,   0,  33 },
		{ TEXT("Rg"), TEXT("Roentgenium"),	2.46f, 1.21f, 241,   0,  30 },
		{ TEXT("Cn"), TEXT("Copernicium"),	2.46f, 1.22f, 242,   0,  28 },
		{ TEXT("Nh"), TEXT("Nihonium"),		2.46f, 1.36f, 242,   0,  26 },
		{ TEXT("Fl"), TEXT("Flerovium"),	2.46f, 1.43f, 243,   0,  24 },
		{ TEXT("Mc"), TEXT("Moscovium"),	2.46f, 1.62f, 244,   0,  22 },
		{ TEXT("Lv"), TEXT("Livermorium"),	2.46f, 1.75f, 245,   0,  20 },
		{ TEXT("Ts"), TEXT("Tenness"),		2.46f, 1.65f, 246,   0,  18 },
		{ TEXT("Og"), TEXT("Oganesson"),	2.46f, 1.57f, 247,   0,  16 }
	};

	/**
	 * Standardatomgewichte in atomaren Masseneinheiten, Index gleich Ordnungszahl.
	 * Fuer Elemente ohne stabiles Isotop steht die Massenzahl des langlebigsten.
	 */
	const float GAtomicMasses[MolecularForge::MaxAtomicNumber + 1] =
	{
		  0.000f,
		  1.008f,   4.003f,   6.940f,   9.012f,  10.810f,  12.011f,  14.007f,  15.999f,
		 18.998f,  20.180f,  22.990f,  24.305f,  26.982f,  28.085f,  30.974f,  32.060f,
		 35.450f,  39.950f,  39.098f,  40.078f,  44.956f,  47.867f,  50.942f,  51.996f,
		 54.938f,  55.845f,  58.933f,  58.693f,  63.546f,  65.380f,  69.723f,  72.630f,
		 74.922f,  78.971f,  79.904f,  83.798f,  85.468f,  87.620f,  88.906f,  91.224f,
		 92.906f,  95.950f,  98.000f, 101.070f, 102.906f, 106.420f, 107.868f, 112.414f,
		114.818f, 118.710f, 121.760f, 127.600f, 126.904f, 131.293f, 132.905f, 137.327f,
		138.905f, 140.116f, 140.908f, 144.242f, 145.000f, 150.360f, 151.964f, 157.250f,
		158.925f, 162.500f, 164.930f, 167.259f, 168.934f, 173.045f, 174.967f, 178.486f,
		180.948f, 183.840f, 186.207f, 190.230f, 192.217f, 195.084f, 196.967f, 200.592f,
		204.380f, 207.200f, 208.980f, 209.000f, 210.000f, 222.000f, 223.000f, 226.000f,
		227.000f, 232.038f, 231.036f, 238.029f, 237.000f, 244.000f, 243.000f, 247.000f,
		247.000f, 251.000f, 252.000f, 257.000f, 258.000f, 259.000f, 266.000f, 267.000f,
		268.000f, 269.000f, 270.000f, 269.000f, 278.000f, 281.000f, 282.000f, 285.000f,
		286.000f, 289.000f, 290.000f, 293.000f, 294.000f, 294.000f
	};

	/**
	 * Symbol -> Ordnungszahl. Der Schluessel ist das auf Grossbuchstaben normierte Symbol,
	 * in zwei Bytes gepackt: (erstes Zeichen << 8) | zweites Zeichen, zweites Zeichen 0 bei
	 * einbuchstabigen Symbolen. Damit ist der Lookup ein Integer-Hash ohne String-Allokation,
	 * was in der ParallelFor-Schleife des Parsers den Unterschied macht.
	 */
	const TMap<uint16, uint8>& GetSymbolLookup()
	{
		static const TMap<uint16, uint8> Lookup = []()
		{
			TMap<uint16, uint8> Map;
			Map.Reserve(MolecularForge::MaxAtomicNumber + 1);
			for (int32 Z = 1; Z <= MolecularForge::MaxAtomicNumber; ++Z)
			{
				const TCHAR* Symbol = GElementTable[Z].Symbol;
				const uint16 First = static_cast<uint16>(FChar::ToUpper(Symbol[0]));
				const uint16 Second = Symbol[1] != TEXT('\0') ? static_cast<uint16>(FChar::ToUpper(Symbol[1])) : 0;
				Map.Add(static_cast<uint16>((First << 8) | Second), static_cast<uint8>(Z));
			}
			return Map;
		}();
		return Lookup;
	}

	uint16 PackSymbolKey(TCHAR First, TCHAR Second)
	{
		const uint16 A = static_cast<uint16>(FChar::ToUpper(First));
		const uint16 B = Second != TEXT('\0') ? static_cast<uint16>(FChar::ToUpper(Second)) : 0;
		return static_cast<uint16>((A << 8) | B);
	}
}

namespace MolecularForge
{
	const FMolElementInfo& GetElement(uint8 AtomicNumber)
	{
		return GElementTable[AtomicNumber <= MaxAtomicNumber ? AtomicNumber : 0];
	}

	float GetAtomicMass(uint8 AtomicNumber)
	{
		return GAtomicMasses[AtomicNumber <= MaxAtomicNumber ? AtomicNumber : 0];
	}

	uint8 AtomicNumberFromSymbol(FStringView Symbol)
	{
		// Fuehrende und nachlaufende Leerzeichen weg — PDB schreibt " C" statt "C".
		int32 Start = 0;
		int32 End = Symbol.Len();
		while (Start < End && FChar::IsWhitespace(Symbol[Start])) { ++Start; }
		while (End > Start && FChar::IsWhitespace(Symbol[End - 1])) { --End; }

		const int32 Len = End - Start;
		if (Len <= 0)
		{
			return 0;
		}

		const TMap<uint16, uint8>& Lookup = GetSymbolLookup();

		// Zweibuchstabig zuerst versuchen, damit "CA" nicht als Kohlenstoff durchgeht.
		if (Len >= 2)
		{
			if (const uint8* Found = Lookup.Find(PackSymbolKey(Symbol[Start], Symbol[Start + 1])))
			{
				return *Found;
			}
		}

		if (const uint8* Found = Lookup.Find(PackSymbolKey(Symbol[Start], TEXT('\0'))))
		{
			return *Found;
		}

		return 0;
	}

	uint8 GuessAtomicNumberFromAtomName(FStringView PaddedAtomName, bool bIsHetatm)
	{
		if (PaddedAtomName.Len() < 2)
		{
			return PaddedAtomName.Len() == 1 ? AtomicNumberFromSymbol(PaddedAtomName) : 0;
		}

		const TCHAR C0 = PaddedAtomName[0];
		const TCHAR C1 = PaddedAtomName[1];

		// Spalte 13 leer oder Ziffer: das Element ist einbuchstabig und steht in Spalte 14.
		// Das ist der Normalfall bei Standardresiduen (" CA ", " N  ", " OG1").
		if (FChar::IsWhitespace(C0) || FChar::IsDigit(C0))
		{
			return AtomicNumberFromSymbol(FStringView(&PaddedAtomName[1], 1));
		}

		// Spalte 13 belegt: bei HETATM sind das typischerweise Metalle mit zweibuchstabigem
		// Symbol ("FE  ", "ZN  ", "MG  "). Bei ATOM ist es dagegen fast immer Wasserstoff
		// mit vorangestellter Nummerierung ("HG11", "HB2 ") — dort darf "HG" nicht als
		// Quecksilber gelesen werden.
		if (bIsHetatm)
		{
			if (const uint8 TwoLetter = AtomicNumberFromSymbol(FStringView(&PaddedAtomName[0], 2)))
			{
				return TwoLetter;
			}
		}

		return AtomicNumberFromSymbol(FStringView(&PaddedAtomName[0], 1));
	}
}
