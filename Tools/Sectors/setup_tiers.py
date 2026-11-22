"""
Sector tiers content setup (Docs/Tasks/TASK_SectorTiers.md). Run with the editor closed:

  UnrealEditor-Cmd.exe Startech.uproject -run=pythonscript "-script=D:/Unreal Projects/Startech/Tools/Sectors/setup_tiers.py [steps]"

Steps (default: all of them, in this order):
  loot     DA_WorldLoot: MineralChance 0.7 (one mineral roll, picked by the sector tier's weights)
  bounty   DA_BountyTemplate_Standard: one location per sector except Terra (its own template) at its tier's KillCreditsMultiplier, plus "anywhere"
  presets  every preset's spawn rules: enemies respawn (Remember When Consumed off, RespawnDelay 30,
           MinDistanceFromPlayer 15000), loot RespawnDelay 60; Terra keeps only drifters, loot and its station;
           DA_EnvironmentPreset_Glacier and _Jove are copies of Rust, recoloured
  catalog  DA_SectorCatalog: Terra, Rust, Glacier, Jove (200000 x 200000 x 50000) with the gate chain
           Terra <-> Rust <-> Glacier <-> Jove, and the Glacier and Jove bodies of the star system

Pass forward slashes in the -script path. Results are reported with unreal.log_warning, because plain unreal.log does
not reach stdout. Every step is idempotent: it sets values, never appends.
"""

import re
import sys

import unreal

WORLD_LOOT = "/Game/Data/WorldLoots/DA_WorldLoot"
BOUNTY_TEMPLATE = "/Game/Quests/DA_BountyTemplate_Standard"

MINERAL_CHANCE = 0.7

# (context id, shown text, reward multiplier = the sector tier's KillCreditsMultiplier); "" is anywhere.
# Terra has its own template, DA_BountyTemplate_Terra (drifters only), made by Tools/Upgrades/setup_upgrades.py.
BOUNTY_LOCATIONS = [
    ("Rust", "in Rust", 1.8),
    ("Glacier", "in Glacier", 3.0),
    ("Jove", "in Jove", 5.0),
    ("", "anywhere", 1.0),
]


PRESET_DIR = "/Game/Environment/Data"
CATALOG = PRESET_DIR + "/DA_SectorCatalog"

ENEMY_RESPAWN_DELAY = 30.0
ENEMY_MIN_DISTANCE = 15000.0
LOOT_RESPAWN_DELAY = 60.0

# Enemy classes Terra keeps (tier I: weak enemies only).
TERRA_ENEMIES = ["BP_Enemy_Drifter"]

PLAY_AREA = (200000.0, 200000.0, 50000.0)

# id, display name, seed, preset, gates [(destination, x)]
SECTORS = [
    ("Terra", "Terra", 1001, "DA_EnvironmentPreset_Terra", [("Rust", 0.925)]),
    ("Rust", "Rust", 2001, "DA_EnvironmentPreset_Rust", [("Terra", -0.925), ("Glacier", 0.925)]),
    ("Glacier", "Glacier", 3001, "DA_EnvironmentPreset_Glacier", [("Rust", -0.925), ("Jove", 0.925)]),
    ("Jove", "Jove", 4001, "DA_EnvironmentPreset_Jove", [("Glacier", -0.925)]),
]


def color(r, g, b, a=1.0):
    return unreal.LinearColor(r, g, b, a)


# Star system bodies added next to Terra and Rust, which are kept as they are.
BODIES = {
    "Glacier": dict(orbit_radius=3.0, orbit_angle_degrees=150.0, orbit_height=-0.2, body_radius=0.9,
                    color_a=color(0.72, 0.86, 1.0), color_b=color(0.30, 0.55, 0.85), moons=[]),
    "Jove": dict(orbit_radius=5.2, orbit_angle_degrees=250.0, orbit_height=0.3, body_radius=6.0,
                 color_a=color(0.95, 0.55, 0.20), color_b=color(0.45, 0.22, 0.08),
                 moons=[("Jove I", unreal.Rotator(roll=0.0, pitch=10.0, yaw=30.0), 0.10, color(0.62, 0.60, 0.56), color(0.35, 0.33, 0.30)),
                        ("Jove II", unreal.Rotator(roll=0.0, pitch=-8.0, yaw=-45.0), 0.07, color(0.70, 0.55, 0.38), color(0.40, 0.28, 0.18))]),
}

