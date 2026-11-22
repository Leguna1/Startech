"""
Creates (or refreshes) DA_HudStyle, the UStartechHudStyle data asset holding every visual constant of the HUD.

Run after build_hud_panel_material.py and build_hud_edge_material.py, so the two materials can be assigned:
    UnrealEditor-Cmd.exe "<project>/Startech.uproject" -run=pythonscript -script="<project>/Tools/HUD/create_hud_style.py"
or inside the editor: Tools > Execute Python Script... > pick this file

Colours are given as sRGB hex and stored as the linear colours Slate expects, exactly as UStartechHudStyle::FromHex
does, so the asset matches the class defaults. Running it again overwrites the values in the existing asset.
"""

import unreal

ASSET_FOLDER = "/Game/UI/HUD"
ASSET_NAME = "DA_HudStyle"
PANEL_MATERIAL = "/Game/UI/HUD/M_HUD_ChamferPanel"
EDGE_MATERIAL = "/Game/UI/HUD/M_HUD_EdgeFeedback"
LOG_PREFIX = "[HUD]"

# Fixed meanings; never reuse a colour for another purpose.
COLOURS = {
    "player_color": ("5DCAA5", 1.0),
    "shield_color": ("85B7EB", 1.0),
    "hull_color": ("EF9F27", 1.0),
    "danger_color": ("E24B4A", 1.0),
    "danger_text_color": ("F09595", 1.0),
    "advantage_color": ("97C459", 1.0),
    "credits_color": ("FAC775", 1.0),
    "text_color": ("D3D1C7", 1.0),
    "text_secondary_color": ("888780", 1.0),
    "empty_segment_color": ("2C2C2A", 1.0),
    "panel_fill_color": ("042C53", 0.35),
    "soft_lock_color": ("B4B2A9", 1.0),
}

VALUES = {
    # Gauges
    "health_segments": 10,
    "cargo_segments": 10,
    "cargo_warn_fraction": 0.8,
    "low_ammo_threshold": 100,
    "max_ammo_segments": 20,
    # Timings, seconds
    "segment_flicker_time": 0.25,
    "segment_flicker_rate": 24.0,
    "credits_count_time": 0.6,
    "pickup_line_lifetime": 3.0,
    "pickup_max_lines": 4,
    "lock_snap_time": 0.15,
    "fold_time": 0.3,
    "boot_time": 0.6,
    "boot_stagger": 0.08,
    "folded_scale": 0.6,
    "sector_banner_time": 2.5,
    "hit_effect_time": 0.4,
    "credits_tint_time": 0.8,
    "low_hull_pulse_rate": 1.5,
    # Fonts, points of the engine's Roboto
    "small_font_size": 9,
    "body_font_size": 12,
    "heading_font_size": 14,
    "credits_font_size": 16,
    "banner_font_size": 24,
    # Reticle, pixels
    "arc_radius": 46.0,
    "tick_ring_radius": 58.0,
    "arc_thickness": 4.0,
    "min_frame_size": 24.0,
    "reticle_text_offset": 80.0,
    # Optional hard-lock ring, off by default
    "show_lock_ring": False,
    "lock_ring_seconds_per_turn": 18.0,
    "lock_ring_opacity": 0.5,
    "lock_ring_dot_count": 24,
    # Markers
    "marker_hide_distance": 30000.0,
    "marker_size": 12.0,
    # Panels and feedback
    "panel_corner_size": 10.0,
    "panel_border_width": 1.0,
    "edge_glow_width": 0.18,
    "out_of_bounds_pulse_min": 0.15,
    "out_of_bounds_pulse_max": 0.7,
    "out_of_bounds_pulse_rate": 1.0,
    "out_of_bounds_pulse_rate_urgent": 3.0,
    "out_of_bounds_urgent_time": 3.0,
    "cockpit_frame_inset": 24.0,
    "cockpit_frame_opacity": 0.3,
}


def log(message):
    unreal.log(f"{LOG_PREFIX} {message}")


def fail(message):
    unreal.log_error(f"{LOG_PREFIX} {message}")
    raise RuntimeError(f"{LOG_PREFIX} {message}")


def srgb_to_linear(channel):
    c = channel / 255.0
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4


def hex_colour(hex_value, alpha):
    r, g, b = (int(hex_value[i:i + 2], 16) for i in (0, 2, 4))
    return unreal.LinearColor(srgb_to_linear(r), srgb_to_linear(g), srgb_to_linear(b), alpha)


def get_or_create_style():
    path = f"{ASSET_FOLDER}/{ASSET_NAME}"
    style_class = getattr(unreal, "StartechHudStyle", None)
    if style_class is None:
        fail("UStartechHudStyle isn't loaded. Build the Startech editor module first.")

    if unreal.EditorAssetLibrary.does_asset_exist(path):
        log(f"Updating {path}")
        return unreal.EditorAssetLibrary.load_asset(path)

    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", style_class)
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    asset = tools.create_asset(ASSET_NAME, ASSET_FOLDER, style_class, factory)
    if asset is None:
        fail(f"Could not create {path}")
    log(f"Created {path}")
    return asset


def load_material(path):
    if not unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.log_warning(f"{LOG_PREFIX} {path} doesn't exist yet; run its build script, then this one again.")
        return None
    return unreal.EditorAssetLibrary.load_asset(path)


def build():
    style = get_or_create_style()

    for name, (hex_value, alpha) in COLOURS.items():
        style.set_editor_property(name, hex_colour(hex_value, alpha))
    for name, value in VALUES.items():
        style.set_editor_property(name, value)

    panel = load_material(PANEL_MATERIAL)
    edge = load_material(EDGE_MATERIAL)
    if panel:
        style.set_editor_property("panel_material", panel)
    if edge:
        style.set_editor_property("edge_feedback_material", edge)

    if not unreal.EditorAssetLibrary.save_loaded_asset(style, False):
        fail(f"Could not save {ASSET_NAME}")
    log(f"{ASSET_NAME} saved (panel material: {'set' if panel else 'missing'}, edge material: {'set' if edge else 'missing'}).")


build()
