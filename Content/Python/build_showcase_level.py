# Copyright 2026 Silvan Teufel All Rights Reserved.
#
# Baut das Schaulevel des Plugins.
#
# Bewusst als Skript und nicht von Hand zusammengeklickt: das Level muss sich nach jeder
# Aenderung an Actor oder Material reproduzierbar neu erzeugen lassen, und fuer die
# Bildaufnahme muessen Kamerastandpunkt und Beleuchtung jedes Mal gleich sein. Von Hand
# gesetzte Werte waeren nach dem dritten Durchlauf nicht mehr dieselben.
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

MAP_PACKAGE = "/MolecularForge/MolecularForge/Maps/L_MF_Showcase"

MAP_FILE = os.path.join(
    unreal.Paths.project_dir(), "Plugins", "MolecularForge",
    "Content", "MolecularForge", "Maps", "L_MF_Showcase.umap")

# Relativ zum Projektverzeichnis — so loest der Actor den Pfad auf.
STRUCTURE_RELATIVE_PATH = "Plugins/MolecularForge/Demo/1EMA.pdb"

# Der Blickpunkt wird aus der Ausdehnung der geladenen Struktur gerechnet, nicht gesetzt.
# Von Hand gewaehlte Werte passen immer nur zu genau einer Struktur — beim naechsten
# Molekuel steht das Bild dann wieder daneben.
CAMERA_FOV_DEGREES = 90.0

# Anteil der Bildbreite, den das Molekuel einnehmen soll. Etwas Luft bleibt, damit die
# Silhouette nicht am Rand klebt.
CAMERA_FILL_FRACTION = 0.55


def get_actor_subsystem():
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def get_level_subsystem():
    return unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)


def spawn(actor_class, location, rotation=None, label=None):
    rotation = rotation or unreal.Rotator(0.0, 0.0, 0.0)
    actor = get_actor_subsystem().spawn_actor_from_class(actor_class, location, rotation)
    if actor and label:
        actor.set_actor_label(label)
    return actor


def build_lighting():
    """Eine schlichte, kontrastreiche Ausleuchtung.

    Kein Himmel und kein Nebel: eine Molekuelabbildung lebt von der Silhouette, und ein
    Verlaufshimmel im Hintergrund nimmt ihr genau die. Stattdessen ein gerichtetes
    Hauptlicht, ein schwaches Gegenlicht fuer die Kanten und ein dunkles Umgebungslicht,
    damit die abgewandten Seiten nicht schwarz absaufen.
    """
    key = spawn(unreal.DirectionalLight, unreal.Vector(0.0, 0.0, 1000.0),
                unreal.Rotator(0.0, -45.0, 35.0), "Licht_Haupt")
    if key:
        # Von 6 auf 3,5 gesenkt, wie im Vergleichslevel: bei 6 liefen die hellen Atome
        # aus. Die drei Level sollen als Satz gleich aussehen.
        key.light_component.set_intensity(3.5)
        key.light_component.set_light_color(unreal.LinearColor(1.0, 0.97, 0.92, 1.0))

    rim = spawn(unreal.DirectionalLight, unreal.Vector(0.0, 0.0, 1000.0),
                unreal.Rotator(0.0, -20.0, -140.0), "Licht_Kante")
    if rim:
        rim.light_component.set_intensity(2.5)
        rim.light_component.set_light_color(unreal.LinearColor(0.6, 0.75, 1.0, 1.0))
        rim.light_component.set_cast_shadows(False)

    sky = spawn(unreal.SkyLight, unreal.Vector(0.0, 0.0, 500.0), None, "Licht_Umgebung")
    if sky:
        sky.light_component.set_intensity(0.6)
        sky.light_component.set_editor_property("source_type",
                                                unreal.SkyLightSourceType.SLS_SPECIFIED_CUBEMAP)


def build_structure_actor():
    actor = spawn(unreal.MolecularStructureActor, unreal.Vector(0.0, 0.0, 0.0),
                  None, "Molekuel_Schaustueck")
    if not actor:
        LOG("FEHLER: MolecularStructureActor liess sich nicht erzeugen.")
        return None

    actor.set_editor_property("StructureFilePath", STRUCTURE_RELATIVE_PATH)
    actor.set_editor_property("bLoadOnBeginPlay", True)
    actor.set_editor_property("Representation", unreal.MolRepresentation.SPACE_FILLING)
    actor.set_editor_property("ColorScheme", unreal.MolColorScheme.ELEMENT)

    options = actor.get_editor_property("LoadOptions")
    options.set_editor_property("bDiscardWater", True)
    options.set_editor_property("bCenterOnOrigin", True)
    options.set_editor_property("UnitsPerAngstrom", 10.0)
    actor.set_editor_property("LoadOptions", options)

    # Im Editor gleich einmal laden, damit das Level nicht leer aussieht, wenn jemand
    # es oeffnet, ohne zu starten.
    actor.call_method("LoadNow")

    assign_materials(actor)

    return actor