# Copies of Rust: planet surface A/B, cloud colour, atmosphere, nebula A/B, asteroid tint.
RECOLOURS = {
    "DA_EnvironmentPreset_Glacier": dict(
        surface_a=color(0.88, 0.94, 1.0), surface_b=color(0.45, 0.66, 0.90), cloud=color(0.95, 0.98, 1.0),
        atmosphere=color(0.30, 0.90, 1.0), nebula_a=color(0.04, 0.20, 0.45), nebula_b=color(0.03, 0.42, 0.42),
        asteroids=color(0.55, 0.60, 0.70)),
    "DA_EnvironmentPreset_Jove": dict(
        surface_a=color(0.92, 0.48, 0.14), surface_b=color(0.32, 0.15, 0.05), cloud=color(1.0, 0.78, 0.42),
        atmosphere=color(1.0, 0.74, 0.22), nebula_a=color(0.32, 0.06, 0.45), nebula_b=color(0.90, 0.40, 0.10),
        asteroids=color(0.07, 0.06, 0.06)),
}


def report(message):
    unreal.log_warning("[SectorTiers] " + message)


def load(path):
    asset = unreal.load_asset(path)
    if asset is None:
        raise RuntimeError("missing asset " + path)
    return asset


def save(asset):
    if not unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError("could not save " + asset.get_path_name())


def setup_loot():
    table = load(WORLD_LOOT)
    table.set_editor_property("mineral_chance", MINERAL_CHANCE)
    save(table)
    report("loot: %s MineralChance = %s" % (WORLD_LOOT, table.get_editor_property("mineral_chance")))


def setup_bounty():
    template = load(BOUNTY_TEMPLATE)
    locations = []
    for context_id, text, multiplier in BOUNTY_LOCATIONS:
        option = unreal.BountyLocationOption()
        option.set_editor_property("context_id", context_id)
        option.set_editor_property("display_name", text)
        option.set_editor_property("weight", 1.0)
        option.set_editor_property("reward_multiplier", multiplier)
        locations.append(option)
    template.set_editor_property("locations", locations)
    save(template)
    for option in template.get_editor_property("locations"):
        report("bounty: location '%s' (%s) x%s" % (option.get_editor_property("context_id"),
                                                   option.get_editor_property("display_name"),
                                                   option.get_editor_property("reward_multiplier")))


def set_bool(struct, name, value):
    """Sets a bool property whose Python name may or may not keep the C++ 'b' prefix."""
    try:
        struct.set_editor_property(name, value)
    except Exception:
        struct.set_editor_property("b_" + name, value)


def get_bool(struct, name):
    try:
        return struct.get_editor_property(name)
    except Exception:
        return struct.get_editor_property("b_" + name)


def class_path(entry):
    return str(entry.get_editor_property("actor_class"))


def is_enemy(entry):
    return "/BP_Enemy_" in class_path(entry)


def is_loot(entry):
    return "BP_ZoneLoot" in class_path(entry)


def short_name(entry):
    match = re.search(r"/(\w+)\.\w+", class_path(entry))
    return match.group(1) if match else class_path(entry)


