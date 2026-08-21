# Copyright 2026 Simulated Flow All Rights Reserved.
#
# Baut ein Level, das alle Darstellungsarten nebeneinander zeigt.
#
# Zweck ist zweierlei: es ist das Bild, das im Listing am meisten erklaert, und es ist die
# schnellste Pruefung. Band, Oberflaeche und Staebe wurden bis jetzt nie gerendert — ein
# einziger Durchlauf sagt, ob alle vier halten, was der Code verspricht, statt vier.
#
# Aufruf:
#   UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script="<dieser Pfad>" -unattended

import math
import os
import sys

import unreal

sys.path.insert(0, os.path.join(unreal.Paths.project_dir(),
                                "Plugins", "MolecularForge", "Content", "Python"))
import mf_szene

LOG = unreal.log

MAP_PACKAGE = "/MolecularForge/MolecularForge/Maps/L_MF_Comparison"

MAP_FILE = os.path.join(
    unreal.Paths.project_dir(), "Plugins", "MolecularForge",
    "Content", "MolecularForge", "Maps", "L_MF_Comparison.umap")

# Crambin: klein genug, dass auch die Oberflaeche schnell entsteht, und gross genug, dass
# Helix und Faltblatt im Band zu erkennen sind.
STRUCTURE_RELATIVE_PATH = "Plugins/MolecularForge/Demo/1CRN.pdb"

UNITS_PER_ANGSTROM = 10.0

# Abstand der Schaustuecke voneinander. Crambin misst rund 25 Angstroem, also 250
# Einheiten — 380 lassen gerade genug Luft. Enger heisst zugleich: jedes Schaustueck wird
# im fertigen Bild groesser, und darauf kommt es bei einem Vergleichsbild an.
SPACING = 380.0

CAMERA_FOV_DEGREES = 90.0

REPRESENTATIONS = [
    (unreal.MolRepresentation.SPACE_FILLING, unreal.MolColorScheme.ELEMENT, "Space-Filling"),
    (unreal.MolRepresentation.BALL_AND_STICK, unreal.MolColorScheme.ELEMENT, "Ball-and-Stick"),
    (unreal.MolRepresentation.BACKBONE, unreal.MolColorScheme.CHAIN, "Rueckgrat"),
    (unreal.MolRepresentation.CARTOON, unreal.MolColorScheme.SECONDARY_STRUCTURE, "Cartoon"),
    (unreal.MolRepresentation.SURFACE, unreal.MolColorScheme.ELEMENT, "Oberflaeche"),
]


def actors():
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def spawn(actor_class, location, rotation=None, label=None):
    rotation = rotation or unreal.Rotator(0.0, 0.0, 0.0)
    actor = actors().spawn_actor_from_class(actor_class, location, rotation)
    if actor and label:
        actor.set_actor_label(label)
    return actor


def build_lighting():
    key = spawn(unreal.DirectionalLight, unreal.Vector(0.0, 0.0, 1000.0),
                unreal.Rotator(0.0, -50.0, 30.0), "Licht_Haupt")
    if key:
        # Nach dem ersten Schaubild von 6 auf 3,5 gesenkt: die hellen Atome liefen aus.
        key.light_component.set_intensity(3.5)
        key.light_component.set_light_color(unreal.LinearColor(1.0, 0.97, 0.92, 1.0))

    rim = spawn(unreal.DirectionalLight, unreal.Vector(0.0, 0.0, 1000.0),
                unreal.Rotator(0.0, -15.0, -145.0), "Licht_Kante")
    if rim:
        rim.light_component.set_intensity(1.8)
        rim.light_component.set_light_color(unreal.LinearColor(0.55, 0.72, 1.0, 1.0))
        rim.light_component.set_cast_shadows(False)

    sky = spawn(unreal.SkyLight, unreal.Vector(0.0, 0.0, 500.0), None, "Licht_Umgebung")
    if sky:
        sky.light_component.set_intensity(0.7)


