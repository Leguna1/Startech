"""Content for the upgrades task (Phase 2), done by script, not by hand.

Run headless with the editor closed:
  UnrealEditor-Cmd.exe "<Project>.uproject" -run=pythonscript
      -script="<Project>/Tools/Upgrades/setup_upgrades.py" -unattended -nopause -nosplash -stdout

Optional arguments pick steps (default: all): catalog ship shop bounty

Does, idempotently:
  catalog  /Game/Data/Upgrades/DA_UpgradeCatalog (UUpgradeCatalog): the eight tracks with their bonuses and the
           shared cost ladder (2k + 20 Ferrite ... 50k + 30 Aurium). Rewritten from TRACKS on every run.
  ship     BP_ShipPawn's HealthComponent MaxShield = 50 (1000 was a test value); MaxHull stays 100.
  shop     DA_Shop_Minerals: the shop stops selling Cobaltine and Aurium (still buys them); Ferrite unchanged.
  bounty   /Game/Quests/DA_BountyTemplate_Terra: a copy of DA_BountyTemplate_Standard with drifters only and the
           single location Terra. The Standard template's Terra location is removed by Tools/Sectors/setup_tiers.py
           bounty (run it after this). Both are listed in QuestFrameworkSettings BountyTemplates (DefaultGame.ini).
Results are reported with unreal.log_warning and "[Upgrades]", because plain unreal.log does not reach stdout.
"""

import sys

import unreal

CATALOG_DIR = "/Game/Data/Upgrades"
CATALOG_NAME = "DA_UpgradeCatalog"
SHIP = "/Game/Blueprints/BP_ShipPawn"
SHOP = "/Game/Data/Shops/DA_Shop_Minerals"
BOUNTY_STANDARD = "/Game/Quests/DA_BountyTemplate_Standard"
BOUNTY_TERRA = "/Game/Quests/DA_BountyTemplate_Terra"

PLAYER_MAX_SHIELD = 50.0

# Minerals the shop no longer sells (gating levels 3-5 behind the outer sectors). It still buys them.
SHOP_STOPS_SELLING = ["Item.Mineral.Cobaltine", "Item.Mineral.Aurium"]

FERRITE = "Item.Mineral.Ferrite"
COBALTINE = "Item.Mineral.Cobaltine"
AURIUM = "Item.Mineral.Aurium"

# (credits, mineral tag, count) per level 1-5; the same ladder for every track to start with.
COSTS = [
    (2000, FERRITE, 20),
    (5000, FERRITE, 50),
    (12000, COBALTINE, 25),
    (25000, COBALTINE, 60),
    (50000, AURIUM, 30),
]


def steps(per_level):
    """The total bonus at levels 1-5 for an equal step per level."""
    return [per_level * level for level in range(1, 6)]


PERCENT = "PERCENT"
FLAT = "FLAT"

# (track id, name, description, [(stat tag, stat label, kind, totals at levels 1-5)])
TRACKS = [
    ("Hull", "Hull plating", "Thicker plating: more hull.",
     [("Stat.Hull.Max", "Hull", FLAT, steps(100.0))]),
    ("ShieldCapacity", "Shield capacity", "A bigger shield generator: more shield.",
     [("Stat.Shield.Max", "Shield", FLAT, steps(100.0))]),
    ("ShieldRecharge", "Shield recharge", "The shield refills faster once it starts recovering.",
     [("Stat.Shield.Regen", "Recharge", PERCENT, steps(0.15))]),
    ("Cargo", "Cargo hold", "More room for minerals.",
     [("Stat.Cargo.Capacity", "Cargo", FLAT, steps(20.0))]),
    ("Engines", "Engines", "Higher top speed and acceleration.",
     [("Stat.Engine.Speed", "Speed", PERCENT, steps(0.06))]),
    ("Lasers", "Lasers", "More damage from every laser.",
     [("Stat.Laser.Damage", "Laser damage", PERCENT, steps(0.12))]),
    ("Drainer", "Drainer", "The shield drainer drains faster.",
     [("Stat.Drainer.Power", "Drain", PERCENT, steps(0.12))]),
    ("Booster", "Booster", "A bigger boost pool that refills faster.",
     [("Stat.Boost.Energy", "Boost", PERCENT, steps(0.20)),
      ("Stat.Boost.Regen", "Boost recharge", PERCENT, steps(0.15))]),
]


def report(message):
    unreal.log_warning("[Upgrades] " + message)


def load(path):
    asset = unreal.EditorAssetLibrary.load_asset(path)
    if asset is None:
        raise RuntimeError("Asset not found: " + path)
    return asset


def save(asset):
    if not unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError("could not save " + asset.get_path_name())


def tag(name):
    result = unreal.GameplayTag()
    result.import_text('(TagName="%s")' % name)
    if str(result.export_text()).find(name) < 0:
        raise RuntimeError("unknown gameplay tag " + name)
    return result


def tag_name(value):
    text = str(value.export_text())
    start = text.find('"')
    return text[start + 1:text.rfind('"')] if start >= 0 else text


