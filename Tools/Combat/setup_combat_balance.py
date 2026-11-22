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
#  [(tier, role)]). Preferred range Ã— 1.1 (the brain's RangeTolerance) must stay inside the weapon's range.
ENEMIES = [
    ("Enemy.Drifter", "Drifter", 150.0, 200.0, "DA_BasicWeapon_Drifter", 1.0, 6500.0, 1.0,
     [(1, REGULAR), (2, WEAK), (3, WEAK), (4, WEAK)]),
    ("Enemy.Chaser", "Chaser", 372.0, 372.0, "DA_BasicWeapon_Chaser", 1.9, 3300.0, 2.0,
     [(2, REGULAR), (3, REGULAR), (4, REGULAR)]),
    ("Enemy.Ranged", "Ranged", 500.0, 500.0, "DA_BasicWeapon_Ranger", 0.7, 9000.0, 3.0,
     [(2, TOUGH), (3, TOUGH), (4, TOUGH)]),
]

# Behaviour per type (FEnemyBehaviourBalance). Missing keys keep the struct's default (0 = off, or the EnemyData value).
#   forward_speed        cm/s (also written to the EnemyData); 0 keeps EnemyData's
#   forward_speed_by_tier  [tier I, II, III, IV] cm/s; 0 entries use forward_speed. The chaser matches the player's
#                        reverse at the expected Engines level (2500 / 2950 / 3250 in tiers II / III / IV), so backing off
#                        holds distance rather than gaining it, and stays slower than the boost once afterburning.
#   home_leash_range     cm from home (also written to the EnemyData); 0 keeps EnemyData's
#   leash_quiet_seconds  give up after this long without damage (dealt counts too with quiet_counts_dealt)
#   leash_far_range_factor / leash_far_seconds   give up after N s with the player beyond factor x laser range
#   circle               strafe round the player in range, approach a flank point out of range
#   afterburn_*          multiplier (x forward speed), seconds, cooldown, trigger factor (x laser range)
#   retreat_fraction     within this x preferred range it turns and flees without firing, until back at preferred range
#   charged_shot         telegraphed, dodgeable projectile; charge_* / charged_shot_* are its numbers. Damage is before
#                        the tier multiplier: started at 40 (about 2.5 s of the ranged enemy's old normal fire, with its
#                        normal shot halved to 0.7); Phase 3 set it to 45: the TTK table's all-hit rows land at 26-30%
#                        and live fights with nothing dodged about 24-27% (a third of charged shots miss a player who
#                        holds still).
BEHAVIOUR = {
    "Enemy.Drifter": dict(leash_quiet_seconds=20.0, leash_far_range_factor=2.0, leash_far_seconds=8.0),
    "Enemy.Chaser": dict(forward_speed=2400.0, forward_speed_by_tier=[0.0, 2500.0, 2950.0, 3250.0], home_leash_range=30000.0, leash_quiet_seconds=25.0,
                         quiet_counts_dealt=True, circle=True, afterburn_multiplier=1.8, afterburn_seconds=4.5,
                         afterburn_cooldown=4.0, afterburn_trigger_factor=1.0),
    "Enemy.Ranged": dict(retreat_fraction=0.4, charged_shot=True, charge_first_delay_min=2.0, charge_first_delay_max=3.0,
                         charge_interval_min=6.0, charge_interval_max=8.0, charge_telegraph_seconds=1.2,
                         charged_shot_damage=45.0, charged_shot_speed=12500.0, charged_shot_radius=150.0,
                         charged_shot_lifetime=2.0),
}

# TTK table only (FEnemyBalance): measured fire uptime (share of a fight the lasers fire; Startech.Combat.Report's
# Uptime column), the uptime in the type's second case (chaser: the player backs off), and the type's own one-fight
# loss targets (second case: every charged shot dodged / the player backs off). regular_only: held only where the type
# is the Regular role. alt_info_only: the second case's target is shown for information and never fails the test.
TABLE = {
    # Uptimes measured in the Phase 3 sessions (TASK_CombatBalance2_Phase3.md): drifter 61% (tier I); chaser 58-78%
    # engaged (face/circle, tiers II-IV), 15-17% against a player backing off in tier II; ranged 70-85%.
    # Chaser with its 45 m gun and per-tier speed (Phase 3 review 2): 38-72% engaged, 10-32% backing off met in aggro range (0.22 average).
    # Its back-off value is information only (alt_info_only): tiers III-IV were accepted under it, and back-off fights
    # vary 3-5 points between sessions (TASK_CombatBalance2_Phase3_Final.md).
    "Enemy.Drifter": dict(fire_uptime=0.61, loss_target=(0.05, 0.15), regular_only=True),
    "Enemy.Chaser": dict(fire_uptime=0.65, fire_uptime_alt=0.22, loss_target=(0.20, 0.25), loss_alt_target=(0.10, 0.15),
                         alt_info_only=True),
    "Enemy.Ranged": dict(fire_uptime=0.77, loss_target=(0.25, 0.30), loss_alt_target=(0.12, 0.18)),
}

