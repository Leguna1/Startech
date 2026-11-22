"""
Builds M_Asteroid, the material for the SM_Asteroid_* meshes.

Run inside the Unreal Editor (Python Editor Script Plugin enabled):
    Tools > Execute Python Script... > pick this file
or in the Output Log's Python console:
    py "<project>/Plugins/ProcSector/Tools/build_asteroid_material.py"

The material:
    Base Color = mesh vertex colour (baked by the mesh generator)
               x Color (vector parameter; AAsteroidField writes each type's Tint here)
               x lerp(BrightnessMin, BrightnessMax, PerInstanceRandom)  -> each rock a little lighter or darker
    Roughness  = Roughness (scalar parameter)
It is flagged for instanced static meshes and Nanite.

Running it again rebuilds the graph of the existing asset instead of making a copy.
"""

import unreal

MATERIAL_FOLDER = "/Game/Meshes/Asteroids"
MATERIAL_NAME = "M_Asteroid"

DEFAULT_COLOR = unreal.LinearColor(0.25, 0.22, 0.2, 1.0)
BRIGHTNESS_MIN = 0.75
BRIGHTNESS_MAX = 1.25
ROUGHNESS = 0.85


def get_or_create_material():
    path = f"{MATERIAL_FOLDER}/{MATERIAL_NAME}"
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        material = unreal.EditorAssetLibrary.load_asset(path)
        unreal.MaterialEditingLibrary.delete_all_material_expressions(material)
        unreal.log(f"Rebuilding {path}")
        return material

    tools = unreal.AssetToolsHelpers.get_asset_tools()
    material = tools.create_asset(MATERIAL_NAME, MATERIAL_FOLDER, unreal.Material, unreal.MaterialFactoryNew())
    unreal.log(f"Created {path}")
    return material


def scalar_parameter(material, name, value, x, y):
    node = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionScalarParameter, x, y)
    node.set_editor_property("parameter_name", name)
    node.set_editor_property("default_value", value)
    return node


def build():
    mel = unreal.MaterialEditingLibrary
    material = get_or_create_material()

    vertex_color = mel.create_material_expression(material, unreal.MaterialExpressionVertexColor, -900, -200)

    tint = mel.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -900, 0)
    tint.set_editor_property("parameter_name", "Color")
    tint.set_editor_property("default_value", DEFAULT_COLOR)

    instance_random = mel.create_material_expression(material, unreal.MaterialExpressionPerInstanceRandom, -900, 400)
    brightness_min = scalar_parameter(material, "BrightnessMin", BRIGHTNESS_MIN, -900, 200)
    brightness_max = scalar_parameter(material, "BrightnessMax", BRIGHTNESS_MAX, -900, 300)

    brightness = mel.create_material_expression(material, unreal.MaterialExpressionLinearInterpolate, -600, 300)
    mel.connect_material_expressions(brightness_min, "", brightness, "A")
    mel.connect_material_expressions(brightness_max, "", brightness, "B")
    mel.connect_material_expressions(instance_random, "", brightness, "Alpha")

    tinted = mel.create_material_expression(material, unreal.MaterialExpressionMultiply, -600, -100)
    mel.connect_material_expressions(vertex_color, "", tinted, "A")
    mel.connect_material_expressions(tint, "", tinted, "B")

    base_color = mel.create_material_expression(material, unreal.MaterialExpressionMultiply, -350, 0)
    mel.connect_material_expressions(tinted, "", base_color, "A")
    mel.connect_material_expressions(brightness, "", base_color, "B")
    mel.connect_material_property(base_color, "", unreal.MaterialProperty.MP_BASE_COLOR)

    roughness = scalar_parameter(material, "Roughness", ROUGHNESS, -350, 250)
    mel.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)

    # set_base_material_usage replaced set_material_usage in newer engine versions.
    set_usage = getattr(mel, "set_base_material_usage", None) or mel.set_material_usage
    set_usage(material, unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES)
    set_usage(material, unreal.MaterialUsage.MATUSAGE_NANITE)

    mel.layout_material_expressions(material)
    mel.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)
    unreal.log(f"{MATERIAL_NAME} built and saved.")


build()
