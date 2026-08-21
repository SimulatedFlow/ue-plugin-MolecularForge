# Copyright 2026 Simulated Flow All Rights Reserved.
#
# Prueft den Parser gegen echte Archivdaten.
#
# Die Automationstests arbeiten mit selbstgeschriebenen Ausschnitten — noetig, damit sie
# ohne Netz laufen, aber sie koennen nur pruefen, was ich beim Schreiben bedacht habe.
# Eine echte Datei aus dem Archiv enthaelt alles, was in dreissig Jahren Formatgeschichte
# hineingeraten ist. Deshalb dieser zweite Weg.
#
# Aufruf (headless):
#   UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script="<dieser Pfad>" -unattended -nullrhi

import os
import unreal

DEMO_DIR = os.path.join(
    unreal.Paths.project_dir(), "Plugins", "MolecularForge", "Demo")

LOG = unreal.log


def make_options(ss_source=None, discard_water=True):
    options = unreal.MolLoadOptions()
    options.set_editor_property("bDiscardWater", discard_water)
    options.set_editor_property("bCenterOnOrigin", True)
    options.set_editor_property("bDeriveBonds", True)
    if ss_source is not None:
        options.set_editor_property("SecondaryStructureSource", ss_source)
    return options


_SELECT_SIGNATURE_LOGGED = [False]


def select(structure, expression):
    """Gibt (Erfolg, Indexliste, Fehlertext) zurueck.

    Wie viele Werte die Python-Bindung zurueckreicht, haengt davon ab, wie Unreal
    Rueckgabewert und Ausgabeparameter zusammenfasst. Statt das zu raten, wird die
    Form einmal protokolliert und danach beides bedient.
    """
    result = unreal.MolecularForgeLibrary.select_atoms(structure, expression)

    if not _SELECT_SIGNATURE_LOGGED[0]:
        LOG("    (Form der Auswahl-Rueckgabe: %s)" % repr(type(result)))
        _SELECT_SIGNATURE_LOGGED[0] = True

    if isinstance(result, tuple):
        if len(result) == 3:
            return result[0], result[1], result[2]
        if len(result) == 2:
            # Rueckgabewert wurde weggelassen: Erfolg ergibt sich aus dem leeren Fehlertext.
            indices, error = result
            return (not error), indices, error

    return False, [], "Unerwartete Rueckgabeform."


def count(structure, expression):
    ok, indices, error = select(structure, expression)
    if not ok:
        LOG("    AUSWAHL FEHLGESCHLAGEN '%s': %s" % (expression, error))
        return -1
    return len(indices)


def describe(path, options):
    structure, error = unreal.MolecularForgeLibrary.load_structure_from_file(None, path, options)
    if structure is None:
        LOG("  FEHLER: %s" % error)
        return None
    return structure


def check(name, condition, detail=""):
    status = "OK   " if condition else "FEHLER"
    LOG("    [%s] %s %s" % (status, name, detail))
    return condition