def assign_materials(actor):
    """Prueft nach, welches Material die Komponenten tatsaechlich tragen.

    Gesetzt werden sie inzwischen von den Komponenten selbst beim Registrieren — hier
    wird nur nachgelesen. Der Unterschied ist wichtig: wuerde das Skript sie setzen,
    ueberschriebe es die Wahl zwischen Impostor und echter Kugel, die der Anwender an
    der Komponente trifft.
    """
    for getter, label in [
        ("get_atoms_component", "Kugeln"),
        ("get_bonds_component", "Staebe"),
        ("get_cartoon_component", "Band"),
        ("get_surface_component", "Oberflaeche"),
    ]:
        component = getattr(actor, getter)()
        if component is None:
            continue

        applied = component.get_material(0)
        LOG("  %s: %s" % (label, applied.get_path_name() if applied else "<leer>"))


def compute_camera_distance(actor):
    """Abstand, aus dem die Struktur das Bild fuellt.

    Die Huelle liefert der Strukturdatensatz selbst — in Angstroem, wie ueberall im
    Plugin. Umgerechnet mit demselben Massstab, den auch die Darstellung benutzt, ergibt
    sich der Radius in Unreal-Einheiten, und daraus ueber den halben Bildwinkel der
    Abstand. So sitzt der Ausschnitt bei jeder Struktur richtig, ohne Nachjustieren.
    """
    default_distance = 900.0

    structure = actor.get_structure()
    if not structure:
        LOG("  (keine Struktur geladen, Kameraabstand geschaetzt)")
        return default_distance

    bounds = structure.get_bounds_angstrom()
    options = actor.get_editor_property("LoadOptions")
    scale = options.get_editor_property("UnitsPerAngstrom")

    extent = bounds.max - bounds.min
    radius_units = max(extent.x, extent.y, extent.z) * 0.5 * scale

    if radius_units <= 0.0:
        return default_distance

    half_fov = math.radians(CAMERA_FOV_DEGREES * 0.5)
    distance = radius_units / (math.tan(half_fov) * CAMERA_FILL_FRACTION)

    LOG("  Strukturradius %.0f Einheiten -> Kameraabstand %.0f" % (radius_units, distance))
    return distance


def build_camera(actor):
    """Startpunkt fuer den Spieler.

    Im -game-Modus setzt die Standard-Spielregel einen fliegenden Pawn auf den
    PlayerStart und macht dessen Kamera zum Blickpunkt. Das ist der kuerzeste Weg zu
    einem reproduzierbaren Standbild, ohne eigenen Spielmodus oder Level-Blueprint.
    """
    distance = compute_camera_distance(actor)

    # Leicht von oben und zur Seite: eine Ansicht genau von vorn laesst ein globulaeres
    # Protein flach wirken, weil keine Kante die Tiefe verraet.
    location = unreal.Vector(-distance * 0.92, -distance * 0.30, distance * 0.22)
    rotation = unreal.Rotator(0.0, -12.0, 18.0)

    spawn(unreal.PlayerStart, location, rotation, "Blickpunkt")
    spawn(unreal.CameraActor, location, rotation, "Kamera_Schaubild")

    LOG("  Blickpunkt: %s" % str(location))


