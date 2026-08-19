# Copyright Simulated Flow. All Rights Reserved.
#
# Baut die Materialien des Plugins.
#
# Als Skript und nicht von Hand geklickt, aus demselben Grund wie beim Level: ein
# Materialgraph, den niemand nachvollziehen kann, ist beim naechsten Umbau verloren.
# Hier steht jeder Knoten und jede Verbindung im Klartext.
#
# Aufruf:
#   UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script="<dieser Pfad>" -unattended

import unreal

LOG = unreal.log

MATERIAL_DIR = "/MolecularForge/Materials"

# Fehlersuche: laesst Ausrichtung und Kreisausschnitt weg. Das Viereck bleibt dann flach
# und liegt in der XY-Ebene, wird aber gezeichnet. Sieht man Rechtecke, stimmen Mesh,
# Instanzen und Farben — und der Fehler steckt in der Ausrichtung. Bleibt es schwarz,
# liegt es weiter vorn.
DIAGNOSTIC_FLAT_QUADS = False

ATOM_MATERIAL = "M_MF_Atoms"
IMPOSTOR_MATERIAL = "M_MF_AtomImpostor"
VERTEX_MATERIAL = "M_MF_VertexColor"

# ---------------------------------------------------------------------------------------
# Der gemeinsame Teil der Impostor-Rechnung.
#
# Ein Impostor ist ein Viereck, das immer zur Kamera zeigt und in dem der Pixelshader die
# Kugel ausrechnet, statt sie aus Dreiecken zu bauen. Die Silhouette wird dadurch nicht
# schlechter, sondern besser — sie ist analytisch exakt statt facettiert — und aus 382
# Dreiecken je Atom werden zwei.
#
# `c` sind die Ecken des Vierecks von -1 bis 1. Der Abstand vom Mittelpunkt entscheidet,
# ob ein Pixel noch auf der Kugel liegt, und liefert zugleich ihre Woelbung.
IMPOSTOR_BASIS = """
	float2 c = (UV - 0.5f) * 2.0f;
	float3 F = normalize(CamVec);
	// Ein Bezugsvektor, der nicht mit der Blickrichtung zusammenfaellt — sonst waere das
	// Kreuzprodukt null und das Viereck haette keine Ausrichtung mehr.
	float3 RefUp = abs(F.z) > 0.99f ? float3(1.0f, 0.0f, 0.0f) : float3(0.0f, 0.0f, 1.0f);
	float3 R = normalize(cross(RefUp, F));
	float3 U = cross(F, R);
"""

MEL = unreal.MaterialEditingLibrary


def create_material(name):
    """Legt ein Material an und ersetzt ein vorhandenes."""
    path = "%s/%s" % (MATERIAL_DIR, name)

    if unreal.EditorAssetLibrary.does_asset_exist(path):
        LOG("  Vorhandenes Material wird ersetzt: %s" % path)
        unreal.EditorAssetLibrary.delete_asset(path)

    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        name, MATERIAL_DIR, unreal.Material, unreal.MaterialFactoryNew())

    if material is None:
        LOG("  FEHLER: %s liess sich nicht anlegen." % path)
    return material


def scalar(material, value, x, y, description):
    node = MEL.create_material_expression(material, unreal.MaterialExpressionConstant, x, y)
    node.set_editor_property("r", value)
    node.set_editor_property("desc", description)
    return node


