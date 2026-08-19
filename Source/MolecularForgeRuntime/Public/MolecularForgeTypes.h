// Copyright Simulated Flow. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MolecularForgeTypes.generated.h"

MOLECULARFORGERUNTIME_API DECLARE_LOG_CATEGORY_EXTERN(LogMolecularForge, Log, All);

/** Woher eine Struktur stammt. Steuert Attribution und Cache-Verhalten. */
UENUM(BlueprintType)
enum class EMolStructureSource : uint8
{
	Unknown			UMETA(DisplayName = "Unbekannt"),
	LocalFile		UMETA(DisplayName = "Lokale Datei"),
	RcsbPdb			UMETA(DisplayName = "RCSB PDB"),
	AlphaFoldDb		UMETA(DisplayName = "AlphaFold DB"),
	Generated		UMETA(DisplayName = "Erzeugt")
};

/** Art einer Kette. Aus den vorkommenden Residuennamen abgeleitet. */
UENUM(BlueprintType)
enum class EMolChainKind : uint8
{
	Unknown			UMETA(DisplayName = "Unbekannt"),
	Protein			UMETA(DisplayName = "Protein"),
	Dna				UMETA(DisplayName = "DNA"),
	Rna				UMETA(DisplayName = "RNA"),
	Ligand			UMETA(DisplayName = "Ligand"),
	Water			UMETA(DisplayName = "Wasser")
};

/** Sekundaerstruktur eines Residuums. */
UENUM(BlueprintType)
enum class EMolSecondaryStructure : uint8
{
	Coil			UMETA(DisplayName = "Coil"),
	Helix			UMETA(DisplayName = "Helix"),
	Sheet			UMETA(DisplayName = "Faltblatt"),
	Turn			UMETA(DisplayName = "Turn")
};

/** Darstellungsart der Struktur. */
UENUM(BlueprintType)
enum class EMolRepresentation : uint8
{
	/** Jedes Atom als Kugel mit vollem Van-der-Waals-Radius. Zeigt die Oberflaeche. */
	SpaceFilling	UMETA(DisplayName = "Space-Filling (CPK)"),
	/** Kleine Kugeln plus Bindungsstaebe. Zeigt die Konnektivitaet. */
	BallAndStick	UMETA(DisplayName = "Ball-and-Stick"),
	/** Nur das Polymer-Rueckgrat. Fuer grosse Komplexe, bei denen alles andere zumacht. */
	Backbone		UMETA(DisplayName = "Rueckgrat"),
	/** Nur die Ankeratome (CA bzw. C1'). Die guenstigste Uebersichtsdarstellung. */
	AlphaTrace		UMETA(DisplayName = "CA-Spur")
};

/** Faerbeschema fuer die Darstellung. */
UENUM(BlueprintType)
enum class EMolColorScheme : uint8
{
	/** CPK-/Jmol-Farben nach Element. Der Standard in der Strukturbiologie. */
	Element			UMETA(DisplayName = "Element (CPK)"),
	/** Je Kette eine Farbe aus einer wahrnehmungsgleichmaessigen Palette. */
	Chain			UMETA(DisplayName = "Kette"),
	/** Helix rot, Faltblatt gelb, Coil grau — die klassische Cartoon-Faerbung. */
	SecondaryStructure UMETA(DisplayName = "Sekundaerstruktur"),
	/**
	 * B-Faktor bzw. pLDDT. Bei AlphaFold-Strukturen wird automatisch die offizielle
	 * Konfidenzpalette benutzt (dunkelblau > 90, hellblau 70-90, gelb 50-70, orange < 50).
	 */
	BFactor			UMETA(DisplayName = "B-Faktor / pLDDT"),
	/** Hydrophobizitaet nach Kyte-Doolittle: hydrophob orange, hydrophil blau. */
	Hydrophobicity	UMETA(DisplayName = "Hydrophobizitaet"),
	/** Eine einheitliche Farbe fuer die gesamte Struktur. */
	Uniform			UMETA(DisplayName = "Einfarbig")
};

/** Bitflags pro Atom. Kompakt gehalten, damit das SoA-Array klein bleibt. */
enum EMolAtomFlags : uint8
{
	MolAtom_None			= 0,
	/** Stammt aus einem HETATM-Record (Ligand, Ion, Wasser, Modifikation). */
	MolAtom_Hetatm			= 1 << 0,
	/** Teil des Protein-Rueckgrats (N, CA, C, O) bzw. des Nukleinsaeure-Rueckgrats. */
	MolAtom_Backbone		= 1 << 1,
	/** Alpha-Kohlenstoff bzw. C1' — der Ankerpunkt fuer Ribbon-Darstellungen. */
	MolAtom_Anchor			= 1 << 2,
	/** Alternative Konformation, die nicht die primaere ist (altLoc != ' ' und != 'A'). */
	MolAtom_AltLocSecondary	= 1 << 3,
	/** Wassermolekuel (HOH/WAT/DOD). */
	MolAtom_Water			= 1 << 4
};

