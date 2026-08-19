# Copyright Simulated Flow. All Rights Reserved.
#
# Packt das Plugin fuer Fab und prueft vorher, ob es den Anforderungen genuegt.
#
# Die Pruefung ist der eigentliche Zweck. Ein falsch gepacktes Plugin faellt nicht beim
# Packen auf, sondern Wochen spaeter in der Fab-Pruefung — und dann fehlt der Zusammenhang.
# Deshalb wird hier alles geprueft, was schon einmal zurueckgewiesen wurde: fehlendes
# Symbol, fehlende Dateiliste, Bauartefakte im Paket, Inhalt ausserhalb des Einpackordners.
#
# Aufruf (ausserhalb von Unreal):
#   python Tools/build_fab_package.py

import json
import os
import shutil
import zipfile

HIER = os.path.dirname(os.path.abspath(__file__))
PLUGIN = os.path.normpath(os.path.join(HIER, ".."))
NAME = "MolecularForge"

ZIEL_ORDNER = os.path.join(os.path.expanduser("~"), "Desktop", "Fab-Upload-" + NAME)

# Ordner, die nie mitgehen: Bauartefakte und Werkzeugstaende.
VERBOTENE_ORDNER = {"Binaries", "Intermediate", "Saved", ".git", ".vs", "DerivedDataCache"}

# Einzelne Dateien, die nicht zum Kaeufer gehoeren.
VERBOTENE_DATEIEN = {"PLAN.md", ".gitignore"}

# Aus Tools geht nur mit, was der Kaeufer wirklich brauchen kann. Die uebrigen Skripte sind
# einmalige Umbauten oder Diagnose und wuerden beim Kaeufer nur Schaden anrichten.
TOOLS_MITLIEFERN = {
    "build_showcase_level.py",
    "build_comparison_level.py",
    "build_mesoscale_level.py",
    "build_materials.py",
    "mf_szene.py",
}

# Das Video ist Listing-Material und wird bei Fab getrennt hochgeladen; im Plugin waere es
# nur Ballast.
DOCS_AUSLASSEN = {"Video"}

# Einzelne Assets, die nicht mitgehen.
#
# Das Impostor-Material laesst sich nicht uebersetzen ("Failed to compile Material ...,
# Default Material will be used in game"). Ein Material, das beim Kaeufer nur eine
# Fehlerzeile erzeugt, hat im Paket nichts verloren — die Impostoren sind ohnehin
# abgeschaltet. Es bleibt im Quellbestand, damit die angefangene Arbeit nicht verloren ist.
VERBOTENE_ASSETS = {
    "Content/MolecularForge/Materials/M_MF_AtomImpostor.uasset",
}


def soll_mit(relativ):
    teile = relativ.replace("\\", "/").split("/")

    if teile[0] in VERBOTENE_ORDNER:
        return False
    if len(teile) == 1 and teile[0] in VERBOTENE_DATEIEN:
        return False
    if teile[0] == "Tools" and (len(teile) < 2 or teile[1] not in TOOLS_MITLIEFERN):
        return False
    if teile[0] == "Docs" and len(teile) > 1 and teile[1] in DOCS_AUSLASSEN:
        return False
    if relativ.replace("\\", "/") in VERBOTENE_ASSETS:
        return False

    return True


def sammle():
    dateien = []
    for wurzel, ordner, namen in os.walk(PLUGIN):
        ordner[:] = [o for o in ordner if o not in VERBOTENE_ORDNER]
        for n in namen:
            voll = os.path.join(wurzel, n)
            relativ = os.path.relpath(voll, PLUGIN)
            if soll_mit(relativ):
                dateien.append(relativ)
    return sorted(dateien)


