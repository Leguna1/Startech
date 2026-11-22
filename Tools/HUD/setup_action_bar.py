"""Creates the action bar's input and layout (the "editor step" of the action bar task, done by script).

Run headless with the editor closed (after building, so the ActionBarLayout class exists):
  UnrealEditor-Cmd.exe "<Project>.uproject" -run=pythonscript
      -script="<Project>/Tools/HUD/setup_action_bar.py" -unattended -nopause -nosplash -stdout

Does, idempotently:
  /Game/Inputs/Actions/IA_Slot1..IA_Slot6   Boolean input actions (copies of IA_Pause), created if missing
  /Game/Inputs/IMC_Ship                     removes every IA_SelectSlot mapping (the old number keys, reported),
                                            then maps One..Six to IA_Slot1..IA_Slot6
  /Game/UI/HUD/DA_ActionBarLayout           an ActionBarLayout with the default slots:
                                            1-4 LL1-LL4 "Lasers", 5-6 SD1-SD2 "Drainer"
  ammo data assets                          IconTier (bars or chevrons drawn on the bar): LL1-LL4 1-4, SD1-SD2 1-2
The game finds the layout through UStartechSettings::ActionBarLayout (DefaultGame.ini).
Results are reported with unreal.log_warning, because plain unreal.log does not reach stdout.
"""

import unreal

ACTIONS = "/Game/Inputs/Actions"
TEMPLATE = ACTIONS + "/IA_Pause"
OLD_ACTION = ACTIONS + "/IA_SelectSlot"
CONTEXT = "/Game/Inputs/IMC_Ship"
LAYOUT_FOLDER = "/Game/UI/HUD"
LAYOUT_NAME = "DA_ActionBarLayout"
KEYS = ["One", "Two", "Three", "Four", "Five", "Six"]
TIERS = {
    "/Game/Data/AmmoAsset/DA_1xAmmo": 1,
    "/Game/Data/AmmoAsset/DA_2x_Ammo": 2,
    "/Game/Data/AmmoAsset/DA_x3_Ammo": 3,
    "/Game/Data/AmmoAsset/DA_x4_Ammo": 4,
    "/Game/Data/AmmoAsset/DA_1xDrainShield": 1,
    "/Game/Data/AmmoAsset/DA_2xDrainShield": 2,
}
SLOTS = [
    ("Item.Ammo.LL1", "Lasers"),
    ("Item.Ammo.LL2", "Lasers"),
    ("Item.Ammo.LL3", "Lasers"),
    ("Item.Ammo.LL4", "Lasers"),
    ("Item.Ammo.SD1", "Drainer"),
    ("Item.Ammo.SD2", "Drainer"),
]


def report(message):
    unreal.log_warning("[ActionBar] " + message)


def load(path):
    asset = unreal.EditorAssetLibrary.load_asset(path)
    if asset is None:
        raise RuntimeError("Asset not found: " + path)
    return asset


def make_key(name):
    key = unreal.Key()
    key.import_text(name)
    if name not in str(key.export_text()):
        raise RuntimeError("Could not make key " + name)
    return key


def tag(name):
    value = unreal.GameplayTag()
    value.import_text('(TagName="{}")'.format(name))
    if name not in str(value.export_text()):
        raise RuntimeError("Gameplay tag not found: " + name)
    return value


def mappings_of(context):
    try:
        return context.get_editor_property("default_key_mappings").get_editor_property("mappings")
    except Exception:
        return context.get_editor_property("mappings")


def make_slot_actions():
    actions = []
    for index in range(1, len(KEYS) + 1):
        path = "{}/IA_Slot{}".format(ACTIONS, index)
        if not unreal.EditorAssetLibrary.does_asset_exist(path):
            action = unreal.EditorAssetLibrary.duplicate_asset(TEMPLATE, path)
            if action is None:
                raise RuntimeError("Could not create " + path)
            action.set_editor_property("value_type", unreal.InputActionValueType.BOOLEAN)
            action.set_editor_property("triggers", [])
            action.set_editor_property("modifiers", [])
            unreal.EditorAssetLibrary.save_loaded_asset(action, False)
            report("Created IA_Slot{}.".format(index))
        actions.append(load(path))
    return actions


def remap_keys(actions):
    context = load(CONTEXT)

    # The old number keys: every mapping of IA_SelectSlot goes.
    if unreal.EditorAssetLibrary.does_asset_exist(OLD_ACTION):
        old = load(OLD_ACTION)
        removed = []
        for mapping in list(mappings_of(context)):
            action = mapping.get_editor_property("action")
            if action is not None and action.get_path_name() == old.get_path_name():
                key = mapping.get_editor_property("key")
                removed.append(str(key.export_text()))
                context.unmap_key(old, key)
        report("Removed IA_SelectSlot mappings: {}".format(", ".join(removed) if removed else "none (already removed)"))

    for action, key_name in zip(actions, KEYS):
        key = make_key(key_name)
        context.unmap_key(action, key)
        context.map_key(action, key)
    unreal.EditorAssetLibrary.save_loaded_asset(context, False)
    report("Mapped One..Six to IA_Slot1..IA_Slot6.")


def make_layout(actions):
    path = LAYOUT_FOLDER + "/" + LAYOUT_NAME
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        layout = load(path)
    else:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.ActionBarLayout)
        layout = unreal.AssetToolsHelpers.get_asset_tools().create_asset(LAYOUT_NAME, LAYOUT_FOLDER, unreal.ActionBarLayout, factory)
        if layout is None:
            raise RuntimeError("Could not create " + path)
        report("Created " + LAYOUT_NAME + ".")

    slots = []
    for (item, group), action in zip(SLOTS, actions):
        entry = unreal.ActionBarSlot()
        entry.set_editor_property("kind", unreal.ActionSlotKind.AMMO)
        entry.set_editor_property("item_tag", tag(item))
        entry.set_editor_property("group_label", unreal.Text(group))
        entry.set_editor_property("input_action", action)
        slots.append(entry)
    layout.set_editor_property("slots", slots)
    unreal.EditorAssetLibrary.save_loaded_asset(layout, False)


def set_tiers():
    """The drawn icon's tier on each ammo: LL1-LL4 are 1-4 bars, SD1-SD2 are 1-2 chevrons."""
    for path, tier in TIERS.items():
        ammo = load(path)
        ammo.set_editor_property("icon_tier", tier)
        unreal.EditorAssetLibrary.save_loaded_asset(ammo, False)
        report("{}: IconTier {}".format(path.split("/")[-1], ammo.get_editor_property("icon_tier")))


def verify():
    context = load(CONTEXT)
    for mapping in mappings_of(context):
        action = mapping.get_editor_property("action")
        name = action.get_name() if action else "None"
        if name.startswith("IA_Slot") or name == "IA_SelectSlot":
            report("IMC_Ship: {} -> {}".format(mapping.get_editor_property("key").export_text(), name))
    layout = load(LAYOUT_FOLDER + "/" + LAYOUT_NAME)
    for index, entry in enumerate(layout.get_editor_property("slots")):
        action = entry.get_editor_property("input_action")
        report("Layout slot {}: {} {} '{}' {}".format(index + 1, entry.get_editor_property("kind"),
            entry.get_editor_property("item_tag").export_text(), entry.get_editor_property("group_label"),
            action.get_name() if action else "None"))


slot_actions = make_slot_actions()
remap_keys(slot_actions)
make_layout(slot_actions)
set_tiers()
verify()
report("Done.")