BEHAVIOUR_PROPERTIES = {
    "forward_speed": "forward_speed",
    "forward_speed_by_tier": "forward_speed_by_tier",
    "home_leash_range": "home_leash_range",
    "leash_quiet_seconds": "leash_quiet_seconds",
    "quiet_counts_dealt": "quiet_counts_dealt_damage",
    "leash_far_range_factor": "leash_far_range_factor",
    "leash_far_seconds": "leash_far_seconds",
    "circle": "circle",
    "afterburn_multiplier": "afterburn_multiplier",
    "afterburn_seconds": "afterburn_seconds",
    "afterburn_cooldown": "afterburn_cooldown",
    "afterburn_trigger_factor": "afterburn_trigger_factor",
    "retreat_fraction": "retreat_fraction",
    "charged_shot": "charged_shot",
    "charge_first_delay_min": "charge_first_delay_min",
    "charge_first_delay_max": "charge_first_delay_max",
    "charge_interval_min": "charge_interval_min",
    "charge_interval_max": "charge_interval_max",
    "charge_telegraph_seconds": "charge_telegraph_seconds",
    "charged_shot_damage": "charged_shot_damage",
    "charged_shot_speed": "charged_shot_speed",
    "charged_shot_radius": "charged_shot_radius",
    "charged_shot_lifetime": "charged_shot_lifetime",
}

# Enemy weapon ranges set by the enemies step, cm (others keep their asset's). The chaser's 45 m gun: Phase 3 review.
WEAPON_RANGE = {
    "DA_BasicWeapon_Chaser": 4500.0,
}

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
        behaviour = unreal.EnemyBehaviourBalance()
        for key, value in BEHAVIOUR.get(enemy_tag, {}).items():
            behaviour.set_editor_property(BEHAVIOUR_PROPERTIES[key], value)
        entry.set_editor_property("behaviour", behaviour)
        table = TABLE.get(enemy_tag, {})
        entry.set_editor_property("fire_uptime", table.get("fire_uptime", 1.0))
        entry.set_editor_property("fire_uptime_alt", table.get("fire_uptime_alt", 1.0))
        low, high = table.get("loss_target", (0.0, 0.0))
        entry.set_editor_property("loss_target_min", low)
        entry.set_editor_property("loss_target_max", high)
        low, high = table.get("loss_alt_target", (0.0, 0.0))
        entry.set_editor_property("loss_alt_target_min", low)
        entry.set_editor_property("loss_alt_target_max", high)
        entry.set_editor_property("loss_targets_regular_only", table.get("regular_only", False))
        entry.set_editor_property("loss_alt_target_info_only", table.get("alt_info_only", False))
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
        report("balance: %s table: uptime %.2f / %.2f, loss %.2f-%.2f, alt %.2f-%.2f" % (
            tag_name(entry.get_editor_property("enemy_tag")), entry.get_editor_property("fire_uptime"),
            entry.get_editor_property("fire_uptime_alt"), entry.get_editor_property("loss_target_min"),
            entry.get_editor_property("loss_target_max"), entry.get_editor_property("loss_alt_target_min"),
            entry.get_editor_property("loss_alt_target_max")))
        behaviour = entry.get_editor_property("behaviour")
        report("balance: %s behaviour %s" % (tag_name(entry.get_editor_property("enemy_tag")), ", ".join(
            "%s=%s" % (key, behaviour.get_editor_property(prop)) for key, prop in BEHAVIOUR_PROPERTIES.items())))
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
    for enemy_tag, suffix, hull, shield, weapon_name, damage, preferred, _, _ in ENEMIES:
        weapon = load(WEAPONS + weapon_name)
        weapon.set_editor_property("damage", damage)
        if weapon_name in WEAPON_RANGE:
            weapon.set_editor_property("range", WEAPON_RANGE[weapon_name])
        save(weapon)
        data = load(ENEMY_DATA + suffix)
        data.set_editor_property("max_hull", hull)
        data.set_editor_property("max_shield", shield)
        data.set_editor_property("laser_weapon", weapon)
        data.set_editor_property("preferred_range", preferred)
        behaviour = BEHAVIOUR.get(enemy_tag, {})
        if behaviour.get("forward_speed", 0.0) > 0.0:
            data.set_editor_property("max_forward_speed", behaviour["forward_speed"])
        if behaviour.get("home_leash_range", 0.0) > 0.0:
            data.set_editor_property("leash_range", behaviour["home_leash_range"])
        save(data)
        report("enemies: %s hull %.0f shield %.0f %s (dmg %.2f, %.1f/s, %.0f m) preferred %.0f m, speed %.0f, leash %.0f m" % (
            suffix, hull, shield, weapon_name, weapon.get_editor_property("damage"),
            weapon.get_editor_property("fire_rate"), weapon.get_editor_property("range") / 100.0, preferred / 100.0,
            data.get_editor_property("max_forward_speed"), data.get_editor_property("leash_range") / 100.0))


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