/**
 * Ein Residuum (Aminosaeure, Nukleotid oder Ligandenmolekuel).
 * Verweist per Index-Spanne in die Atom-Arrays der Struktur — keine eigenen Kopien.
 */
USTRUCT(BlueprintType)
struct MOLECULARFORGERUNTIME_API FMolResidue
{
	GENERATED_BODY()

	/** Dreibuchstaben-Code aus der Datei, z.B. "MET", "HOH", "ATP". */
	UPROPERTY(BlueprintReadOnly, Category = "MolecularForge")
	FName Name;

	/** Sequenznummer aus der Datei. Nicht zwingend luecken- oder duplikatfrei. */
	UPROPERTY(BlueprintReadOnly, Category = "MolecularForge")
	int32 SequenceNumber = 0;

	/** Index der Kette in UMolecularStructure::Chains. */
	UPROPERTY(BlueprintReadOnly, Category = "MolecularForge")
	int32 ChainIndex = INDEX_NONE;

	/** Erster Atomindex dieses Residuums. */
	UPROPERTY(BlueprintReadOnly, Category = "MolecularForge")
	int32 FirstAtom = 0;

	/** Anzahl Atome dieses Residuums. */
	UPROPERTY(BlueprintReadOnly, Category = "MolecularForge")
	int32 NumAtoms = 0;

	UPROPERTY(BlueprintReadOnly, Category = "MolecularForge")
	EMolSecondaryStructure SecondaryStructure = EMolSecondaryStructure::Coil;

	/** Insertion-Code aus Spalte 27. Bei Antikoerpern relevant, sonst meist ' '. */
	UPROPERTY(BlueprintReadOnly, Category = "MolecularForge")
	uint8 InsertionCode = ' ';

	/** Einbuchstaben-Code fuer Sequenzanzeige; 'X' wenn nicht standardisiert. */
	UPROPERTY(BlueprintReadOnly, Category = "MolecularForge")
	uint8 OneLetterCode = 'X';
};

/** Eine Polymerkette bzw. eine Gruppe zusammengehoeriger Heterogruppen. */
USTRUCT(BlueprintType)
struct MOLECULARFORGERUNTIME_API FMolChain
{
	GENERATED_BODY()

	/**
	 * Ketten-ID, z.B. "A".
	 * Als FName und nicht als einzelnes Zeichen, weil mmCIF mehrstellige Bezeichner
	 * erlaubt — in grossen Komplexen wie Ribosomen sind "AA" und "AB" verschiedene
	 * Ketten. Auf ein Zeichen gekuerzt wuerden sie stillschweigend verschmelzen.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "MolecularForge")
	FName Id;

	UPROPERTY(BlueprintReadOnly, Category = "MolecularForge")
	EMolChainKind Kind = EMolChainKind::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "MolecularForge")
	int32 FirstResidue = 0;

	UPROPERTY(BlueprintReadOnly, Category = "MolecularForge")
	int32 NumResidues = 0;

	UPROPERTY(BlueprintReadOnly, Category = "MolecularForge")
	int32 FirstAtom = 0;

	UPROPERTY(BlueprintReadOnly, Category = "MolecularForge")
	int32 NumAtoms = 0;
};

/**
 * Eine kovalente Bindung zwischen zwei Atomen.
 * Indizes verweisen in die Atom-Arrays. Es gilt immer AtomA < AtomB, damit
 * die Liste duplikatfrei und stabil sortierbar ist.
 */
USTRUCT(BlueprintType)
struct MOLECULARFORGERUNTIME_API FMolBond
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "MolecularForge")
	int32 AtomA = 0;

	UPROPERTY(BlueprintReadOnly, Category = "MolecularForge")
	int32 AtomB = 0;

	/** 1 = Einfach, 2 = Doppel, 3 = Dreifach. Abstandsbasiert abgeleitet, daher meist 1. */
	UPROPERTY(BlueprintReadOnly, Category = "MolecularForge")
	uint8 Order = 1;
};

/** Kopfdaten der Struktur. Fuer UI, Attribution und Debug. */
USTRUCT(BlueprintType)
struct MOLECULARFORGERUNTIME_API FMolStructureMeta
{
	GENERATED_BODY()

	/** PDB-Code (z.B. "1CRN") oder UniProt-Accession (z.B. "P69905"). */
	UPROPERTY(BlueprintReadOnly, Category = "MolecularForge")
	FString Identifier;

