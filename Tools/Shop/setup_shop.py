"""Shop content for the shop polish task, done by script, not by hand.

Run headless with the editor closed:
  UnrealEditor-Cmd.exe "<Project>.uproject" -run=pythonscript
      -script="<Project>/Tools/Shop/setup_shop.py [widgets] [data]" -unattended -nopause -nosplash -stdout

Steps (default: both), idempotent:
  widgets  /Game/Blueprints/Widgets/WB_Shop_Screen: TabWidgetClass = UShopTabWidget, EntryWidgetClass =
           UShopEntryWidget, MountRowWidgetClass = UMountRowWidget (native classes, built in C++ on UHubButtonWidget).
           The frame (panel, shop name, TabContainer, EntryContainer) is unchanged; ColumnHeaders is collapsed at
           runtime and the quantity chips are inserted under TabContainer at runtime. The old hand-laid WB_Shop_Tab,
           WB_Shop_Entry and WB_MountRow are no longer used; they are left in place.
  data     DA_Shop_Weapon loses DA_BasicWeapon_Chaser (an enemy weapon). Every good the shops trade is re-saved, so its
           BaseValue (now a float) is stored as one.
Results are reported with unreal.log_warning and "[Shop]", because plain unreal.log does not reach stdout.
"""

import sys

import unreal

SCREEN = "/Game/Blueprints/Widgets/WB_Shop_Screen"
SHOPS_DIR = "/Game/Data/Shops"
WEAPON_SHOP = SHOPS_DIR + "/DA_Shop_Weapon"
REMOVED_FROM_WEAPON_SHOP = ["DA_BasicWeapon_Chaser"]

CLASSES = {
    "tab_widget_class": unreal.ShopTabWidget,
    "entry_widget_class": unreal.ShopEntryWidget,
    "mount_row_widget_class": unreal.MountRowWidget,
}


def report(message):
    unreal.log_warning("[Shop] " + message)


def load(path):
    asset = unreal.EditorAssetLibrary.load_asset(path)
    if asset is None:
        raise RuntimeError("Asset not found: " + path)
    return asset


def save(asset):
    if not unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError("could not save " + asset.get_path_name())


def setup_widgets():
    blueprint = load(SCREEN)
    cdo = unreal.get_default_object(blueprint.generated_class())
    cdo.modify()
    for name, cls in CLASSES.items():
        cdo.set_editor_property(name, cls.static_class())

    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    save(blueprint)

    cdo = unreal.get_default_object(blueprint.generated_class())
    for name in CLASSES:
        report("widgets: %s %s = %s" % (SCREEN, name, cdo.get_editor_property(name).get_path_name()))


def setup_data():
    weapon_shop = load(WEAPON_SHOP)
    kept = [entry for entry in weapon_shop.get_editor_property("entries")
            if not entry.get_editor_property("good") or
            entry.get_editor_property("good").get_name() not in REMOVED_FROM_WEAPON_SHOP]
    weapon_shop.set_editor_property("entries", kept)
    save(weapon_shop)

    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    goods = {}
    for data in registry.get_assets_by_path(SHOPS_DIR, recursive=True):
        shop = data.get_asset()
        if not isinstance(shop, unreal.ShopData):
            continue
        names = []
        for entry in shop.get_editor_property("entries"):
            good = entry.get_editor_property("good")
            if good:
                goods[good.get_path_name()] = good
                names.append("%s (%s)" % (good.get_name(), good.get_editor_property("base_value")))
        report("data: %s: %s" % (shop.get_name(), ", ".join(names)))

    for good in goods.values():
        good.modify()
        save(good)
    report("data: re-saved %d good(s)" % len(goods))


def main():
    wanted = [arg.lower() for arg in sys.argv[1:]] or ["widgets", "data"]
    if "widgets" in wanted:
        setup_widgets()
    if "data" in wanted:
        setup_data()
    report("done: " + " ".join(wanted))


main()
