"""Content for the combat balance task, done by script, not by hand.

Run headless with the editor closed:
  UnrealEditor-Cmd.exe "<Project>.uproject" -run=pythonscript
      -script="<Project>/Tools/Combat/setup_combat_balance.py" -unattended -nopause -nosplash -stdout

Optional arguments pick steps (default: all): balance ammo enemies shop loot

DA_CombatBalance is the source of every combat number at runtime. Change the constants below, run this, then check
the result headless with Startech.Combat.TTKTable (see CLAUDE.md).

Does, idempotently:
  balance  /Game/Data/Combat/DA_CombatBalance (UCombatBalance, UStartechSettings::CombatBalance), rewritten from the
           constants below: free, drop-only and locked ammo, LL4 drops, ammo damage and prices, every enemy type,
           the expected-gear cases and the targets.
  ammo     Copies AMMO's damage and base value into the ammo assets, so the editor shows what the game uses.
  enemies  Copies ENEMIES into DA_Enemy_* (hull, shield, laser, preferred range) and the enemy weapons' damage, for
           the same reason.
  shop     DA_Shop_Ammo: no LL1 or LL4 entry; LL2 and LL3 sold, never bought back.
  loot     DA_WorldLoot: no LL1 entry (it is free).
Results are reported with unreal.log_warning and "[CombatBalance]", because plain unreal.log does not reach stdout.
"""

import sys

import unreal

BALANCE_DIR = "/Game/Data/Combat"
BALANCE_NAME = "DA_CombatBalance"
AMMO_SHOP = "/Game/Data/Shops/DA_Shop_Ammo"
WORLD_LOOT = "/Game/Data/WorldLoots/DA_WorldLoot"

LL1 = "Item.Ammo.LL1"
LL2 = "Item.Ammo.LL2"
LL3 = "Item.Ammo.LL3"
LL4 = "Item.Ammo.LL4"

AMMO_ASSETS = {
    LL1: "/Game/Data/AmmoAsset/DA_1xAmmo",
    LL2: "/Game/Data/AmmoAsset/DA_2x_Ammo",
    LL3: "/Game/Data/AmmoAsset/DA_x3_Ammo",
    LL4: "/Game/Data/AmmoAsset/DA_x4_Ammo",
}

FREE_AMMO = LL1
DROP_ONLY = [LL4]

# (ammo, required tier, locked reason)
UNLOCKS = [
    (LL3, 3, "Reach Glacier (tier III)"),
]

# (tier, ammo, chance, min rounds, max rounds)
TIER_DROPS = [
    (3, LL4, 0.15, 20, 60),
    (4, LL4, 0.25, 20, 60),
]

# (ammo, damage multiplier, base value per round). The shop's SellMarkup (1.5) makes the price: 0.45 and 0.90 cr.
AMMO = [
    (LL1, 1.0, 1.00),
    (LL2, 2.0, 0.30),
    (LL3, 3.0, 0.60),
    (LL4, 4.0, 2.00),
]

WEAPONS = "/Game/Data/WeaponAsset/"
ENEMY_DATA = "/Game/Data/EnemyData/DA_Enemy_"

REGULAR = "REGULAR"
TOUGH = "TOUGH"
WEAK = "WEAK"

# (tag, data asset suffix, hull, shield, weapon, damage per shot, preferred range cm, reward multiplier,
#  [(tier, role)]). Preferred range × 1.1 (the brain's RangeTolerance) must stay inside the weapon's range.
ENEMIES = [
    ("Enemy.Drifter", "Drifter", 150.0, 200.0, "DA_BasicWeapon_Drifter", 1.0, 6500.0, 1.0,
     [(1, REGULAR), (2, WEAK), (3, WEAK), (4, WEAK)]),
    ("Enemy.Chaser", "Chaser", 372.0, 372.0, "DA_BasicWeapon_Chaser", 1.25, 3600.0, 2.0,
     [(2, REGULAR), (3, REGULAR), (4, REGULAR)]),
    ("Enemy.Ranged", "Ranged", 500.0, 500.0, "DA_BasicWeapon_Ranger", 1.4, 9000.0, 3.0,
     [(2, TOUGH), (3, TOUGH), (4, TOUGH)]),
]