def build_atom_material():
    """Farbe je Instanz aus den Per-Instance-Daten.

    Die Kugelkomponente legt dort seit Phase 1 Farbe und Radius ab (0..2 Farbe, 3 Radius),
    ohne dass es bis jetzt jemand gelesen haette. Genau dafuer war die Belegung gedacht:
    zehntausende Atome brauchen zehntausende Farben, und ein Materialexemplar je Atom
    waere weder bezahlbar noch instanzierbar.
    """
    material = create_material(ATOM_MATERIAL)
    if material is None:
        return None

    channels = []
    for index, label in enumerate(("Rot", "Gruen", "Blau")):
        node = MEL.create_material_expression(
            material, unreal.MaterialExpressionPerInstanceCustomData, -800, -200 + index * 150)
        node.set_editor_property("data_index", index)
        node.set_editor_property("desc", "Instanzfarbe %s" % label)
        channels.append(node)

    # Aus drei Einzelwerten einen Farbvektor bauen.
    append_rg = MEL.create_material_expression(
        material, unreal.MaterialExpressionAppendVector, -500, -150)
    MEL.connect_material_expressions(channels[0], "", append_rg, "A")
    MEL.connect_material_expressions(channels[1], "", append_rg, "B")

    append_rgb = MEL.create_material_expression(
        material, unreal.MaterialExpressionAppendVector, -300, -100)
    MEL.connect_material_expressions(append_rg, "", append_rgb, "A")
    MEL.connect_material_expressions(channels[2], "", append_rgb, "B")

    MEL.connect_material_property(append_rgb, "", unreal.MaterialProperty.MP_BASE_COLOR)

    # Ohne dieses Kennzeichen verweigert die Engine das Material auf instanzierten Meshes
    # und nimmt stillschweigend das graue Standardmaterial — im Editor sieht man dann
    # farblose Kugeln und keinen Hinweis, warum. Die Meldung dazu steht nur im Log.
    material.set_editor_property("used_with_instanced_static_meshes", True)

    # Atome sind keine Metalle und keine Spiegel. Ein mittleres Rauigkeitsniveau laesst
    # die Kugeln plastisch wirken, ohne dass die Glanzlichter das Bild uebernehmen.
    MEL.connect_material_property(
        scalar(material, 0.0, -300, 100, "Metallisch"), "", unreal.MaterialProperty.MP_METALLIC)
    MEL.connect_material_property(
        scalar(material, 0.35, -300, 200, "Rauigkeit"), "", unreal.MaterialProperty.MP_ROUGHNESS)
    MEL.connect_material_property(
        scalar(material, 0.5, -300, 300, "Glanz"), "", unreal.MaterialProperty.MP_SPECULAR)

    MEL.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material, False)

    LOG("  %s gebaut." % ATOM_MATERIAL)
    return material


def custom_node(material, name, code, output_type, x, y, input_names):
    """Legt einen HLSL-Knoten an und benennt seine Eingaenge."""
    node = MEL.create_material_expression(material, unreal.MaterialExpressionCustom, x, y)
    node.set_editor_property("code", code)
    node.set_editor_property("output_type", output_type)
    node.set_editor_property("description", name)
    entries = []
    for name_of_input in input_names:
        entry = unreal.CustomInput()
        entry.set_editor_property("input_name", name_of_input)
        entries.append(entry)

    node.set_editor_property("inputs", entries)
    return node


