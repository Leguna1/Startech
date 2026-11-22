"""
Shared helpers for the HUD material build scripts (build_hud_panel_material.py, build_hud_edge_material.py).

Graphs are made only from nodes valid in the User Interface material domain: TexCoord, parameters, vertex colour and
plain math (no Custom, ScreenPosition, ViewSize, SceneTexture or PixelDepth nodes). Every connection is checked and
the finished material must compile its shaders without errors, or the script fails.
"""

import unreal

LOG_PREFIX = "[HUD]"


def log(message):
    # Warning level, so the line also reaches stdout in a headless run.
    unreal.log_warning(f"{LOG_PREFIX} {message}")


def fail(message):
    unreal.log_error(f"{LOG_PREFIX} {message}")
    raise RuntimeError(f"{LOG_PREFIX} {message}")


def get_or_create_material(folder, name):
    """The material at folder/name with its graph emptied, or a new one."""
    path = f"{folder}/{name}"
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        material = unreal.EditorAssetLibrary.load_asset(path)
        unreal.MaterialEditingLibrary.delete_all_material_expressions(material)
        log(f"Rebuilding {path}")
        return material

    tools = unreal.AssetToolsHelpers.get_asset_tools()
    material = tools.create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())
    if material is None:
        fail(f"Could not create {path}")
    log(f"Created {path}")
    return material


class Graph:
    """Builds a material graph node by node. Each node is (expression, output pin name)."""

    def __init__(self, material):
        self.material = material
        self.mel = unreal.MaterialEditingLibrary
        self.count = 0

    # Nodes

    def node(self, cls, **properties):
        # Laid out properly at the end; a rough position keeps nodes from stacking meanwhile.
        x = -1600 + (self.count % 8) * 200
        y = -600 + (self.count // 8) * 150
        self.count += 1
        expression = self.mel.create_material_expression(self.material, cls, x, y)
        if expression is None:
            fail(f"Could not create a {cls.__name__} node")
        for key, value in properties.items():
            expression.set_editor_property(key, value)
        return expression

    def uv(self):
        return self.node(unreal.MaterialExpressionTextureCoordinate, coordinate_index=0), ""

    def vertex_color(self):
        return self.node(unreal.MaterialExpressionVertexColor)

    def scalar(self, name, value):
        return self.node(unreal.MaterialExpressionScalarParameter, parameter_name=name, default_value=value), ""

    def vector(self, name, value):
        return self.node(unreal.MaterialExpressionVectorParameter, parameter_name=name, default_value=value)

    def mask(self, source, r=False, g=False, b=False, a=False):
        expression = self.node(unreal.MaterialExpressionComponentMask, r=r, g=g, b=b, a=a)
        self.connect(source, expression, "")
        return expression, ""

    def unary(self, cls, source):
        expression = self.node(cls)
        self.connect(source, expression, "")
        return expression, ""

    def binary(self, cls, a, b=None, const_b=None):
        """A two-input node (Add, Subtract, Multiply, Divide, Min, Max, DotProduct). b may be a constant instead."""
        properties = {} if const_b is None else {"const_b": float(const_b)}
        expression = self.node(cls, **properties)
        self.connect(a, expression, "A")
        if b is not None:
            self.connect(b, expression, "B")
        return expression, ""

    def lerp(self, a, b, alpha, const_a=None):
        properties = {} if const_a is None else {"const_a": float(const_a)}
        expression = self.node(unreal.MaterialExpressionLinearInterpolate, **properties)
        if a is not None:
            self.connect(a, expression, "A")
        self.connect(b, expression, "B")
        self.connect(alpha, expression, "Alpha")
        return expression, ""

    # Shorthands

    def add(self, a, b=None, const_b=None):
        return self.binary(unreal.MaterialExpressionAdd, a, b, const_b)

    def sub(self, a, b=None, const_b=None):
        return self.binary(unreal.MaterialExpressionSubtract, a, b, const_b)

    def mul(self, a, b=None, const_b=None):
        return self.binary(unreal.MaterialExpressionMultiply, a, b, const_b)

    def div(self, a, b=None, const_b=None):
        return self.binary(unreal.MaterialExpressionDivide, a, b, const_b)

    def min(self, a, b=None, const_b=None):
        return self.binary(unreal.MaterialExpressionMin, a, b, const_b)

    def max(self, a, b=None, const_b=None):
        return self.binary(unreal.MaterialExpressionMax, a, b, const_b)

    def dot(self, a, b):
        return self.binary(unreal.MaterialExpressionDotProduct, a, b)

    def abs(self, source):
        return self.unary(unreal.MaterialExpressionAbs, source)

    def one_minus(self, source):
        return self.unary(unreal.MaterialExpressionOneMinus, source)

    def saturate(self, source):
        return self.unary(unreal.MaterialExpressionSaturate, source)

    def sqrt(self, source):
        return self.unary(unreal.MaterialExpressionSquareRoot, source)

    # Wiring

    def connect(self, source, target, target_pin):
        expression, pin = source
        if not self.mel.connect_material_expressions(expression, pin, target, target_pin):
            fail(f"Could not connect {expression.get_name()}.{pin or 'out'} -> {target.get_name()}.{target_pin or 'in'}")

    def output(self, source, prop):
        expression, pin = source
        if not self.mel.connect_material_property(expression, pin, prop):
            fail(f"Could not connect {expression.get_name()}.{pin or 'out'} -> material {prop}")


def compile_and_save(material, name):
    """Recompiles the material, waits for its shaders, fails loudly on any error, then saves it."""
    mel = unreal.MaterialEditingLibrary
    mel.layout_material_expressions(material)

    # Errors from translating the graph.
    errors = list(mel.recompile_material(material))

    # Errors from the shader compiler itself (a Slate pixel shader failing shows up only here).
    checker = getattr(unreal, "HudMaterialCheckLibrary", None)
    if checker is None:
        fail("HudMaterialCheckLibrary isn't loaded. Build the Startech editor module first.")
    shader_errors, platform = checker.get_material_compile_errors(material)
    errors += [e for e in shader_errors if e not in errors]

    if errors:
        for error in errors:
            unreal.log_error(f"{LOG_PREFIX} {name}: {error}")
        fail(f"{name} failed to compile for {platform} ({len(errors)} error(s)); not saved.")

    stats = mel.get_statistics(material)
    log(f"{name} compiled for {platform}: {stats.num_pixel_shader_instructions} pixel shader instructions.")

    if not unreal.EditorAssetLibrary.save_loaded_asset(material, False):
        fail(f"Could not save {name}")
    log(f"{name} saved.")
