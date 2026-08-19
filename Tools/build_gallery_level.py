# Copyright Simulated Flow. All Rights Reserved.
#
# Baut ein Level mit *einer* Darstellungsart, formatfuellend — die Vorlage fuer die
# Galeriebilder des Listings.
#
# Das Vergleichslevel zeigt alle fuenf nebeneinander und erklaert damit den Umfang; es zeigt
# aber keine einzige davon so gross, dass man die Machart beurteilen koennte. Dafuer ist
# dieses Level da: dieselbe Struktur, dieselbe Beleuchtung, nur die Darstellung wechselt.
# Gleiche Bedingungen sind hier kein Selbstzweck — nur so sagt der Bildvergleich etwas ueber
# die Darstellung und nicht ueber die Ausleuchtung.
#
# Welche Art gebaut wird, kommt aus der Umgebungsvariablen MF_DARSTELLUNG. Der Weg ueber die
# Umgebung statt ueber Aufrufparameter ist gewaehlt, weil das Python-Kommandlet keine
# eigenen Argumente durchreicht.
#
# Aufruf:
#   $env:MF_DARSTELLUNG = "CARTOON"
#   UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script="<dieser Pfad>" -unattended

import math
import os
import sys

import unreal

sys.path.insert(0, os.path.join(unreal.Paths.project_dir(),
                                "Plugins", "MolecularForge", "Tools"))
import mf_szene

LOG = unreal.log

MAP_PACKAGE = "/MolecularForge/MolecularForge/Maps/L_MF_Gallery"

MAP_FILE = os.path.join(
    unreal.Paths.project_dir(), "Plugins", "MolecularForge",
    "Content", "MolecularForge", "Maps", "L_MF_Gallery.umap")

# GFP fuer alle fuenf Bilder. Klein genug, dass auch die Oberflaeche in vertretbarer Zeit
# entsteht, und als Faltung dankbar: das Fassmotiv ist im Band sofort zu erkennen, und in
# der Kalottendarstellung sieht man eine geschlossene Oberflaeche statt eines Drahtknaeuels.
STRUCTURE_RELATIVE_PATH = "Plugins/MolecularForge/Demo/1EMA.pdb"

UNITS_PER_ANGSTROM = 10.0
CAMERA_FOV_DEGREES = 90.0

# Wie viel der Bildbreite die Struktur einnimmt. Groesser als im Schaulevel — hier ist die
# Struktur das ganze Motiv und muss nicht mit Beiwerk teilen.
CAMERA_FILL_FRACTION = 0.62

# Name -> (Darstellung, Farbschema, Dateiname).
DARSTELLUNGEN = {
    "SPACE_FILLING": (unreal.MolRepresentation.SPACE_FILLING,
                      unreal.MolColorScheme.ELEMENT, "10_SpaceFilling"),
    "BALL_AND_STICK": (unreal.MolRepresentation.BALL_AND_STICK,
                       unreal.MolColorScheme.ELEMENT, "11_BallAndStick"),
    "BACKBONE": (unreal.MolRepresentation.BACKBONE,
                 unreal.MolColorScheme.CHAIN, "12_Rueckgrat"),
    "CARTOON": (unreal.MolRepresentation.CARTOON,
                unreal.MolColorScheme.SECONDARY_STRUCTURE, "13_Cartoon"),
    "SURFACE": (unreal.MolRepresentation.SURFACE,
                unreal.MolColorScheme.ELEMENT, "14_Oberflaeche"),
}


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
                unreal.Rotator(0.0, -45.0, 35.0), "Licht_Haupt")
    if key:
        key.light_component.set_intensity(3.5)
        key.light_component.set_light_color(unreal.LinearColor(1.0, 0.97, 0.92, 1.0))

    rim = spawn(unreal.DirectionalLight, unreal.Vector(0.0, 0.0, 1000.0),
                unreal.Rotator(0.0, -20.0, -140.0), "Licht_Kante")
    if rim:
        rim.light_component.set_intensity(2.0)
        rim.light_component.set_light_color(unreal.LinearColor(0.6, 0.75, 1.0, 1.0))
        rim.light_component.set_cast_shadows(False)

    sky = spawn(unreal.SkyLight, unreal.Vector(0.0, 0.0, 500.0), None, "Licht_Umgebung")
    if sky:
        sky.light_component.set_intensity(0.6)