def apply_spawn_rules(preset, keep_enemies=None):
    """Enemies respawn, loot every 60 s; keep_enemies, when given, drops every other enemy class."""
    # Struct arrays come back as copies: every level is rebuilt and set back.
    params = preset.get_editor_property("base_params")
    zones = []
    for zone in params.get_editor_property("zones"):
        kept = []
        for entry in zone.get_editor_property("entries"):
            if is_enemy(entry):
                if keep_enemies is not None and not any("/%s." % name in class_path(entry) for name in keep_enemies):
                    report("presets: %s %s: removed %s" % (preset.get_name(), zone.get_editor_property("zone_name"),
                                                          short_name(entry)))
                    continue
                set_bool(entry, "remember_when_consumed", False)
                entry.set_editor_property("respawn_delay", ENEMY_RESPAWN_DELAY)
                entry.set_editor_property("min_distance_from_player", ENEMY_MIN_DISTANCE)
            elif is_loot(entry):
                entry.set_editor_property("respawn_delay", LOOT_RESPAWN_DELAY)
            kept.append(entry)
        zone.set_editor_property("entries", kept)
        zones.append(zone)
    params.set_editor_property("zones", zones)
    preset.set_editor_property("base_params", params)


def recolour(preset, colours):
    params = preset.get_editor_property("base_params")

    planet = params.get_editor_property("planet")
    planet.set_editor_property("surface_color_a", colours["surface_a"])
    planet.set_editor_property("surface_color_b", colours["surface_b"])
    planet.set_editor_property("cloud_color", colours["cloud"])
    params.set_editor_property("planet", planet)

    atmosphere = params.get_editor_property("atmosphere")
    atmosphere.set_editor_property("color", colours["atmosphere"])
    params.set_editor_property("atmosphere", atmosphere)

    backdrop = params.get_editor_property("backdrop")
    backdrop.set_editor_property("nebula_color", colours["nebula_a"])
    backdrop.set_editor_property("nebula_color_b", colours["nebula_b"])
    params.set_editor_property("backdrop", backdrop)

    asteroids = params.get_editor_property("asteroids")
    asteroids.set_editor_property("tint", colours["asteroids"])
    types = []
    for asteroid_type in asteroids.get_editor_property("types"):
        asteroid_type.set_editor_property("tint", colours["asteroids"])
        types.append(asteroid_type)
    asteroids.set_editor_property("types", types)
    params.set_editor_property("asteroids", asteroids)

    preset.set_editor_property("base_params", params)


def describe_preset(preset):
    params = preset.get_editor_property("base_params")
    for zone in params.get_editor_property("zones"):
        parts = []
        for entry in zone.get_editor_property("entries"):
            parts.append("%s %d-%d remember=%s respawn=%g mindist=%g" % (
                short_name(entry), entry.get_editor_property("min_count"), entry.get_editor_property("max_count"),
                get_bool(entry, "remember_when_consumed"), entry.get_editor_property("respawn_delay"),
                entry.get_editor_property("min_distance_from_player")))
        report("presets: %s %s: %s" % (preset.get_name(), zone.get_editor_property("zone_name"),
                                       "; ".join(parts) if parts else "(empty)"))


def setup_presets():
    rust = load(PRESET_DIR + "/DA_EnvironmentPreset_Rust")
    apply_spawn_rules(rust)
    save(rust)

    terra = load(PRESET_DIR + "/DA_EnvironmentPreset_Terra")
    apply_spawn_rules(terra, keep_enemies=TERRA_ENEMIES)
    save(terra)

    for name, colours in RECOLOURS.items():
        path = PRESET_DIR + "/" + name
        if not unreal.EditorAssetLibrary.does_asset_exist(path):
            if unreal.EditorAssetLibrary.duplicate_asset(rust.get_path_name().split(".")[0], path) is None:
                raise RuntimeError("could not copy Rust to " + path)
            report("presets: created %s from Rust" % name)
        copy = load(path)
        # Always the whole of Rust's values again, so a rerun matches Rust's current spawns.
        copy.set_editor_property("base_params", rust.get_editor_property("base_params"))
        copy.set_editor_property("hue_jitter_degrees", rust.get_editor_property("hue_jitter_degrees"))
        copy.set_editor_property("scalar_jitter_fraction", rust.get_editor_property("scalar_jitter_fraction"))
        recolour(copy, colours)
        save(copy)

    for name in ["DA_EnvironmentPreset_Terra", "DA_EnvironmentPreset_Rust"] + list(RECOLOURS.keys()):
        describe_preset(load(PRESET_DIR + "/" + name))


