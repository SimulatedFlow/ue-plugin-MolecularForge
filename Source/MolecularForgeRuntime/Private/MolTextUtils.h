// Copyright 2026 Silvan Teufel All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Kleine Textwerkzeuge, die sich PDB- und mmCIF-Leser teilen.
 *
 * Alle arbeiten auf FStringView und allozieren nichts. Das ist hier kein Selbstzweck:
 * die Konvertierungsfunktionen laufen in `ParallelFor`-Schleifen ueber hunderttausende
 * Zeilen, und eine FString-Allokation je Feld waere dort der teuerste Einzelposten.
 */
namespace MolecularForge::Text
{
	/**
	 * Zeichenbereich einer Zeile, tolerant gegen zu kurze Zeilen.
	 * PDB ist ein Spaltenformat, aber in freier Wildbahn sind Zeilen oft am Ende
	 * abgeschnitten — jeder Zugriff muss das aushalten, ohne zu lesen, was nicht da ist.
	 */
	inline FStringView SafeSlice(FStringView Line, int32 Start, int32 Length)
	{
		if (Start >= Line.Len() || Start < 0)
		{
			return FStringView();
		}
		const int32 Clamped = FMath::Min(Length, Line.Len() - Start);
		return Line.Mid(Start, Clamped);
	}

	inline FStringView TrimView(FStringView View)
	{
		int32 Start = 0;
		int32 End = View.Len();
		while (Start < End && FChar::IsWhitespace(View[Start])) { ++Start; }
		while (End > Start && FChar::IsWhitespace(View[End - 1])) { --End; }
		return View.Mid(Start, End - Start);
	}

	/**
	 * Kopiert einen View nullterminiert auf den Stack.
	 * FCString::Atof und Atoi brauchen einen Nullterminator, FStringView hat keinen.
	 * Gibt false zurueck, wenn der Abschnitt leer oder unplausibel lang ist.
	 */
	inline bool ToStackBuffer(FStringView View, TCHAR* OutBuffer, int32 BufferSize)
	{
		if (View.IsEmpty() || View.Len() >= BufferSize)
		{
			return false;
		}
		FMemory::Memcpy(OutBuffer, View.GetData(), View.Len() * sizeof(TCHAR));
		OutBuffer[View.Len()] = TEXT('\0');
		return true;
	}

	inline bool ViewToFloat(FStringView View, float& OutValue)
	{
		TCHAR Buffer[32];
		if (!ToStackBuffer(TrimView(View), Buffer, UE_ARRAY_COUNT(Buffer)))
		{
			return false;
		}
		OutValue = FCString::Atof(Buffer);
		return true;
	}

	inline bool ViewToInt(FStringView View, int32& OutValue)
	{
		TCHAR Buffer[32];
		if (!ToStackBuffer(TrimView(View), Buffer, UE_ARRAY_COUNT(Buffer)))
		{
			return false;
		}
		OutValue = FCString::Atoi(Buffer);
		return true;
	}

	/** Fliesskommazahl aus einem festen Spaltenbereich (PDB). */
	inline bool ParseFixedFloat(FStringView Line, int32 Start, int32 Length, float& OutValue)
	{
		return ViewToFloat(SafeSlice(Line, Start, Length), OutValue);
	}

	/** Ganzzahl aus einem festen Spaltenbereich (PDB). */
	inline bool ParseFixedInt(FStringView Line, int32 Start, int32 Length, int32& OutValue)
	{
		return ViewToInt(SafeSlice(Line, Start, Length), OutValue);
	}

	inline uint8 CharAtOrSpace(FStringView Line, int32 Index)
	{
		return (Index >= 0 && Index < Line.Len()) ? static_cast<uint8>(Line[Index]) : static_cast<uint8>(' ');
	}

	inline FName NameFromView(FStringView View)
	{
		const FStringView Trimmed = TrimView(View);
		return Trimmed.IsEmpty() ? NAME_None : FName(Trimmed.Len(), Trimmed.GetData());
	}

	inline FName NameFromSlice(FStringView Line, int32 Start, int32 Length)
	{
		return NameFromView(SafeSlice(Line, Start, Length));
	}
}
