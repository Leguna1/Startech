"""
Builds M_HUD_ChamferPanel, the HUD's translucent panel background with cut corners.

Run headless (editor closed, Startech editor module built):
    UnrealEditor-Cmd.exe "<project>/Startech.uproject" -run=pythonscript -script="<project>/Tools/HUD/build_hud_panel_material.py"
or inside the editor: Tools > Execute Python Script... > pick this file

The material (domain User Interface, blend mode Translucent):
    a rectangle of Size pixels with its top-right and bottom-left corners cut at 45 degrees by CornerSize pixels,
    filled with FillColor and outlined BorderWidth pixels wide in BorderColor. The shape is a signed distance built
    from TexCoord[0] and Size with plain math nodes (no Custom node), so the corners stay sharp at any widget size.
    Size is set from C++ (FHudPanelBrush) to the drawn size. The widget's tint and opacity (vertex colour) multiply
    the result.

Running it again rebuilds the graph of the existing asset. Any connection that fails, or any shader compile error,
raises an error and the asset isn't saved.
"""

import os
import sys

import unreal

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
from hud_material_graph import Graph, compile_and_save, get_or_create_material  # noqa: E402

MATERIAL_FOLDER = "/Game/UI/HUD"
MATERIAL_NAME = "M_HUD_ChamferPanel"

# 1 / sqrt(2): turns a distance along a 45-degree diagonal into a perpendicular distance.
DIAGONAL = 0.70710678


def build():
    material = get_or_create_material(MATERIAL_FOLDER, MATERIAL_NAME)
    material.set_editor_property("material_domain", unreal.MaterialDomain.MD_UI)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)

    g = Graph(material)
    size = g.vector("Size", unreal.LinearColor(380.0, 112.0, 0.0, 0.0))
    corner = g.scalar("CornerSize", 10.0)
    border_width = g.scalar("BorderWidth", 1.0)
    fill = g.vector("FillColor", unreal.LinearColor(0.0012, 0.0251, 0.0865, 0.35))
    border = g.vector("BorderColor", unreal.LinearColor(0.11, 0.59, 0.38, 0.5))
    vertex = g.vertex_color()

    # S: size in pixels, at least 1; H: half of it; Q: this pixel relative to the centre (y down).
    s = g.max(g.mask((size, "RGB"), r=True, g=True), const_b=1.0)
    h = g.mul(s, const_b=0.5)
    q = g.sub(g.mul(g.uv(), s), h)
    q_x = g.mask(q, r=True)
    q_y = g.mask(q, g=True)

    # Signed distance to the box: max of |Q| - H over both axes.
    d = g.sub(g.abs(q), h)
    box = g.max(g.mask(d, r=True), g.mask(d, g=True))

    # The two 45-degree cuts: top right (x up, y up the screen) and bottom left.
    cut = g.sub(g.add(g.mask(h, r=True), g.mask(h, g=True)), corner)
    cut_top_right = g.mul(g.sub(g.sub(q_x, q_y), cut), const_b=DIAGONAL)
    cut_bottom_left = g.mul(g.sub(g.sub(q_y, q_x), cut), const_b=DIAGONAL)
    dist = g.max(box, g.max(cut_top_right, cut_bottom_left))

    # Coverage with a one-pixel soft edge: inside the shape, and inside the border band.
    inside = g.saturate(g.sub(g.mul(dist, const_b=-1.0), const_b=-0.5))
    half_border = g.mul(border_width, const_b=0.5)
    band = g.sub(g.abs(g.add(dist, half_border)), half_border)
    on_border = g.saturate(g.sub(g.mul(band, const_b=-1.0), const_b=-0.5))
    # No border at all when BorderWidth is 0.
    on_border = g.mul(on_border, g.saturate(g.mul(border_width, const_b=10000.0)))

    rgb = g.lerp((fill, "RGB"), (border, "RGB"), on_border)
    alpha = g.mul(inside, g.lerp((fill, "A"), (border, "A"), on_border))

    # Vertex colour's first output is its RGB.
    g.output(g.mul(rgb, (vertex, "")), unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    g.output(g.saturate(g.mul(alpha, (vertex, "A"))), unreal.MaterialProperty.MP_OPACITY)

    compile_and_save(material, MATERIAL_NAME)


build()