	UPROPERTY(BlueprintReadOnly, Category = "MolecularForge")
	FString Title;

	/** Experimentelle Methode, z.B. "X-RAY DIFFRACTION", "ELECTRON MICROSCOPY". */
	UPROPERTY(BlueprintReadOnly, Category = "MolecularForge")
	FString ExperimentalMethod;

	/** Aufloesung in Angstroem. 0 wenn nicht angegeben (NMR, Vorhersagen). */
	UPROPERTY(BlueprintReadOnly, Category = "MolecularForge")
	float ResolutionAngstrom = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "MolecularForge")
	EMolStructureSource Source = EMolStructureSource::Unknown;

	/**
	 * Wenn true, ist das B-Faktor-Feld in Wahrheit ein pLDDT-Konfidenzwert (0..100).
	 * AlphaFold-Dateien tun genau das — die Faerbung muss dann die Konfidenzpalette nutzen.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "MolecularForge")
	bool bBFactorIsPLDDT = false;

	/** Anzahl Modelle in der Datei (NMR-Ensembles haben mehrere). Geladen wird derzeit Modell 1. */
	UPROPERTY(BlueprintReadOnly, Category = "MolecularForge")
	int32 NumModelsInFile = 1;

	/** Pflichtangabe bei CC-BY-Quellen. Wird im UI und im Fab-Listing ausgegeben. */
	UPROPERTY(BlueprintReadOnly, Category = "MolecularForge")
	FString Attribution;
};

/** Woher die Sekundaerstruktur kommen soll. */
UENUM(BlueprintType)
enum class EMolSecondaryStructureSource : uint8
{
	/** Nur uebernehmen, was in der Datei steht. Ohne Angabe bleibt alles Coil. */
	FromFile		UMETA(DisplayName = "Nur aus der Datei"),
	/** Angaben der Datei verwerfen und selbst rechnen. */
	Compute			UMETA(DisplayName = "Immer berechnen"),
	/**
	 * Aus der Datei nehmen, und nur rechnen, wenn dort nichts steht.
	 * Der sinnvolle Normalfall: experimentelle Strukturen bringen die vom Autor
	 * hinterlegte Zuordnung mit, AlphaFold-Vorhersagen bringen keine.
	 */
	FromFileElseCompute	UMETA(DisplayName = "Datei, sonst berechnen")
};

/** Steuert, was beim Laden gemacht wird. */
USTRUCT(BlueprintType)
struct MOLECULARFORGERUNTIME_API FMolLoadOptions
{
	GENERATED_BODY()

	/** Wassermolekuele verwerfen. Spart bei Kristallstrukturen oft die Haelfte der Atome. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MolecularForge")
	bool bDiscardWater = true;

	/** Wasserstoffatome verwerfen. In Kristallstrukturen meist ohnehin nicht enthalten. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MolecularForge")
	bool bDiscardHydrogen = false;

	/** Nur die primaere Konformation behalten (altLoc ' ' oder 'A'). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MolecularForge")
	bool bPrimaryAltLocOnly = true;

	/** Bindungen abstandsbasiert ableiten. Ohne das bleibt die Bindungsliste leer. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MolecularForge")
	bool bDeriveBonds = true;

	/** Struktur so verschieben, dass ihr Schwerpunkt im Ursprung liegt. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MolecularForge")
	bool bCenterOnOrigin = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MolecularForge")
	EMolSecondaryStructureSource SecondaryStructureSource = EMolSecondaryStructureSource::FromFileElseCompute;

	/**
	 * Umrechnung Angstroem -> Unreal-Einheiten.
	 * 1 Angstroem = 0.1 nm. Der Standardwert 10 macht aus 1 A = 10 cm, damit ein
	 * mittleres Protein etwa Zimmergroesse hat und mit Standard-Kameras handhabbar ist.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MolecularForge", meta = (ClampMin = "0.01"))
	float UnitsPerAngstrom = 10.f;
};

namespace MolecularForge
{
	/**
	 * Pflichtangabe fuer AlphaFold-Daten. Die Datenbank steht unter CC-BY-4.0, und
	 * Namensnennung ist dort keine Hoeflichkeit, sondern Lizenzbedingung. Der Text steht
	 * hier an einer Stelle, damit er ueberall gleich lautet — Parser, UI und Fab-Listing.
	 */
	MOLECULARFORGERUNTIME_API const FString& GetAlphaFoldAttribution();

	/** Entsprechendes fuer Daten aus dem RCSB-PDB-Archiv. */
	MOLECULARFORGERUNTIME_API const FString& GetPdbAttribution();
}