def build_specimen(index, representation, color_scheme, label):
    """Ein Schaustueck der Reihe."""
    offset = (index - (len(REPRESENTATIONS) - 1) * 0.5) * SPACING
    actor = spawn(unreal.MolecularStructureActor, unreal.Vector(0.0, offset, 0.0), None,
                  "Schaustueck_%s" % label)
    if not actor:
        LOG("  FEHLER: %s liess sich nicht erzeugen." % label)
        return None

    actor.set_editor_property("StructureFilePath", STRUCTURE_RELATIVE_PATH)
    actor.set_editor_property("bLoadOnBeginPlay", True)
    actor.set_editor_property("Representation", representation)
    actor.set_editor_property("ColorScheme", color_scheme)

    options = actor.get_editor_property("LoadOptions")
    options.set_editor_property("bDiscardWater", True)
    options.set_editor_property("bCenterOnOrigin", True)
    options.set_editor_property("UnitsPerAngstrom", UNITS_PER_ANGSTROM)
    actor.set_editor_property("LoadOptions", options)

    actor.call_method("LoadNow")

    # Nachsehen, ob wirklich etwas entstanden ist. Ein leeres Band faellt im Bild
    # sonst erst auf, wenn man genau hinschaut — und im Zweifel gar nicht.
    atoms = actor.get_atoms_component()
    cartoon = actor.get_cartoon_component()
    surface = actor.get_surface_component()
    bonds = actor.get_bonds_component()

    LOG("  %-14s Kugeln %5d | Staebe %5d | Band %6d Dreiecke | Oberflaeche %6d Dreiecke" % (
        label,
        atoms.get_num_visible_atoms() if atoms else -1,
        bonds.get_num_visible_bonds() if bonds else -1,
        cartoon.get_num_triangles() if cartoon else -1,
        surface.get_num_triangles() if surface else -1))

    if surface and surface.get_last_error():
        LOG("    Oberflaeche meldet: %s" % surface.get_last_error())

    return actor


def build_camera():
    """Blickpunkt, aus dem die ganze Reihe ins Bild passt."""
    # Die Reihe spannt sich zwischen dem ersten und dem letzten Schaustueck auf, nicht
    # ueber alle Abstaende — an den Enden liegt kein halber Abstand mehr.
    row_width = SPACING * (len(REPRESENTATIONS) - 1) + 300.0
    half_fov = math.radians(CAMERA_FOV_DEGREES * 0.5)

    # Achtung, das gilt fuer 16:9 und nur dafuer: Unreal haelt den *senkrechten* Bildwinkel
    # fest und leitet den waagerechten aus dem Seitenverhaeltnis ab. Nimmt man dasselbe
    # Level im Breitformat auf, wird die Reihe nicht beschnitten, sondern kleiner.
    # Nachgemessen mit 1920x640: alles rueckt zusammen.
    #
    # Vorher stand hier ein Faktor 1,75 im Nenner, angeblich gemessen. Er hat den Abstand
    # aber *verkleinert* und damit die Kamera naeher herangeholt, sodass das erste und das
    # letzte Schaustueck aus dem Bild liefen. Wer einen Erfahrungswert einbaut, muss ihn am
    # fertigen Bild nachpruefen — genau das war damals unterblieben.
    distance = (row_width * 0.5) / math.tan(half_fov) * 1.10

    location = unreal.Vector(-distance, 0.0, distance * 0.28)
    rotation = unreal.Rotator(0.0, -14.0, 0.0)

    spawn(unreal.PlayerStart, location, rotation, "Blickpunkt")
    spawn(unreal.CameraActor, location, rotation, "Kamera_Vergleich")

    LOG("  Blickpunkt in %.0f Einheiten Abstand." % distance)


def save_level():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    size_before = os.path.getsize(MAP_FILE) if os.path.exists(MAP_FILE) else 0

    unreal.EditorLoadingAndSavingUtils.save_map(world, MAP_PACKAGE)

    size_after = os.path.getsize(MAP_FILE) if os.path.exists(MAP_FILE) else 0

    # Der Rueckgabewert allein genuegt nicht — siehe Bauplan.
    if size_after > size_before:
        LOG("Level gespeichert: %s (%d Byte)" % (MAP_PACKAGE, size_after))
        return True

    LOG("FEHLER: Die Karte wurde nicht geschrieben.")
    return False


def main():
    LOG("=" * 78)
    LOG("MolecularForge — Vergleichslevel bauen")
    LOG("=" * 78)

    if unreal.EditorAssetLibrary.does_asset_exist(MAP_PACKAGE):
        unreal.EditorAssetLibrary.delete_asset(MAP_PACKAGE)
    if os.path.exists(MAP_FILE):
        os.remove(MAP_FILE)

    existing = actors().get_all_level_actors()
    if existing:
        actors().destroy_actors(existing)

    build_lighting()
    mf_szene.build_exposure_volume()

    for index, (representation, color_scheme, label) in enumerate(REPRESENTATIONS):
        build_specimen(index, representation, color_scheme, label)

    build_camera()
    save_level()

    LOG("=" * 78)


main()
