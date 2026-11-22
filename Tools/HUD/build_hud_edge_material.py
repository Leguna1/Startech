"""
Builds M_HUD_EdgeFeedback, the HUD's full-screen edge glow (shield ripple, hull flash, out-of-bounds tint).

Run headless (editor closed, Startech editor module built):
    UnrealEditor-Cmd.exe "<project>/Startech.uproject" -run=pythonscript -script="<project>/Tools/HUD/build_hud_edge_material.py"
or inside the editor: Tools > Execute Python Script... > pick this file

The material (domain User Interface, blend mode Translucent), drawn over the whole screen by UHudScreenFeedbackWidget,
uses only TexCoord[0] (0 to 1 across the widget), its parameters, vertex colour and plain math:
    Color        vector: the glow colour (its alpha scales the glow too)
    Intensity    scalar: overall strength, 0 to 1
    Direction    vector: a unit 2D screen direction in x, y (y down; (0, -1) is the top edge), or zero to light all
                 edges evenly; otherwise the glow is weighted toward the edge it points at
    Width        scalar: how far the glow reaches in from an edge, as a fraction of half the widget's height
    AspectRatio  scalar: widget width / height, set from C++, so the glow is equally thick on every edge
The widget's opacity (vertex colour alpha) applies.

Running it again rebuilds the graph of the existing asset. Any connection that fails, or any shader compile error,
raises an error and the asset isn't saved.
"""

import os
import sys

import unreal

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
from hud_material_graph import Graph, compile_and_save, get_or_create_material  # noqa: E402

MATERIAL_FOLDER = "/Game/UI/HUD"
MATERIAL_NAME = "M_HUD_EdgeFeedback"


def build():
    material = get_or_create_material(MATERIAL_FOLDER, MATERIAL_NAME)
    material.set_editor_property("material_domain", unreal.MaterialDomain.MD_UI)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)

    g = Graph(material)
    color = g.vector("Color", unreal.LinearColor(0.235, 0.474, 0.831, 1.0))
    intensity = g.scalar("Intensity", 1.0)
    direction = g.vector("Direction", unreal.LinearColor(0.0, 0.0, 0.0, 0.0))
    width = g.scalar("Width", 0.18)
    aspect = g.scalar("AspectRatio", 16.0 / 9.0)
    vertex = g.vertex_color()

    # C: -1 to 1 across the widget, centre at 0.
    centred = g.sub(g.mul(g.uv(), const_b=2.0), const_b=1.0)

    # Distance to the nearest edge, in units of half the height: x is stretched by the aspect ratio.
    inset = g.one_minus(g.abs(centred))
    inset_x = g.mul(g.mask(inset, r=True), aspect)
    inset_y = g.mask(inset, g=True)
    edge_distance = g.min(inset_x, inset_y)

    # Glow: 1 at the edge, falling to 0 at Width in, squared for a soft tail.
    reach = g.max(width, const_b=0.0001)
    glow = g.saturate(g.one_minus(g.div(edge_distance, reach)))
    glow = g.mul(glow, glow)

    # Direction weight: how much this pixel faces the hit, squared. A zero Direction gives weight 1 everywhere.
    centred_length = g.max(g.sqrt(g.dot(centred, centred)), const_b=0.0001)
    facing = g.div(centred, centred_length)
    direction_xy = g.mask((direction, "RGB"), r=True, g=True)
    toward = g.saturate(g.dot(facing, direction_xy))
    toward = g.mul(toward, toward)
    directional = g.saturate(g.sqrt(g.dot(direction_xy, direction_xy)))
    weight = g.lerp(None, toward, directional, const_a=1.0)

    # Opacity: glow x weight x intensity x colour alpha x widget opacity.
    alpha = g.mul(glow, weight)
    alpha = g.mul(alpha, intensity)
    alpha = g.mul(alpha, (color, "A"))
    alpha = g.saturate(g.mul(alpha, (vertex, "A")))

    g.output((color, "RGB"), unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    g.output(alpha, unreal.MaterialProperty.MP_OPACITY)

    compile_and_save(material, MATERIAL_NAME)


build()
