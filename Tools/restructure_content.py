# Copyright 2026 Silvan Teufel All Rights Reserved.
#
# Schiebt den Plugin-Inhalt in den Einpack-Ordner /MolecularForge/MolecularForge/.
#
# Fab verlangt, dass aller Inhalt eines Produkts in genau einem Ordner liegt, der so heisst
# wie das Produkt. Bei einem Code-Plugin heisst der Einhaengepunkt zwar schon
# /MolecularForge/, das genuegt der Pruefung aber nicht — die bereits angenommenen Plugins
# im selben Projekt sind alle so aufgebaut.
#
# Verschoben wird ueber die Editor-Umbenennung und nicht im Dateisystem: nur so werden die
# Verweise aus den Karten auf die Materialien mitgezogen. Die Karten selbst baut hinterher
# ihr jeweiliges Skript neu, das ist billiger und sicherer als sie zu verschieben.
#
# Aufruf:
#   UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script="<dieser Pfad>" -unattended

import unreal

LOG = unreal.log_warning

QUELLE_MATERIALIEN = "/MolecularForge/Materials"
ZIEL_MATERIALIEN = "/MolecularForge/MolecularForge/Materials"

ALTE_KARTEN = "/MolecularForge/Maps"


def verschiebe_materialien():
    if not unreal.EditorAssetLibrary.does_directory_exist(QUELLE_MATERIALIEN):
        LOG("UMBAU Materialien liegen bereits am Ziel.")
        return True

    # Ohne diesen Zwangslauf ist das Verzeichnis im Verzeichnisdienst noch leer, und das
    # Skript meldet froehlich „0 Materialien verschoben", obwohl drei dort liegen. Der
    # Verzeichnisdienst scannt im Kommandozeilenbetrieb sonst nebenher.
    unreal.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous(
        [QUELLE_MATERIALIEN], force_rescan=True)

    unreal.EditorAssetLibrary.make_directory(ZIEL_MATERIALIEN)

    bewegt = 0
    for pfad in unreal.EditorAssetLibrary.list_assets(QUELLE_MATERIALIEN, recursive=False):
        name = pfad.split("/")[-1].split(".")[0]
        ziel = "%s/%s" % (ZIEL_MATERIALIEN, name)
        if unreal.EditorAssetLibrary.rename_asset(pfad, ziel):
            LOG("UMBAU %s -> %s" % (name, ziel))
            bewegt += 1
        else:
            LOG("UMBAU FEHLER bei %s" % name)

    unreal.EditorAssetLibrary.delete_directory(QUELLE_MATERIALIEN)
    LOG("UMBAU %d Materialien verschoben." % bewegt)
    return bewegt > 0


def entferne_alte_karten():
    """Die Karten werden von ihren Bauskripten neu erzeugt, nicht verschoben."""
    if unreal.EditorAssetLibrary.does_directory_exist(ALTE_KARTEN):
        unreal.EditorAssetLibrary.delete_directory(ALTE_KARTEN)
        LOG("UMBAU alte Kartenablage entfernt.")


verschiebe_materialien()
entferne_alte_karten()
unreal.EditorAssetLibrary.save_directory("/MolecularForge", only_if_is_dirty=False, recursive=True)
LOG("UMBAU fertig.")