def pruefe(dateien):
    """Gibt die Liste der Beanstandungen zurueck. Leer heisst: packbar."""
    fehler = []
    satz = set(d.replace("\\", "/") for d in dateien)

    if "Resources/Icon128.png" not in satz:
        fehler.append("Resources/Icon128.png fehlt — ohne Symbol wird das Plugin abgelehnt.")

    if "Config/FilterPlugin.ini" not in satz:
        fehler.append("Config/FilterPlugin.ini fehlt — Nicht-Asset-Dateien wuerden nicht "
                      "mitgeliefert, die Beispiel-Level blieben beim Kaeufer leer.")

    uplugin = NAME + ".uplugin"
    if uplugin not in satz:
        fehler.append(uplugin + " fehlt.")
    else:
        with open(os.path.join(PLUGIN, uplugin), "r", encoding="utf-8") as f:
            try:
                daten = json.load(f)
            except ValueError as e:
                daten = None
                fehler.append("%s ist kein gueltiges JSON: %s" % (uplugin, e))

        if daten:
            for pflicht in ("FriendlyName", "Description", "Category",
                            "CreatedBy", "EngineVersion", "VersionName"):
                if not daten.get(pflicht):
                    fehler.append("%s: Feld '%s' fehlt oder ist leer." % (uplugin, pflicht))

    # Aller Inhalt muss im Einpackordner liegen.
    for d in satz:
        if d.startswith("Content/") and not d.startswith("Content/%s/" % NAME):
            fehler.append("Inhalt ausserhalb des Einpackordners: " + d)

    for d in satz:
        erste = d.split("/")[0]
        if erste in VERBOTENE_ORDNER:
            fehler.append("Bauartefakt im Paket: " + d)

    if not any(d.startswith("Source/") for d in satz):
        fehler.append("Kein Quelltext im Paket — ein Code-Plugin ohne Source ist unbrauchbar.")

    return fehler


def packe(dateien):
    if os.path.isdir(ZIEL_ORDNER):
        shutil.rmtree(ZIEL_ORDNER)
    os.makedirs(ZIEL_ORDNER)

    archiv = os.path.join(ZIEL_ORDNER, "%s_UE5.8.zip" % NAME)
    with zipfile.ZipFile(archiv, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for relativ in dateien:
            # Im Archiv liegt alles unter einem Ordner mit dem Plugin-Namen, sonst
            # entpackt der Kaeufer den Inhalt in sein Plugins-Verzeichnis statt daneben.
            z.write(os.path.join(PLUGIN, relativ), os.path.join(NAME, relativ))

    # Der Listing-Text und die Bilder wandern daneben, nicht ins Archiv.
    for zusatz in ("DESCRIPTION.md", "ATTRIBUTION.md"):
        shutil.copy2(os.path.join(PLUGIN, zusatz), os.path.join(ZIEL_ORDNER, zusatz))

    listing = os.path.join(ZIEL_ORDNER, "Listing-Material")
    os.makedirs(listing)
    for unterordner in ("Bilder", "Video"):
        quelle = os.path.join(PLUGIN, "Docs", unterordner)
        if os.path.isdir(quelle):
            shutil.copytree(quelle, os.path.join(listing, unterordner))

    return archiv


def main():
    print("=" * 78)
    print("MolecularForge — Fab-Paket bauen")
    print("=" * 78)

    dateien = sammle()
    print("Dateien im Paket: %d" % len(dateien))

    fehler = pruefe(dateien)
    if fehler:
        print("\nNICHT PACKBAR — %d Beanstandung(en):" % len(fehler))
        for f in fehler:
            print("  - " + f)
        return 1

    archiv = packe(dateien)
    groesse = os.path.getsize(archiv)
    print("\nPruefung bestanden.")
    print("Archiv: %s (%.1f MB)" % (archiv, groesse / (1024.0 * 1024.0)))
    print("Listing-Material und Texte liegen daneben in %s" % ZIEL_ORDNER)
    print("\nDer Upload gehoert Silvan — hier wird nichts hochgeladen.")
    return 0


raise SystemExit(main())
