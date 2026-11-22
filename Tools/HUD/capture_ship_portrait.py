"""
Captures a ship Blueprint as a square HUD portrait texture.

Usage, inside the editor (Output Log, Python console):
    py "<project>/Tools/HUD/capture_ship_portrait.py" [ShipBlueprint] [OutputTexture] [Size]
Headless (needs rendering, hence -AllowCommandletRendering):
    UnrealEditor-Cmd.exe "<project>/Startech.uproject" -run=pythonscript -AllowCommandletRendering
        -script="<project>/Tools/HUD/capture_ship_portrait.py /Game/Blueprints/BP_ShipPawn /Game/UI/HUD/T_Portrait_Player 128"

Defaults: /Game/Blueprints/BP_ShipPawn, /Game/UI/HUD/T_Portrait_Player, 128.

What it does:
    1. spawns the Blueprint in the editor world far from the origin, with a temporary directional light
    2. places a SceneCapture2D at a three-quarter view (front, right, above), framed to the ship's bounds, rendering
       only the ship
    3. captures into a render target, then draws that additively over a second render target cleared to the dark
       background (#04342C), so empty space is the background colour
    4. saves the result as a static texture with RenderingLibrary.render_target_create_static_texture2d_editor_only
       (UI texture group, no mips), replacing an existing texture at the output path
    5. deletes the temporary actors
Use the same script for enemy portraits later, with their Blueprint and an output path of their own.
"""

import math
import sys

import unreal

DEFAULT_BLUEPRINT = "/Game/Blueprints/BP_ShipPawn"
DEFAULT_OUTPUT = "/Game/UI/HUD/T_Portrait_Player"
DEFAULT_SIZE = 128
BACKGROUND_HEX = "04342C"
LOG_PREFIX = "[HUD]"

# Far from anything in the level, in cm.
SPAWN_LOCATION = unreal.Vector(1000000.0, 1000000.0, 200000.0)
FIELD_OF_VIEW = 30.0
FRAMING_MARGIN = 1.15
# From the ship toward the camera: in front, to the right, and above.
VIEW_DIRECTION = unreal.Vector(0.75, 0.55, 0.4)


def log(message):
    unreal.log(f"{LOG_PREFIX} {message}")


def fail(message):
    unreal.log_error(f"{LOG_PREFIX} {message}")
    raise RuntimeError(f"{LOG_PREFIX} {message}")


def srgb_to_linear(channel):
    c = channel / 255.0
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4


def background_colour():
    r, g, b = (int(BACKGROUND_HEX[i:i + 2], 16) for i in (0, 2, 4))
    return unreal.LinearColor(srgb_to_linear(r), srgb_to_linear(g), srgb_to_linear(b), 1.0)


def parse_arguments():
    args = [a for a in sys.argv[1:] if a]
    blueprint = args[0] if len(args) > 0 else DEFAULT_BLUEPRINT
    output = args[1] if len(args) > 1 else DEFAULT_OUTPUT
    size = int(args[2]) if len(args) > 2 else DEFAULT_SIZE
    if size < 16:
        fail(f"Size {size} is too small")
    return blueprint, output, size


def normalised(vector):
    length = math.sqrt(vector.x * vector.x + vector.y * vector.y + vector.z * vector.z)
    return unreal.Vector(vector.x / length, vector.y / length, vector.z / length)