def build_impostor_material():
    """Kugeln als Impostoren: ein Viereck je Atom statt eines Kugelmeshs.

    Das Viereck sitzt an der Vorderseite der Kugel und nicht in ihrem Mittelpunkt. Der
    Grund ist der Tiefenversatz: Unreal kann einen Pixel nur nach hinten schieben, nie
    nach vorn. Liegt das Viereck vorn, ist die Kugeloberflaeche immer dahinter, und der
    Versatz bleibt positiv.
    """
    material = create_material(IMPOSTOR_MATERIAL)
    if material is None:
        return None

    # ---- gemeinsame Eingaenge ----
    uv = MEL.create_material_expression(material, unreal.MaterialExpressionTextureCoordinate, -1400, 0)

    radius = MEL.create_material_expression(
        material, unreal.MaterialExpressionPerInstanceCustomData, -1400, 150)
    radius.set_editor_property("data_index", 3)
    radius.set_editor_property("desc", "Kugelradius in Unreal-Einheiten")

    camera_vector = MEL.create_material_expression(
        material, unreal.MaterialExpressionCameraVectorWS, -1400, 300)

    # Der Versatz des Vertex vom Mittelpunkt seiner Instanz.
    #
    # `PreSkinnedPosition` waere der naheliegende Weg, liefert bei Static Meshes aber
    # null — das Viereck fiel damit in einen Punkt zusammen und war unsichtbar. Die
    # Differenz aus Welt- und Objektposition ist dagegen belastbar und wird von Unreal
    # auch bei grossen Weltkoordinaten richtig behandelt.
    world_position = MEL.create_material_expression(
        material, unreal.MaterialExpressionWorldPosition, -1400, 450)
    object_position = MEL.create_material_expression(
        material, unreal.MaterialExpressionObjectPositionWS, -1400, 600)
    local_position = MEL.create_material_expression(
        material, unreal.MaterialExpressionSubtract, -1150, 500)
    MEL.connect_material_expressions(world_position, "", local_position, "A")
    MEL.connect_material_expressions(object_position, "", local_position, "B")

    # ---- Viereck ausrichten ----
    # Die Ecke wird hier aus der lokalen Position abgeleitet und nicht aus der
    # Texturkoordinate. Der Unterschied ist wesentlich: eine gespiegelte UV-Belegung
    # wuerde die Ecken vertauschen, und das Viereck schluege eine Schleife statt eine
    # Flaeche aufzuspannen. Im Pixelshader ist die UV dagegen unbedenklich — eine
    # gespiegelte Kugel sieht aus wie eine Kugel.
    wpo = custom_node(
        material, "Impostor: Viereck ausrichten",
        """
	float3 F = normalize(CamVec);
	float3 RefUp = abs(F.z) > 0.99f ? float3(1.0f, 0.0f, 0.0f) : float3(0.0f, 0.0f, 1.0f);
	float3 R = normalize(cross(RefUp, F));
	float3 U = cross(F, R);

	// Die Ecke wird aus dem tatsaechlichen Versatz abgeleitet und nicht aus der
	// Texturkoordinate. Damit koennen Ecke und Versatz gar nicht auseinanderlaufen —
	// bei einer gespiegelten UV-Belegung waere das Viereck sonst verzerrt.
	// Das Viereck liegt ungedreht in der XY-Ebene, also traegt z nichts bei.
	float2 c = RelPos.xy / max(Radius, 0.0001f);

	// Wo der Vertex liegen soll: auf einem zur Kamera gedrehten Viereck, das um einen
	// Radius nach vorn geschoben ist, damit der Tiefenversatz nie negativ wird.
	float3 Target = F * Radius + R * (c.x * Radius) + U * (c.y * Radius);

	return Target - RelPos;
""",
        unreal.CustomMaterialOutputType.CMOT_FLOAT3, -800, 0,
        ["RelPos", "Radius", "CamVec"])

    MEL.connect_material_expressions(local_position, "", wpo, "RelPos")
    MEL.connect_material_expressions(radius, "", wpo, "Radius")
    MEL.connect_material_expressions(camera_vector, "", wpo, "CamVec")

    # ---- runder Ausschnitt ----
    mask = custom_node(
        material, "Impostor: Kreisausschnitt",
        """
	float2 c = (UV - 0.5f) * 2.0f;
	return dot(c, c) <= 1.0f ? 1.0f : 0.0f;
""",
        unreal.CustomMaterialOutputType.CMOT_FLOAT1, -800, 300, ["UV"])

    MEL.connect_material_expressions(uv, "", mask, "UV")

    # ---- Woelbung als Normale ----
    normal = custom_node(
        material, "Impostor: Normale der Kugel",
        IMPOSTOR_BASIS + """
	float z = sqrt(saturate(1.0f - dot(c, c)));
	return normalize(R * c.x + U * c.y + F * z);
""",
        unreal.CustomMaterialOutputType.CMOT_FLOAT3, -800, 450,
        ["UV", "CamVec"])

    MEL.connect_material_expressions(uv, "", normal, "UV")
    MEL.connect_material_expressions(camera_vector, "", normal, "CamVec")

    # ---- Tiefenversatz ----
    # Ohne ihn schneiden sich benachbarte Kugeln nicht richtig: sie stuenden alle auf
    # ihrer flachen Vierecksebene, und wo zwei sich beruehren, gaebe es eine gerade
    # Kante statt einer Durchdringung.
    depth = custom_node(
        material, "Impostor: Tiefenversatz",
        """
	float2 c = (UV - 0.5f) * 2.0f;
	float z = sqrt(saturate(1.0f - dot(c, c)));
	return Radius * (1.0f - z);
""",
        unreal.CustomMaterialOutputType.CMOT_FLOAT1, -800, 700, ["UV", "Radius"])

    MEL.connect_material_expressions(uv, "", depth, "UV")
    MEL.connect_material_expressions(radius, "", depth, "Radius")

    # ---- Farbe wie beim Kugelmaterial ----
    channels = []
    for index in range(3):
        node = MEL.create_material_expression(
            material, unreal.MaterialExpressionPerInstanceCustomData, -1400, -400 + index * 120)
        node.set_editor_property("data_index", index)
        channels.append(node)

    append_rg = MEL.create_material_expression(
        material, unreal.MaterialExpressionAppendVector, -1100, -350)
    MEL.connect_material_expressions(channels[0], "", append_rg, "A")
    MEL.connect_material_expressions(channels[1], "", append_rg, "B")

    append_rgb = MEL.create_material_expression(
        material, unreal.MaterialExpressionAppendVector, -900, -320)
    MEL.connect_material_expressions(append_rg, "", append_rgb, "A")
    MEL.connect_material_expressions(channels[2], "", append_rgb, "B")

    # ---- alles ueber einen Attributknoten zusammenfuehren ----
    #
    # Der Umweg ist noetig, weil Python den Eingang fuer den Tiefenversatz nicht direkt
    # anbietet — die Aufzaehlung der Materialeingaenge kennt ihn schlicht nicht. Der
    # Attributknoten hat ihn dagegen als Pin, und ueber ihn laesst sich das ganze
    # Material anschliessen.
    attributes = MEL.create_material_expression(
        material, unreal.MaterialExpressionMakeMaterialAttributes, -400, 0)

    MEL.connect_material_expressions(append_rgb, "", attributes, "BaseColor")

    if DIAGNOSTIC_FLAT_QUADS:
        LOG("  ACHTUNG: Impostor im Diagnosemodus — flache Vierecke ohne Ausrichtung.")
    else:
        MEL.connect_material_expressions(normal, "", attributes, "Normal")
        MEL.connect_material_expressions(mask, "", attributes, "OpacityMask")
        MEL.connect_material_expressions(wpo, "", attributes, "WorldPositionOffset")
        MEL.connect_material_expressions(depth, "", attributes, "PixelDepthOffset")

    MEL.connect_material_expressions(
        scalar(material, 0.0, -700, 900, "Metallisch"), "", attributes, "Metallic")
    MEL.connect_material_expressions(
        scalar(material, 0.35, -700, 1000, "Rauigkeit"), "", attributes, "Roughness")

    material.set_editor_property("use_material_attributes", True)
    MEL.connect_material_property(attributes, "", unreal.MaterialProperty.MP_MATERIAL_ATTRIBUTES)

    # Maskiert, weil der Kreisausschnitt harte Kanten hat und keine Halbtransparenz.
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    material.set_editor_property("opacity_mask_clip_value", 0.5)

    # Die Normale kommt in Weltkoordinaten aus dem HLSL, nicht im Tangentenraum.
    material.set_editor_property("tangent_space_normal", False)

    # Beidseitig, weil das Viereck je nach Blickwinkel auch von hinten getroffen wird.
    material.set_editor_property("two_sided", True)

    material.set_editor_property("used_with_instanced_static_meshes", True)

    MEL.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material, False)

    LOG("  %s gebaut." % IMPOSTOR_MATERIAL)
    return material


