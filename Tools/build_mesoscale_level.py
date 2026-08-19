# Copyright Simulated Flow. All Rights Reserved.
#
# Baut die Demo der mesoskopischen Ebene: eine Molekuelpopulation, die diffundiert und
# aneinander andockt.
#
# Das ist der Teil aus Phase 4, der bis jetzt nur Einheitentests gesehen hat. Diffusion,
# Randbedingung, Bindung, Detailstufen und der gestaffelte Renderer laufen hier zum ersten
# Mal wirklich — und zwar zusammen.
#
# Aufruf:
#   UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script="<dieser Pfad>" -unattended

import math
import os
import sys

import unreal

sys.path.insert(0, os.path.join(unreal.Paths.project_dir(),
                                "Plugins", "MolecularForge", "Tools"))
import mf_szene

LOG = unreal.log

MAP_PACKAGE = "/MolecularForge/Maps/L_MF_Mesoscale"

MAP_FILE = os.path.join(
    unreal.Paths.project_dir(), "Plugins", "MolecularForge",
    "Content", "Maps", "L_MF_Mesoscale.umap")

# Halbe Kantenlaengen des Raums, in dem die Molekuele wimmeln.
#
# Die Groesse ist nicht beliebig, sondern folgt aus der Belegung. 260 Molekuele mit 22 A
# Radius und 520 mit 9 A fuellen zusammen rund 1,3e7 A^3. In einem Raum von 360x360x200 A
# waeren das ueber die Haelfte des Volumens — dichter als jedes Zellinnere und im Bild eine
# geschlossene Flaeche. Bei 600x600x240 A sind es rund 15 %: dicht gedraengt, aber man sieht
# noch einzelne Molekuele und Zwischenraum.
BOUNDS_EXTENT = unreal.Vector(3000.0, 3000.0, 1200.0)

# Der eingestellte Wert ist der *waagerechte* Bildwinkel. Fuers Einpassen zaehlt der
# senkrechte, denn er ist der kleinere — im Vergleichslevel hat genau das gefehlt.
CAMERA_FOV_DEGREES = 90.0
CAMERA_ASPECT = 16.0 / 9.0


def actors():
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def spawn(actor_class, location, rotation=None, label=None):
    rotation = rotation or unreal.Rotator(0.0, 0.0, 0.0)
    actor = actors().spawn_actor_from_class(actor_class, location, rotation)
    if actor and label:
        actor.set_actor_label(label)
    return actor


def build_lighting():
    key = spawn(unreal.DirectionalLight, unreal.Vector(0.0, 0.0, 2000.0),
                unreal.Rotator(0.0, -55.0, 25.0), "Licht_Haupt")
    if key:
        # Niedriger als in den anderen Levels. Hier stehen hunderte helle Kugeln dicht
        # beieinander und werfen sich gegenseitig Licht zu; mit 3,5 brannte das Bild aus.
        key.light_component.set_intensity(2.2)

    rim = spawn(unreal.DirectionalLight, unreal.Vector(0.0, 0.0, 2000.0),
                unreal.Rotator(0.0, -10.0, -150.0), "Licht_Kante")
    if rim:
        rim.light_component.set_intensity(1.3)
        rim.light_component.set_light_color(unreal.LinearColor(0.45, 0.68, 1.0, 1.0))
        rim.light_component.set_cast_shadows(False)

    sky = spawn(unreal.SkyLight, unreal.Vector(0.0, 0.0, 500.0), None, "Licht_Umgebung")
    if sky:
        sky.light_component.set_intensity(0.5)


def build_spawner():
    spawner = spawn(unreal.MolecularMesoSpawner, unreal.Vector(0.0, 0.0, 0.0), None, "Population")
    if not spawner:
        LOG("FEHLER: Spawner liess sich nicht erzeugen.")
        return None

    # Zwei Arten, damit im Bild Groessenunterschiede zu sehen sind — und damit die
    # Bindung zwischen ungleichen Partnern etwas zu tun bekommt.
    gross = unreal.MolMesoSpecies()
    gross.set_editor_property("Name", "Grosses Protein")
    gross.set_editor_property("Count", 260)
    gross.set_editor_property("ContactRadiusAngstrom", 22.0)

    klein = unreal.MolMesoSpecies()
    klein.set_editor_property("Name", "Ligand")
    klein.set_editor_property("Count", 520)
    klein.set_editor_property("ContactRadiusAngstrom", 9.0)

    spawner.set_editor_property("Species", [gross, klein])
    spawner.set_editor_property("bSpawnOnBeginPlay", True)
    spawner.set_editor_property("RandomSeed", 20260819)

    parameters = spawner.get_editor_property("Parameters")
    parameters.set_editor_property("BoundsCenter", unreal.Vector(0.0, 0.0, 0.0))
    parameters.set_editor_property("BoundsExtent", BOUNDS_EXTENT)
    parameters.set_editor_property("UnitsPerAngstrom", 10.0)
    parameters.set_editor_property("BoundaryMode", unreal.MolBoundaryMode.REFLECT)

    # Sichtbar langsam. Der Literaturwert waere rund 10^8 und ergaebe Rauschen.
    parameters.set_editor_property("DiffusionCoefficient", 250.0)
    parameters.set_editor_property("bEnableBinding", True)
    parameters.set_editor_property("BindProbabilityPerSecond", 3.0)
    parameters.set_editor_property("UnbindProbabilityPerSecond", 0.4)

    # Die Stufengrenzen muessen zum Kameraabstand passen, sonst faellt die ganze Population
    # in eine einzige Stufe und die Staffelung liefe zwar, waere aber nicht nachweisbar.
    # Die mittlere Grenze liegt auf dem Kameraabstand selbst: dann steht ungefaehr die
    # vordere Haelfte des Raums in der mittleren Stufe und die hintere in der fernen.
    distance = camera_distance()
    parameters.set_editor_property("FullDetailDistance", 300.0)
    parameters.set_editor_property("BackboneDistance", distance)
    parameters.set_editor_property("BlobDistance", distance * 4.0)

    spawner.set_editor_property("Parameters", parameters)

    LOG("  Population: 260 grosse + 520 kleine Molekuele.")
    return spawner