def build_specimen(representation, color_scheme):
    actor = spawn(unreal.MolecularStructureActor, unreal.Vector(0.0, 0.0, 0.0),
                  None, "Schaustueck")
    if not actor:
        LOG("FEHLER: MolecularStructureActor liess sich nicht erzeugen.")
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

    # Nur fuers Schaubild: die Vorgabe -2,3 ist der in der Literatur uebliche Wert und
    # bleibt die Voreinstellung des Plugins, zeichnet aber jedes Atom einzeln nach. Fuer
    # ein Bild, das die Oberflaeche als Oberflaeche zeigen soll, ist das die falsche Wahl.
    # Muss vor dem Laden stehen — das Netz entsteht waehrend LoadNow.
    if representation == unreal.MolRepresentation.SURFACE:
        surface = actor.get_surface_component()
        if surface:
            surface.set_editor_property("Blobbiness", -1.5)

    actor.call_method("LoadNow")

    atoms = actor.get_atoms_component()
    bonds = actor.get_bonds_component()
    cartoon = actor.get_cartoon_component()
    surface = actor.get_surface_component()

    LOG("  Kugeln %5d | Staebe %5d | Band %7d Dreiecke | Oberflaeche %7d Dreiecke" % (
        atoms.get_num_visible_atoms() if atoms else -1,
        bonds.get_num_visible_bonds() if bonds else -1,
        cartoon.get_num_triangles() if cartoon else -1,
        surface.get_num_triangles() if surface else -1))

    if surface and surface.get_last_error():
        LOG("  Oberflaeche meldet: %s" % surface.get_last_error())

    return actor


def build_camera(actor):
    """
    Bildgrenze an der tatsaechlichen Huelle des Actors, nicht an den Atomkoordinaten.

    Der Unterschied ist nicht klein: die Oberflaeche liegt einen Sondenradius ausserhalb
    der Atommittelpunkte, und das Band schwingt ueber die Ruecgratatome hinaus. Mit den
    Atomgrenzen gerechnet war die Oberflaeche oben angeschnitten — und zwar genau bei der
    Darstellung, die am meisten Platz braucht.
    """
    radius_units = 450.0

    if actor:
        _, extent = actor.get_actor_bounds(only_colliding_components=False)
        gemessen = max(extent.x, extent.y, extent.z)
        if gemessen > 1.0:
            radius_units = gemessen

    half_fov = math.radians(CAMERA_FOV_DEGREES * 0.5)
    distance = radius_units / (math.tan(half_fov) * CAMERA_FILL_FRACTION)

    location = unreal.Vector(-distance * 0.94, -distance * 0.26, distance * 0.20)
    rotation = unreal.MathLibrary.find_look_at_rotation(location, unreal.Vector(0.0, 0.0, 0.0))

    spawn(unreal.PlayerStart, location, rotation, "Blickpunkt")
    spawn(unreal.CameraActor, location, rotation, "Kamera_Galerie")

    LOG("  Strukturradius %.0f -> Abstand %.0f" % (radius_units, distance))


def save_level():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    size_before = os.path.getsize(MAP_FILE) if os.path.exists(MAP_FILE) else 0

    unreal.EditorLoadingAndSavingUtils.save_map(world, MAP_PACKAGE)

    size_after = os.path.getsize(MAP_FILE) if os.path.exists(MAP_FILE) else 0
    if size_after > 0 and size_after != size_before:
        LOG("Level gespeichert: %s (%d Byte)" % (MAP_PACKAGE, size_after))
        return True

    LOG("FEHLER: Die Karte wurde nicht geschrieben.")
    return False


def main():
    schluessel = os.environ.get("MF_DARSTELLUNG", "").strip().upper()
    if schluessel not in DARSTELLUNGEN:
        LOG("FEHLER: MF_DARSTELLUNG ist '%s', erlaubt sind: %s"
            % (schluessel, ", ".join(sorted(DARSTELLUNGEN))))
        return

    representation, color_scheme, dateiname = DARSTELLUNGEN[schluessel]

    LOG("=" * 78)
    LOG("MolecularForge — Galeriebild '%s' (%s)" % (schluessel, dateiname))
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
    actor = build_specimen(representation, color_scheme)
    build_camera(actor)
    save_level()

    LOG("BILDNAME %s" % dateiname)
    LOG("=" * 78)


main()