def make_body(body_id, values):
    body = unreal.CelestialBodyDefinition()
    body.set_editor_property("body_id", body_id)
    for key in ["orbit_radius", "orbit_angle_degrees", "orbit_height", "body_radius", "color_a", "color_b"]:
        body.set_editor_property(key, values[key])
    moons = []
    for moon_id, offset, radius, colour_a, colour_b in values["moons"]:
        moon = unreal.MoonDefinition()
        moon.set_editor_property("moon_id", moon_id)
        moon.set_editor_property("offset_from_planet", offset)
        moon.set_editor_property("relative_radius", radius)
        moon.set_editor_property("color_a", colour_a)
        moon.set_editor_property("color_b", colour_b)
        moons.append(moon)
    body.set_editor_property("moons", moons)
    return body


def setup_catalog():
    catalog = load(CATALOG)

    existing = {str(sector.get_editor_property("sector_id")): sector for sector in catalog.get_editor_property("sectors")}
    sectors = []
    for sector_id, display_name, seed, preset_name, gates in SECTORS:
        sector = existing.get(sector_id) or unreal.SectorDefinition()
        sector.set_editor_property("sector_id", sector_id)
        sector.set_editor_property("display_name", display_name)
        sector.set_editor_property("seed", seed)
        sector.set_editor_property("body_id", sector_id)
        sector.set_editor_property("preset", load(PRESET_DIR + "/" + preset_name))
        sector.set_editor_property("play_area_extent", unreal.Vector(*PLAY_AREA))
        links = []
        for destination, x in gates:
            link = unreal.GateLink()
            link.set_editor_property("destination_sector_id", destination)
            link.set_editor_property("normalized_location", unreal.Vector(x, 0.0, 0.0))
            links.append(link)
        sector.set_editor_property("gates", links)
        sectors.append(sector)
    catalog.set_editor_property("sectors", sectors)

    system = catalog.get_editor_property("star_system")
    bodies = [body for body in system.get_editor_property("bodies")
              if str(body.get_editor_property("body_id")) not in BODIES]
    for body_id, values in BODIES.items():
        bodies.append(make_body(body_id, values))
    system.set_editor_property("bodies", bodies)
    catalog.set_editor_property("star_system", system)
    save(catalog)

    for sector in catalog.get_editor_property("sectors"):
        report("catalog: %s seed %d body %s preset %s extent %s gates %s" % (
            sector.get_editor_property("sector_id"), sector.get_editor_property("seed"),
            sector.get_editor_property("body_id"), sector.get_editor_property("preset").get_name(),
            sector.get_editor_property("play_area_extent"),
            ", ".join("%s at %.3f" % (gate.get_editor_property("destination_sector_id"),
                                      gate.get_editor_property("normalized_location").x)
                      for gate in sector.get_editor_property("gates"))))
    for body in catalog.get_editor_property("star_system").get_editor_property("bodies"):
        report("catalog: body %s orbit %g angle %g height %g radius %g moons %d" % (
            body.get_editor_property("body_id"), body.get_editor_property("orbit_radius"),
            body.get_editor_property("orbit_angle_degrees"), body.get_editor_property("orbit_height"),
            body.get_editor_property("body_radius"), len(body.get_editor_property("moons"))))


STEPS = [("loot", setup_loot), ("bounty", setup_bounty), ("presets", setup_presets), ("catalog", setup_catalog)]


def main():
    wanted = [arg.lower() for arg in sys.argv[1:]]
    errors = 0
    for name, step in STEPS:
        if wanted and name not in wanted:
            continue
        try:
            step()
        except Exception as error:  # report every step, keep going
            errors += 1
            report("ERROR in %s: %s" % (name, error))
    report("done, %d error(s)" % errors)


main()