# (tier, ammo, lasers level, hull level, shield level, min TTK, max TTK, tolerance s, check damage)
GEAR_CASES = [
    (1, LL1, 0, 0, 0, 3.0, 5.0, 0.0, True),
    (1, LL1, 1, 0, 0, 3.0, 5.0, 0.0, True),
    (2, LL2, 2, 1, 1, 4.0, 6.0, 0.0, True),
    (3, LL3, 3, 3, 2, 5.0, 7.0, 0.0, True),
    (4, LL3, 5, 5, 5, 8.0, 10.0, 0.25, True),
    (4, LL4, 5, 5, 5, 6.0, 8.0, 0.25, False),
]

# Radius of every laser and drainer shot, in cm (a line can pass between a ship's convex collision pieces).
SHOT_RADIUS = 50.0

ONE_FIGHT_LOSS = (0.20, 0.25)
TOUGH_FACTOR = 2.0
MAX_AMMO_COST_SHARE = 0.30

PLAYER_SHIP_CLASS = "/Game/Blueprints/BP_ShipPawn.BP_ShipPawn_C"
LOOT_SHOP = "/Game/Data/Shops/DA_Shop_Minerals"

# The ammo shop sells these, in this order, and buys none back.
SHOP_SELLS = [LL2, LL3]


def report(message):
    unreal.log_warning("[CombatBalance] " + message)


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


def good_tag(good):
    return tag_name(good.get_editor_property("item_tag")) if good else ""


