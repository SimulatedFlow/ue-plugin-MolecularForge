# Copyright 2026 Silvan Teufel All Rights Reserved.
#
# Schaut nach, was im Schaulevel tatsaechlich steht — Material, Instanzzahl, Sichtbarkeit.
# Gedacht fuer den Fall, dass ein Bild anders aussieht als erwartet und die Frage lautet:
# liegt es am Material, an der Komponente oder am Level?

import unreal

LOG = unreal.log

MAP_PACKAGE = "/MolecularForge/MolecularForge/Maps/L_MF_Showcase"


def describe_component(component, label):
    LOG("  %s (%s)" % (label, component.get_class().get_name()))
    LOG("    sichtbar: %s" % component.is_visible())

    materials = component.get_materials()
    if not materials:
        LOG("    Materialien: keine")
    for index, material in enumerate(materials):
        LOG("    Material %d: %s" % (index, material.get_path_name() if material else "<leer>"))

    if isinstance(component, unreal.InstancedStaticMeshComponent):
        mesh = component.static_mesh
        LOG("    Mesh: %s" % (mesh.get_path_name() if mesh else "<leer>"))
        LOG("    Instanzen: %d" % component.get_instance_count())
        LOG("    Custom-Data-Werte je Instanz: %d" % component.num_custom_data_floats)


def main():
    LOG("=" * 78)
    LOG("Schaulevel untersuchen")
    LOG("=" * 78)

    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(MAP_PACKAGE)

    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    LOG("Actors: %d" % len(actors))

    for actor in actors:
        if not isinstance(actor, unreal.MolecularStructureActor):
            continue

        LOG("")
        LOG("%s" % actor.get_actor_label())
        LOG("  Darstellung: %s" % actor.get_editor_property("Representation"))
        LOG("  Faerbung: %s" % actor.get_editor_property("ColorScheme"))

        structure = actor.get_structure()
        LOG("  Struktur: %s" % (structure.get_summary() if structure else "<nicht geladen>"))

        for getter, label in [
            ("get_atoms_component", "Kugeln"),
            ("get_bonds_component", "Staebe"),
            ("get_cartoon_component", "Band"),
            ("get_surface_component", "Oberflaeche"),
        ]:
            component = getattr(actor, getter)()
            if component:
                describe_component(component, label)

    LOG("=" * 78)


main()
