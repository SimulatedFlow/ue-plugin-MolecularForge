# Copyright 2026 Silvan Teufel All Rights Reserved.
#
# Gemeinsame Bausteine der Beispiel-Level.
#
# Hier steht, was in allen drei Levels gleich sein soll. Der Grund ist nicht Sparsamkeit,
# sondern Vergleichbarkeit: drei Schaubilder mit drei verschiedenen Belichtungen sehen im
# Listing nach drei verschiedenen Plugins aus.
#
# Eingebunden wird das so — ohne Verlass auf __file__, das im Kommandlet nicht gesetzt sein
# muss:
#
#     import os, sys, unreal
#     sys.path.insert(0, os.path.join(unreal.Paths.project_dir(),
#                                     "Plugins", "MolecularForge", "Content", "Python"))
#     import mf_szene

import unreal

LOG = unreal.log


def actor_subsystem():
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def build_exposure_volume(bias=2.6, bloom=0.35, label="Belichtung"):
    """
    Nagelt die Belichtung fest.

    Ohne das regelt die automatische Belichtung auf den Bildinhalt. Und der Bildinhalt ist
    hier immer dasselbe: ein helles Motiv vor Schwarz. Die Automatik faehrt dann auf, bis
    das Motiv ausbrennt — und gleicht jede Absenkung der Lampen wieder aus, sodass man an
    den Lichtern beliebig drehen kann, ohne dass sich am Bild etwas aendert. Genau daran
    ist die Mesoskala-Demo zuerst gescheitert: die Molekuele waren weiss, obwohl im
    Instanzdatensatz nachweislich Farben standen.

    Min und Max gleichzusetzen schaltet die Regelung ab; die Helligkeit stellt man danach
    ueber die Blendenkorrektur ein, nicht ueber die Lampen.
    """
    volume = actor_subsystem().spawn_actor_from_class(
        unreal.PostProcessVolume, unreal.Vector(0.0, 0.0, 0.0), unreal.Rotator(0.0, 0.0, 0.0))

    if not volume:
        LOG("FEHLER: Post-Process-Volumen liess sich nicht erzeugen.")
        return None

    volume.set_actor_label(label)
    volume.set_editor_property("unbound", True)

    settings = volume.get_editor_property("settings")
    settings.set_editor_property("override_auto_exposure_min_brightness", True)
    settings.set_editor_property("auto_exposure_min_brightness", 1.0)
    settings.set_editor_property("override_auto_exposure_max_brightness", True)
    settings.set_editor_property("auto_exposure_max_brightness", 1.0)
    settings.set_editor_property("override_auto_exposure_bias", True)
    settings.set_editor_property("auto_exposure_bias", bias)

    # Etwas Bloom laesst die Szene lebendig wirken; zu viel traegt ausgebrannte Stellen
    # in die Umgebung und macht aus einem Molekuel einen Lichtfleck.
    settings.set_editor_property("override_bloom_intensity", True)
    settings.set_editor_property("bloom_intensity", bloom)

    volume.set_editor_property("settings", settings)
    LOG("  Belichtung festgesetzt (Blendenkorrektur %.1f)." % bias)
    return volume