def setup_balance():
    path = BALANCE_DIR + "/" + BALANCE_NAME
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        balance = load(path)
    else:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.CombatBalance)
        balance = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            BALANCE_NAME, BALANCE_DIR, unreal.CombatBalance, factory)
        if balance is None:
            raise RuntimeError("could not create " + path)

    balance.set_editor_property("free_ammo_tag", tag(FREE_AMMO))
    balance.set_editor_property("drop_only_ammo_tags", [tag(name) for name in DROP_ONLY])

    unlocks = []
    for ammo, tier, reason in UNLOCKS:
        unlock = unreal.AmmoUnlock()
        unlock.set_editor_property("ammo_tag", tag(ammo))
        unlock.set_editor_property("required_tier", tier)
        unlock.set_editor_property("locked_reason", reason)
        unlocks.append(unlock)
    balance.set_editor_property("ammo_unlocks", unlocks)

    drops = []
    for tier, ammo, chance, low, high in TIER_DROPS:
        drop = unreal.TierAmmoDrop()
        drop.set_editor_property("tier", tier)
        drop.set_editor_property("ammo_tag", tag(ammo))
        drop.set_editor_property("chance", chance)
        drop.set_editor_property("min_count", low)
        drop.set_editor_property("max_count", high)
        drops.append(drop)
    balance.set_editor_property("tier_ammo_drops", drops)

    ammo_list = []
    for name, multiplier, value in AMMO:
        entry = unreal.AmmoBalance()
        entry.set_editor_property("ammo_tag", tag(name))
        entry.set_editor_property("damage_multiplier", multiplier)
        entry.set_editor_property("base_value", value)
        ammo_list.append(entry)
    balance.set_editor_property("ammo", ammo_list)

    enemies = []
    for enemy_tag, _, hull, shield, weapon, damage, preferred, reward, roles in ENEMIES:
        entry = unreal.EnemyBalance()
        entry.set_editor_property("enemy_tag", tag(enemy_tag))
        entry.set_editor_property("hull", hull)
        entry.set_editor_property("shield", shield)
        entry.set_editor_property("laser_weapon", load(WEAPONS + weapon))
        entry.set_editor_property("damage_per_shot", damage)
        entry.set_editor_property("preferred_range", preferred)
        entry.set_editor_property("reward_multiplier", reward)
        appearances = []
        for tier, role in roles:
            appearance = unreal.EnemyTierRole()
            appearance.set_editor_property("tier", tier)
            appearance.set_editor_property("role", getattr(unreal.CombatRole, role))
            appearances.append(appearance)
        entry.set_editor_property("appearances", appearances)
        enemies.append(entry)
    balance.set_editor_property("enemies", enemies)

    cases = []
    for tier, ammo, lasers, hull, shield, low, high, tolerance, check in GEAR_CASES:
        case = unreal.CombatGearCase()
        case.set_editor_property("tier", tier)
        case.set_editor_property("ammo_tag", tag(ammo))
        case.set_editor_property("lasers_level", lasers)
        case.set_editor_property("hull_level", hull)
        case.set_editor_property("shield_level", shield)
        case.set_editor_property("min_time_to_kill", low)
        case.set_editor_property("max_time_to_kill", high)
        case.set_editor_property("tolerance_seconds", tolerance)
        case.set_editor_property("check_damage", check)
        cases.append(case)
    balance.set_editor_property("gear_cases", cases)

    balance.set_editor_property("shot_radius", SHOT_RADIUS)
    balance.set_editor_property("one_fight_loss_min", ONE_FIGHT_LOSS[0])
    balance.set_editor_property("one_fight_loss_max", ONE_FIGHT_LOSS[1])
    balance.set_editor_property("tough_factor", TOUGH_FACTOR)
    balance.set_editor_property("max_ammo_cost_share", MAX_AMMO_COST_SHARE)
    balance.set_editor_property("player_ship_class", unreal.load_class(None, PLAYER_SHIP_CLASS))
    balance.set_editor_property("ammo_shop", load(AMMO_SHOP))
    balance.set_editor_property("loot_shop", load(LOOT_SHOP))
    save(balance)

    balance = load(path)
    report("balance: free %s, drop-only %s" % (
        tag_name(balance.get_editor_property("free_ammo_tag")),
        [tag_name(each) for each in balance.get_editor_property("drop_only_ammo_tags")]))
    for unlock in balance.get_editor_property("ammo_unlocks"):
        report("balance: unlock %s at tier %d: %s" % (tag_name(unlock.get_editor_property("ammo_tag")),
                                                    unlock.get_editor_property("required_tier"),
                                                    unlock.get_editor_property("locked_reason")))
    for drop in balance.get_editor_property("tier_ammo_drops"):
        report("balance: tier %d drops %s at %.2f, %d-%d" % (drop.get_editor_property("tier"),
                                                            tag_name(drop.get_editor_property("ammo_tag")),
                                                            drop.get_editor_property("chance"),
                                                            drop.get_editor_property("min_count"),
                                                            drop.get_editor_property("max_count")))
    for entry in balance.get_editor_property("enemies"):
        report("balance: %s hull %.0f shield %.0f %s dmg %.2f pref %.0f reward x%.1f, %d tier(s)" % (
            tag_name(entry.get_editor_property("enemy_tag")), entry.get_editor_property("hull"),
            entry.get_editor_property("shield"), entry.get_editor_property("laser_weapon").get_name(),
            entry.get_editor_property("damage_per_shot"), entry.get_editor_property("preferred_range"),
            entry.get_editor_property("reward_multiplier"), len(entry.get_editor_property("appearances"))))
    report("balance: %d ammo, %d gear case(s), ship %s" % (
        len(balance.get_editor_property("ammo")), len(balance.get_editor_property("gear_cases")),
        balance.get_editor_property("player_ship_class")))


def setup_ammo():
    for name, multiplier, value in AMMO:
        path = AMMO_ASSETS[name]
        ammo = load(path)
        if good_tag(ammo) != name:
            raise RuntimeError("%s has tag %s, expected %s" % (path, good_tag(ammo), name))
        ammo.set_editor_property("damage_multiplier", multiplier)
        ammo.set_editor_property("base_value", value)
        save(ammo)
        report("ammo: %s '%s' damage x%.2f base value %.2f" % (
            name, ammo.get_editor_property("display_name"), ammo.get_editor_property("damage_multiplier"),
            ammo.get_editor_property("base_value")))