def build_renderer():
    renderer = spawn(unreal.MolecularMesoRenderer, unreal.Vector(0.0, 0.0, 0.0), None, "Darstellung")
    if not renderer:
        LOG("FEHLER: Renderer liess sich nicht erzeugen.")
        return None

    renderer.set_editor_property("UnitsPerAngstrom", 10.0)

    # Die Artenliste des Renderers bleibt leer. Sie erwartet geladene Strukturen, und
    # die sind keine Assets — sie entstehen erst zur Laufzeit beim Einlesen einer Datei.
    # Ohne sie faellt die nahe Stufe auf das Ersatzmesh zurueck, was fuer diese Demo
    # genuegt. Siehe die offenen Punkte im Bauplan.
    renderer.set_editor_property("MaxFullDetailMolecules", 0)

    LOG("  Renderer gesetzt.")
    return renderer


def camera_distance():
    """
    Abstand, bei dem der ganze Raum ins Bild passt — gemessen an der Huelle, nicht am
    Mittelpunkt.

    Der vorige Versuch hat den Abstand aus der Kantenlaenge gerechnet und die Kamera damit
    bei 1895 Einheiten in einen Raum gestellt, der bis 1800 reicht. Sie stand also *in* der
    Wolke, und das Bild zeigte folgerichtig eine weisse Flaeche aus naechster Naehe.
    Richtig ist die Umkugel: der Blickkegel muss eine Kugel vom Radius der Raumdiagonale
    fassen, und dafuer gilt Abstand = Radius / sin(halber Bildwinkel).
    """
    half_h = math.radians(CAMERA_FOV_DEGREES * 0.5)
    half_v = math.atan(math.tan(half_h) / CAMERA_ASPECT)

    radius = math.sqrt(BOUNDS_EXTENT.x ** 2 + BOUNDS_EXTENT.y ** 2 + BOUNDS_EXTENT.z ** 2)

    # Der Kasten ist keine Kugel; schraeg gesehen ist seine Silhouette deutlich kleiner als
    # die Umkugel — und weil er flach ist, faellt der Unterschied hier gross aus. Der Faktor
    # holt das wieder heran, ohne den Rand anzuschneiden; nachgemessen am fertigen Bild.
    return radius / math.sin(half_v) * 0.68


def build_camera():
    distance = camera_distance()
    radius = math.sqrt(BOUNDS_EXTENT.x ** 2 + BOUNDS_EXTENT.y ** 2 + BOUNDS_EXTENT.z ** 2)

    # Von schraeg oben und seitlich, damit der Raum Tiefe bekommt.
    direction = unreal.Vector(-0.86, -0.36, 0.36)
    length = math.sqrt(direction.x ** 2 + direction.y ** 2 + direction.z ** 2)
    location = unreal.Vector(direction.x / length * distance,
                             direction.y / length * distance,
                             direction.z / length * distance)

    # Nicht raten, sondern zum Mittelpunkt hin ausrichten.
    rotation = unreal.MathLibrary.find_look_at_rotation(location, unreal.Vector(0.0, 0.0, 0.0))

    spawn(unreal.PlayerStart, location, rotation, "Blickpunkt")
    spawn(unreal.CameraActor, location, rotation, "Kamera_Mesoskala")

    LOG("  Blickpunkt in %.0f Einheiten Abstand (Raumdiagonale %.0f) — %s ausserhalb."
        % (distance, radius, "liegt" if distance > radius else "liegt NICHT"))
    return distance


def save_level():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    size_before = os.path.getsize(MAP_FILE) if os.path.exists(MAP_FILE) else 0

    unreal.EditorLoadingAndSavingUtils.save_map(world, MAP_PACKAGE)

    size_after = os.path.getsize(MAP_FILE) if os.path.exists(MAP_FILE) else 0
    if size_after > size_before:
        LOG("Level gespeichert: %s (%d Byte)" % (MAP_PACKAGE, size_after))
        return True

    LOG("FEHLER: Die Karte wurde nicht geschrieben.")
    return False


def main():
    LOG("=" * 78)
    LOG("MolecularForge — Mesoskala-Demo bauen")
    LOG("=" * 78)

    if unreal.EditorAssetLibrary.does_asset_exist(MAP_PACKAGE):
        unreal.EditorAssetLibrary.delete_asset(MAP_PACKAGE)
    if os.path.exists(MAP_FILE):
        os.remove(MAP_FILE)

    existing = actors().get_all_level_actors()
    if existing:
        actors().destroy_actors(existing)

    build_lighting()
    # Etwas heller als die anderen beiden Level: das Motiv ist hier weit weg und fuellt
    # nur die Bildmitte, und die Kugeln haben keine eigenen Glanzlichter.
    mf_szene.build_exposure_volume(bias=3.0)
    build_spawner()
    build_renderer()
    build_camera()
    save_level()

    LOG("=" * 78)


main()
