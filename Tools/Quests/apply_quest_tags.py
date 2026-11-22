"""Gives Startech's data assets their quest tags (the "editor step" of the quest task, done by script).

Run headless with the editor closed:
  UnrealEditor-Cmd.exe "<Project>.uproject" -run=pythonscript
      -script="<Project>/Tools/Quests/apply_quest_tags.py" -unattended -nopause -nosplash -stdout
or from the editor's Python console: exec(open(r"<path>/apply_quest_tags.py").read())

Sets:
  UEnemyData::EnemyTags    DA_Enemy_Drifter -> Enemy.Drifter, DA_Enemy_Chaser -> Enemy.Chaser,
                           DA_Enemy_Ranged -> Enemy.Ranged
  UTradeableData::ItemTag  DA_Item_Ferrite/Cobaltine/Aurium -> Item.Mineral.<Name>,
                           DA_1xAmmo/DA_2x_Ammo/DA_x3_Ammo/DA_x4_Ammo -> Item.Ammo.LL1..LL4,
                           DA_1xDrainShield/DA_2xDrainShield -> Item.Ammo.SD1/SD2,
                           DA_BasicWeapon -> Item.Weapon.Laser, DA_ShieldDrainer -> Item.Weapon.ShieldDrainer
                           (weapons are saved by tag; every tagged good is listed in UStartechSettings::ItemCatalog)

The tags are native (Source/Startech/Private/StartechGameplayTags.cpp), so they exist whenever the Startech module
is loaded. Results are reported with unreal.log_warning, because plain unreal.log does not reach stdout.
"""

import unreal

ENEMY_TAGS = {
    "/Game/Data/EnemyData/DA_Enemy_Drifter": "Enemy.Drifter",
    "/Game/Data/EnemyData/DA_Enemy_Chaser": "Enemy.Chaser",
    "/Game/Data/EnemyData/DA_Enemy_Ranged": "Enemy.Ranged",
}

ITEM_TAGS = {
    "/Game/Data/Minerals/DA_Item_Ferrite": "Item.Mineral.Ferrite",
    "/Game/Data/Minerals/DA_Item_Cobaltine": "Item.Mineral.Cobaltine",
    "/Game/Data/Minerals/DA_Item_Aurium": "Item.Mineral.Aurium",
    "/Game/Data/AmmoAsset/DA_1xAmmo": "Item.Ammo.LL1",
    "/Game/Data/AmmoAsset/DA_2x_Ammo": "Item.Ammo.LL2",
    "/Game/Data/AmmoAsset/DA_x3_Ammo": "Item.Ammo.LL3",
    "/Game/Data/AmmoAsset/DA_x4_Ammo": "Item.Ammo.LL4",
    "/Game/Data/AmmoAsset/DA_1xDrainShield": "Item.Ammo.SD1",
    "/Game/Data/AmmoAsset/DA_2xDrainShield": "Item.Ammo.SD2",
    "/Game/Data/WeaponAsset/DA_BasicWeapon": "Item.Weapon.Laser",
    "/Game/Data/WeaponAsset/DA_ShieldDrainer": "Item.Weapon.ShieldDrainer",
}


def report(message):
    unreal.log_warning("[QuestTags] " + message)


def tag(name):
    """A GameplayTag from its name, through text import (the tag must exist)."""
    value = unreal.GameplayTag()
    value.import_text('(TagName="{}")'.format(name))
    if str(value.export_text()).find(name) < 0:
        raise RuntimeError("Gameplay tag not found: " + name)
    return value


def tags(*names):
    """A GameplayTagContainer holding the named tags."""
    container = unreal.GameplayTagContainer()
    inner = ",".join('(TagName="{}")'.format(name) for name in names)
    container.import_text("(GameplayTags=({}))".format(inner))
    if any(str(container.export_text()).find(name) < 0 for name in names):
        raise RuntimeError("Gameplay tag not found in: " + ", ".join(names))
    return container


def load(path):
    asset = unreal.EditorAssetLibrary.load_asset(path)
    if asset is None:
        raise RuntimeError("Asset not found: " + path)
    return asset


def apply():
    for path, name in ENEMY_TAGS.items():
        asset = load(path)
        asset.set_editor_property("enemy_tags", tags(name))
        unreal.EditorAssetLibrary.save_loaded_asset(asset, False)
    for path, name in ITEM_TAGS.items():
        asset = load(path)
        asset.set_editor_property("item_tag", tag(name))
        unreal.EditorAssetLibrary.save_loaded_asset(asset, False)


def verify():
    """Reloads every asset and reports its tag."""
    for path in ENEMY_TAGS:
        report("{}: EnemyTags {}".format(path, load(path).get_editor_property("enemy_tags").export_text()))
    for path in ITEM_TAGS:
        report("{}: ItemTag {}".format(path, load(path).get_editor_property("item_tag").export_text()))


apply()
verify()
report("Done.")