def setup_enemies():
    for _, suffix, hull, shield, weapon_name, damage, preferred, _, _ in ENEMIES:
        weapon = load(WEAPONS + weapon_name)
        weapon.set_editor_property("damage", damage)
        save(weapon)
        data = load(ENEMY_DATA + suffix)
        data.set_editor_property("max_hull", hull)
        data.set_editor_property("max_shield", shield)
        data.set_editor_property("laser_weapon", weapon)
        data.set_editor_property("preferred_range", preferred)
        save(data)
        report("enemies: %s hull %.0f shield %.0f %s (dmg %.2f, %.1f/s, %.0f m) preferred %.0f m" % (
            suffix, hull, shield, weapon_name, weapon.get_editor_property("damage"),
            weapon.get_editor_property("fire_rate"), weapon.get_editor_property("range") / 100.0, preferred / 100.0))


def setup_shop():
    shop = load(AMMO_SHOP)
    removed = [FREE_AMMO] + DROP_ONLY
    entries = []
    present = []
    for entry in shop.get_editor_property("entries"):
        name = good_tag(entry.get_editor_property("good"))
        if name in removed:
            report("shop: removed %s" % name)
            continue
        if name in SHOP_SELLS:
            entry.set_editor_property("shop_sells", True)
            entry.set_editor_property("shop_buys", False)
            present.append(name)
        entries.append(entry)

    for name in SHOP_SELLS:
        if name in present:
            continue
        entry = unreal.ShopEntry()
        entry.set_editor_property("good", load(AMMO_ASSETS[name]))
        entry.set_editor_property("shop_sells", True)
        entry.set_editor_property("shop_buys", False)
        entries.append(entry)
        report("shop: added %s" % name)

    # Laser ammo in SHOP_SELLS order first, then everything else as it was.
    def order(entry):
        name = good_tag(entry.get_editor_property("good"))
        return SHOP_SELLS.index(name) if name in SHOP_SELLS else len(SHOP_SELLS)

    entries.sort(key=order)
    shop.set_editor_property("entries", entries)
    save(shop)

    shop = load(AMMO_SHOP)
    markup = shop.get_editor_property("sell_markup")
    for entry in shop.get_editor_property("entries"):
        good = entry.get_editor_property("good")
        report("shop: %s sells %s buys %s price %.3f cr" % (
            good_tag(good), entry.get_editor_property("shop_sells"), entry.get_editor_property("shop_buys"),
            good.get_editor_property("base_value") * markup if good else 0.0))


def setup_loot():
    loot = load(WORLD_LOOT)
    entries = []
    for entry in loot.get_editor_property("ammo"):
        name = good_tag(entry.get_editor_property("ammo"))
        if name == FREE_AMMO:
            report("loot: removed %s (%.2f, %d-%d)" % (name, entry.get_editor_property("chance"),
                                                     entry.get_editor_property("min_count"),
                                                     entry.get_editor_property("max_count")))
            continue
        entries.append(entry)
    loot.set_editor_property("ammo", entries)
    save(loot)
    for entry in load(WORLD_LOOT).get_editor_property("ammo"):
        report("loot: keeps %s" % good_tag(entry.get_editor_property("ammo")))
    report("loot: %d ammo entr(ies) left" % len(entries))


def main():
    wanted = [arg.lower() for arg in sys.argv[1:]] or ["balance", "ammo", "enemies", "shop", "loot"]
    if "balance" in wanted:
        setup_balance()
    if "ammo" in wanted:
        setup_ammo()
    if "enemies" in wanted:
        setup_enemies()
    if "shop" in wanted:
        setup_shop()
    if "loot" in wanted:
        setup_loot()
    report("done: " + " ".join(wanted))


main()