def main():
    LOG("=" * 78)
    LOG("MolecularForge — Pruefung gegen echte Strukturdaten")
    LOG("=" * 78)

    failures = 0

    for pdb_id, expectation in [
        # Crambin: winzig, klassisches Testobjekt, gemischt Helix und Faltblatt.
        ("1CRN", dict(min_atoms=300, max_atoms=500, chains=1)),
        # GFP: Fass aus elf Faltblattstraengen. Der Grund, warum es hier steht:
        # die Faltblatt-Erkennung war bisher nur durch Code-Lektuere gedeckt.
        ("1EMA", dict(min_atoms=1700, max_atoms=2200, chains=1)),
        # Haemoglobin: vier Ketten, vier Haemgruppen mit Eisen, fast reine Helix.
        ("4HHB", dict(min_atoms=4300, max_atoms=4900, chains=4)),
    ]:
        LOG("")
        LOG("-" * 78)
        LOG("%s" % pdb_id)
        LOG("-" * 78)

        for extension in ("pdb", "cif"):
            path = os.path.join(DEMO_DIR, "%s.%s" % (pdb_id, extension))
            if not os.path.exists(path):
                LOG("  %s: Datei fehlt" % extension)
                failures += 1
                continue

            structure = describe(path, make_options())
            if structure is None:
                failures += 1
                continue

            atoms = structure.get_num_atoms()
            residues = structure.get_num_residues()
            chains = structure.get_num_chains()
            bonds = structure.get_num_bonds()

            LOG("  %s: %d Atome, %d Residuen, %d Ketten, %d Bindungen"
                % (extension, atoms, residues, chains, bonds))

            if not check("Atomzahl plausibel",
                         expectation["min_atoms"] <= atoms <= expectation["max_atoms"],
                         "(%d)" % atoms):
                failures += 1

            # Bindungen: in einem Protein hat fast jedes Schweratom eine bis vier.
            # Deutlich weniger hiesse, die Ableitung greift nicht.
            if not check("Bindungen abgeleitet", bonds > atoms * 0.8, "(%d)" % bonds):
                failures += 1

        # ---- Kreuzvergleich der beiden Formate ----
        # Dieselbe Struktur, zwei Formate, echte Archivdateien: die Ergebnisse muessen
        # uebereinstimmen. Das ist derselbe Test wie in der Automation, nur mit Daten,
        # die niemand fuer den Test zurechtgelegt hat.
        from_pdb = describe(os.path.join(DEMO_DIR, "%s.pdb" % pdb_id), make_options())
        from_cif = describe(os.path.join(DEMO_DIR, "%s.cif" % pdb_id), make_options())

        if from_pdb and from_cif:
            same_atoms = from_pdb.get_num_atoms() == from_cif.get_num_atoms()
            if not check("PDB und mmCIF liefern dieselbe Atomzahl", same_atoms,
                         "(%d gegen %d)" % (from_pdb.get_num_atoms(), from_cif.get_num_atoms())):
                failures += 1

        # ---- Sekundaerstruktur: Datei gegen eigene Rechnung ----
        path = os.path.join(DEMO_DIR, "%s.pdb" % pdb_id)

        from_file = describe(path, make_options(
            unreal.MolSecondaryStructureSource.FROM_FILE))
        computed = describe(path, make_options(
            unreal.MolSecondaryStructureSource.COMPUTE))

        if from_file and computed:
            file_helix = count(from_file, "ss H")
            file_sheet = count(from_file, "ss S")
            our_helix = count(computed, "ss H")
            our_sheet = count(computed, "ss S")
            total = from_file.get_num_atoms()

            LOG("  Sekundaerstruktur (Atome von %d):" % total)
            LOG("    aus der Datei:  Helix %d, Faltblatt %d" % (file_helix, file_sheet))
            LOG("    selbst gerechnet: Helix %d, Faltblatt %d" % (our_helix, our_sheet))

            # Verglichen wird die Groessenordnung und nicht die genaue Zahl: die Angabe
            # in der Datei stammt vom Autor der Struktur und weicht auch zwischen
            # etablierten Programmen um einige Residuen ab. Was zaehlt, ist, dass ein
            # helixreiches Protein als helixreich und ein Fass aus Faltblaettern als
            # faltblattreich herauskommt.
            if file_helix > total * 0.2:
                if not check("Helixreiche Struktur wird als solche erkannt",
                             our_helix > total * 0.15,
                             "(%d von %d)" % (our_helix, total)):
                    failures += 1

            if file_sheet > total * 0.15:
                if not check("Faltblattreiche Struktur wird als solche erkannt",
                             our_sheet > total * 0.10,
                             "(%d von %d)" % (our_sheet, total)):
                    failures += 1

    # ---- Auswahl und Messung an echten Daten ----
    LOG("")
    LOG("-" * 78)
    LOG("Auswahl und Messung an 4HHB")
    LOG("-" * 78)

    hemoglobin = describe(os.path.join(DEMO_DIR, "4HHB.pdb"),
                          make_options(discard_water=False))

    if hemoglobin:
        for expression, minimum in [
            ("protein", 4000),
            ("water", 100),
            ("chain A", 900),
            ("element Fe", 4),          # vier Haemgruppen, je ein Eisen
            ("resn HEM", 150),
            ("within 3 of element Fe", 4),
            ("backbone and chain A", 400),
        ]:
            found = count(hemoglobin, expression)
            if not check("'%s'" % expression, found >= minimum, "-> %d" % found):
                failures += 1

        # Das Eisen im Haem ist an ein Histidin der Proteinkette gebunden. Der Abstand
        # liegt bei rund 2,1 Angstroem — eine Zahl aus der Literatur, die sich hier
        # nachmessen laesst.
        ok, iron, _ = select(hemoglobin, "element Fe")
        ok2, his_ne2, _ = select(hemoglobin, "resn HIS and name NE2")

        if ok and ok2 and iron and his_ne2:
            shortest = 999.0
            for fe in iron:
                for ne2 in his_ne2:
                    distance = unreal.MolecularForgeLibrary.measure_distance(hemoglobin, fe, ne2)
                    if 0.0 < distance < shortest:
                        shortest = distance

            LOG("    kuerzester Fe-NE2-Abstand: %.2f A" % shortest)
            if not check("Eisen-Histidin-Bindung liegt bei rund 2,1 A",
                         1.8 < shortest < 2.6, "(%.2f A)" % shortest):
                failures += 1

    LOG("")
    LOG("=" * 78)
    if failures == 0:
        LOG("ERGEBNIS: alles bestanden")
    else:
        LOG("ERGEBNIS: %d Pruefungen fehlgeschlagen" % failures)
    LOG("=" * 78)


main()