def capture(blueprint_path, output_path, size):
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if world is None:
        fail("No editor world to capture in")
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

    ship_class = unreal.EditorAssetLibrary.load_blueprint_class(blueprint_path)
    if ship_class is None:
        fail(f"Could not load Blueprint {blueprint_path}")

    spawned = []
    try:
        ship = actors.spawn_actor_from_class(ship_class, SPAWN_LOCATION, unreal.Rotator(0.0, 0.0, 0.0))
        if ship is None:
            fail(f"Could not spawn {blueprint_path}")
        spawned.append(ship)

        origin, extent = ship.get_actor_bounds(False)
        radius = max(math.sqrt(extent.x * extent.x + extent.y * extent.y + extent.z * extent.z), 1.0)
        distance = radius * FRAMING_MARGIN / math.tan(math.radians(FIELD_OF_VIEW * 0.5))
        direction = normalised(VIEW_DIRECTION)
        camera_location = unreal.Vector(origin.x + direction.x * distance, origin.y + direction.y * distance,
                                        origin.z + direction.z * distance)
        camera_rotation = unreal.MathLibrary.find_look_at_rotation(camera_location, origin)

        # A key light from over the camera's shoulder.
        light = actors.spawn_actor_from_class(unreal.DirectionalLight, camera_location,
                                              unreal.Rotator(camera_rotation.roll, camera_rotation.pitch - 25.0,
                                                             camera_rotation.yaw + 30.0))
        if light is None:
            fail("Could not spawn the temporary light")
        spawned.append(light)

        capture_actor = actors.spawn_actor_from_class(unreal.SceneCapture2D, camera_location, camera_rotation)
        if capture_actor is None:
            fail("Could not spawn the SceneCapture2D")
        spawned.append(capture_actor)

        shot = unreal.RenderingLibrary.create_render_target2d(world, size, size,
                                                              unreal.TextureRenderTargetFormat.RTF_RGBA8,
                                                              unreal.LinearColor(0.0, 0.0, 0.0, 1.0))
        component = capture_actor.capture_component2d
        component.set_editor_property("fov_angle", FIELD_OF_VIEW)
        component.set_editor_property("capture_source", unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
        component.set_editor_property("primitive_render_mode",
                                      unreal.SceneCapturePrimitiveRenderMode.PRM_USE_SHOW_ONLY_LIST)
        component.set_editor_property("show_only_actors", [ship])
        component.set_editor_property("capture_every_frame", False)
        component.set_editor_property("capture_on_movement", False)
        component.set_editor_property("texture_target", shot)
        component.capture_scene()

        # The ship over the background colour: the capture is black where there is no ship, so adding it keeps the
        # background there and leaves the ship only faintly tinted.
        background = background_colour()
        final = unreal.RenderingLibrary.create_render_target2d(world, size, size,
                                                               unreal.TextureRenderTargetFormat.RTF_RGBA8, background)
        unreal.RenderingLibrary.clear_render_target2d(world, final, background)
        canvas, _, context = unreal.RenderingLibrary.begin_draw_canvas_to_render_target(world, final)
        canvas.draw_texture(shot, unreal.Vector2D(0.0, 0.0), unreal.Vector2D(size, size), unreal.Vector2D(0.0, 0.0),
                            unreal.Vector2D(1.0, 1.0), unreal.LinearColor(1.0, 1.0, 1.0, 1.0),
                            unreal.BlendMode.BLEND_ADDITIVE, 0.0, unreal.Vector2D(0.5, 0.5))
        unreal.RenderingLibrary.end_draw_canvas_to_render_target(world, context)

        # Replace, rather than get a "_1" copy next to the old one.
        if unreal.EditorAssetLibrary.does_asset_exist(output_path):
            if not unreal.EditorAssetLibrary.delete_asset(output_path):
                fail(f"Could not replace the existing {output_path}")

        texture = unreal.RenderingLibrary.render_target_create_static_texture2d_editor_only(
            final, output_path, unreal.TextureCompressionSettings.TC_DEFAULT,
            unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        if texture is None:
            fail("render_target_create_static_texture2d_editor_only returned nothing")
        texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
        texture.set_editor_property("srgb", True)

        saved_path = texture.get_path_name().split(".")[0]
        if not unreal.EditorAssetLibrary.save_asset(saved_path, False):
            fail(f"Could not save {saved_path}")
        log(f"Portrait of {blueprint_path} saved to {saved_path} ({size}x{size}).")
    finally:
        for actor in spawned:
            actors.destroy_actor(actor)


capture(*parse_arguments())