def setup_catalog():
    path = CATALOG_DIR + "/" + CATALOG_NAME
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        catalog = load(path)
    else:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.UpgradeCatalog)
        catalog = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            CATALOG_NAME, CATALOG_DIR, unreal.UpgradeCatalog, factory)
        if catalog is None:
            raise RuntimeError("could not create " + path)

    tracks = []
    for track_id, name, description, effects in TRACKS:
        track = unreal.UpgradeTrack()
        track.set_editor_property("track_id", track_id)
        track.set_editor_property("display_name", name)
        track.set_editor_property("description", description)

        effect_list = []
        for stat, label, kind, totals in effects:
            effect = unreal.UpgradeStatEffect()
            effect.set_editor_property("stat_tag", tag(stat))
            effect.set_editor_property("stat_label", label)
            effect.set_editor_property("kind", getattr(unreal.UpgradeBonusKind, kind))
            effect.set_editor_property("bonus_at_level", [float(value) for value in totals])
            effect_list.append(effect)
        track.set_editor_property("effects", effect_list)

        costs = []
        for credits, mineral, count in COSTS:
            cost = unreal.UpgradeLevelCost()
            cost.set_editor_property("credits", credits)
            mineral_cost = unreal.UpgradeMineralCost()
            mineral_cost.set_editor_property("mineral_tag", tag(mineral))
            mineral_cost.set_editor_property("count", count)
            cost.set_editor_property("minerals", [mineral_cost])
            costs.append(cost)
        track.set_editor_property("level_costs", costs)
        tracks.append(track)

    catalog.set_editor_property("tracks", tracks)
    save(catalog)
    for track in catalog.get_editor_property("tracks"):
        effects = ", ".join("%s %s %s" % (tag_name(e.get_editor_property("stat_tag")), e.get_editor_property("kind"),
                                          list(e.get_editor_property("bonus_at_level")))
                            for e in track.get_editor_property("effects"))
        report("catalog: %s (%s) levels %d: %s" % (track.get_editor_property("track_id"),
                                                   track.get_editor_property("display_name"),
                                                   len(track.get_editor_property("level_costs")), effects))


def setup_ship():
    blueprint = load(SHIP)
    cdo = unreal.get_default_object(blueprint.generated_class())
    health = cdo.get_editor_property("health_component")
    health.modify()
    health.set_editor_property("max_shield", PLAYER_MAX_SHIELD)
    save(blueprint)
    report("ship: %s MaxHull %s MaxShield %s regen %s/s" % (SHIP, health.get_editor_property("max_hull"),
                                                          health.get_editor_property("max_shield"),
                                                          health.get_editor_property("shield_regen_rate")))


def setup_shop():
    shop = load(SHOP)
    entries = []
    for entry in shop.get_editor_property("entries"):
        good = entry.get_editor_property("good")
        name = tag_name(good.get_editor_property("item_tag")) if good else ""
        if name in SHOP_STOPS_SELLING:
            entry.set_editor_property("shop_sells", False)
        entries.append(entry)
    shop.set_editor_property("entries", entries)
    save(shop)
    for entry in shop.get_editor_property("entries"):
        good = entry.get_editor_property("good")
        report("shop: %s sells %s buys %s" % (good.get_name() if good else "none",
                                             entry.get_editor_property("shop_sells"),
                                             entry.get_editor_property("shop_buys")))


def setup_bounty():
    if not unreal.EditorAssetLibrary.does_asset_exist(BOUNTY_TERRA):
        if not unreal.EditorAssetLibrary.duplicate_asset(BOUNTY_STANDARD, BOUNTY_TERRA):
            raise RuntimeError("could not duplicate " + BOUNTY_STANDARD)
    template = load(BOUNTY_TERRA)
    template.set_editor_property("template_id", "TerraBounty")

    targets = []
    for target in template.get_editor_property("targets"):
        if "Enemy.Drifter" in str(target.get_editor_property("subject_tags").export_text()):
            targets.append(target)
    if not targets:
        raise RuntimeError("no drifter target in " + BOUNTY_STANDARD)
    template.set_editor_property("targets", targets)

    option = unreal.BountyLocationOption()
    option.set_editor_property("context_id", "Terra")
    option.set_editor_property("display_name", "in Terra")
    option.set_editor_property("weight", 1.0)
    option.set_editor_property("reward_multiplier", 1.0)
    template.set_editor_property("locations", [option])
    save(template)

    report("bounty: %s id %s" % (BOUNTY_TERRA, template.get_editor_property("template_id")))
    for target in template.get_editor_property("targets"):
        report("bounty: target %s %d-%d x%d" % (target.get_editor_property("display_name"),
                                              target.get_editor_property("min_count"),
                                              target.get_editor_property("max_count"),
                                              target.get_editor_property("reward_per_unit")))
    for option in template.get_editor_property("locations"):
        report("bounty: location '%s' (%s) x%s" % (option.get_editor_property("context_id"),
                                                   option.get_editor_property("display_name"),
                                                   option.get_editor_property("reward_multiplier")))
    standard = load(BOUNTY_STANDARD)
    report("bounty: standard id %s, %d target(s)" % (standard.get_editor_property("template_id"),
                                                    len(standard.get_editor_property("targets"))))


def main():
    wanted = [arg.lower() for arg in sys.argv[1:]] or ["catalog", "ship", "shop", "bounty"]
    if "catalog" in wanted:
        setup_catalog()
    if "ship" in wanted:
        setup_ship()
    if "shop" in wanted:
        setup_shop()
    if "bounty" in wanted:
        setup_bounty()
    report("done: " + " ".join(wanted))


main()