def build_vertex_color_material():
    """Farbe aus den Vertexfarben — fuer Band und Oberflaeche.

    Beide erzeugen ein einziges Mesh und koennen deshalb keine Instanzdaten tragen. Ihre
    Farbe steckt stattdessen in den Vertices, wo die Erzeugung sie hingeschrieben hat.
    """
    material = create_material(VERTEX_MATERIAL)
    if material is None:
        return None

    vertex_color = MEL.create_material_expression(
        material, unreal.MaterialExpressionVertexColor, -500, -100)
    vertex_color.set_editor_property("desc", "Farbe aus dem Mesh")

    MEL.connect_material_property(vertex_color, "", unreal.MaterialProperty.MP_BASE_COLOR)

    MEL.connect_material_property(
        scalar(material, 0.0, -300, 100, "Metallisch"), "", unreal.MaterialProperty.MP_METALLIC)
    MEL.connect_material_property(
        scalar(material, 0.30, -300, 200, "Rauigkeit"), "", unreal.MaterialProperty.MP_ROUGHNESS)

    # Beidseitig: das Cartoon-Band ist eine duenne Flaeche, und an ihrem Rand sieht man
    # ohne das durch sie hindurch ins Nichts.
    material.set_editor_property("two_sided", True)

    # Dieselbe Falle wie beim Atommaterial, nur fuer die anderen Traeger. Lieber beide
    # Kennzeichen setzen, als beim naechsten Komponentenwechsel wieder graue Flaechen
    # zu suchen.
    material.set_editor_property("used_with_instanced_static_meshes", True)
    material.set_editor_property("used_with_static_lighting", True)

    MEL.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material, False)

    LOG("  %s gebaut." % VERTEX_MATERIAL)
    return material


def main():
    LOG("=" * 78)
    LOG("MolecularForge — Materialien bauen")
    LOG("=" * 78)

    atom = build_atom_material()
    impostor = build_impostor_material()
    vertex = build_vertex_color_material()

    LOG("=" * 78)
    if atom and impostor and vertex:
        LOG("Fertig. Alle Materialien liegen unter %s" % MATERIAL_DIR)
    else:
        LOG("FEHLER: nicht alle Materialien konnten gebaut werden.")
    LOG("=" * 78)


main()