def save_level(level_subsystem):
    """Speichert die Karte und meldet, welcher Weg getragen hat.

    Unreal bietet mehrere, und welcher bei einer frisch angelegten Karte greift, haengt
    davon ab, ob sie schon einen Dateinamen hat. Statt das zu raten, werden sie der Reihe
    nach probiert — und protokolliert, damit beim naechsten Mal klar ist, welcher zaehlt.
    """
    size_before = os.path.getsize(MAP_FILE) if os.path.exists(MAP_FILE) else 0

    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()

    # Diagnose vor dem Speichern: wenn die Welt eine andere ist als die frisch angelegte,
    # speichern alle Wege brav das falsche — und melden das nicht.
    LOG("  Welt: %s" % world.get_path_name())
    actors = get_actor_subsystem().get_all_level_actors()
    LOG("  Actors in der Welt: %d" % len(actors))
    for actor in actors:
        LOG("    - %s (%s)" % (actor.get_actor_label(), actor.get_class().get_name()))

    # Der entscheidende Punkt: im Kommandozeilenbetrieb markiert das Setzen von Actors
    # das Paket nicht als geaendert. Alle Speicherwege, die nur "schmutzige" Pakete
    # anfassen, tun dann schlicht nichts — und melden trotzdem Erfolg oder Misserfolg,
    # ohne dass man den Unterschied saehe. Deshalb wird erzwungen gespeichert.
    attempts = [
        ("save_map (Paketpfad)",
         lambda: unreal.EditorLoadingAndSavingUtils.save_map(world, MAP_PACKAGE)),
        ("save_map (Dateipfad)",
         lambda: unreal.EditorLoadingAndSavingUtils.save_map(world, MAP_FILE)),
        ("save_loaded_asset (erzwungen)",
         lambda: unreal.EditorAssetLibrary.save_loaded_asset(world, False)),
        ("save_current_level", lambda: level_subsystem.save_current_level()),
    ]

    for name, attempt in attempts:
        try:
            reported = attempt()
        except Exception as error:
            LOG("  %s: Ausnahme (%s)" % (name, error))
            continue

        size_after = os.path.getsize(MAP_FILE) if os.path.exists(MAP_FILE) else 0
        LOG("  %s: meldet %s, Datei %d -> %d Byte" % (name, reported, size_before, size_after))

        # Der Rueckgabewert allein genuegt nicht: entscheidend ist, ob die Datei
        # tatsaechlich gewachsen ist. Eine leere Karte hat rund 8 KB, eine mit
        # Actors und Instanzen deutlich mehr.
        if size_after > size_before:
            LOG("Level gespeichert: %s (%d Byte)" % (MAP_PACKAGE, size_after))
            return True

        size_before = size_after

    LOG("FEHLER: Kein Speicherweg hat die Karte geschrieben.")
    return False


def main():
    LOG("=" * 78)
    LOG("MolecularForge — Schaulevel bauen")
    LOG("=" * 78)

    level_subsystem = get_level_subsystem()

    # Reproduzierbarkeit vor Bequemlichkeit: das Level wird jedes Mal von Grund auf neu
    # gebaut. Wuerde ein vorhandenes weiterverwendet, sammelten sich ueber die Laeufe
    # doppelte Actors an, und der Bildausschnitt waere beim naechsten Mal ein anderer.
    if unreal.EditorAssetLibrary.does_asset_exist(MAP_PACKAGE):
        LOG("Vorhandenes Level wird ersetzt.")
        unreal.EditorAssetLibrary.delete_asset(MAP_PACKAGE)

    # Zusaetzlich ueber das Dateisystem: eine Karte, die geschrieben wurde, nachdem der
    # Asset-Registry zuletzt gelesen hat, ist ihm unbekannt — `does_asset_exist` sagt dann
    # nein, waehrend `new_level` die Datei trotzdem findet und sich weigert.
    if os.path.exists(MAP_FILE):
        LOG("Vorhandene Kartendatei wird entfernt: %s" % MAP_FILE)
        try:
            os.remove(MAP_FILE)
        except OSError as error:
            LOG("FEHLER: Kartendatei liess sich nicht entfernen: %s" % error)
            return

    # Bewusst *kein* `new_level(Zielpfad)`: das legt zwar eine leere Karte am Ziel an,
    # laesst den Editor danach aber auf einer namenlosen Welt stehen — die Actors landen
    # dann in `/Temp/Untitled`, und jeder Speicherversuch schreibt ins Leere, ohne dass
    # man es der Rueckmeldung ansieht. Stattdessen wird in die vorhandene Welt gebaut
    # und am Ende unter dem Zielnamen weggeschrieben.
    existing = get_actor_subsystem().get_all_level_actors()
    if existing:
        LOG("Vorhandene Welt wird geleert (%d Actors)." % len(existing))
        get_actor_subsystem().destroy_actors(existing)

    build_lighting()
    mf_szene.build_exposure_volume()
    LOG("Beleuchtung gesetzt.")

    actor = build_structure_actor()
    if actor:
        atoms = actor.get_atoms_component()
        visible = atoms.get_num_visible_atoms() if atoms else -1
        LOG("Struktur geladen: %d sichtbare Atome." % visible)

    build_camera(actor)

    save_level(level_subsystem)

    LOG("=" * 78)


main()
