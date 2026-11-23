"""Creates DA_ControlsHelp, everything the Controls screen shows except the keys (TASK_ControlsScreen).

Run headless with the editor closed (after building, so the ControlsHelpData class exists):
  UnrealEditor-Cmd.exe "<Project>.uproject" -run=pythonscript
      -script="<Project>/Tools/Menu/setup_controls_help.py" -unattended -nopause -nosplash -stdout

Does, idempotently (the asset is rewritten from the tables below on every run, so edit them here, not in the asset):
  /Game/UI/Controls/DA_ControlsHelp   a ControlsHelpData with the mapping contexts to read (IMC_Ship), the groups,
                                      one row per input, the short key labels and the "How combat works" tips
Then it builds the list as the screen does and reports every mapped action no row describes.

The keys are never written here: rows name Input Actions, and the screen reads their keys from the mapping contexts
when it opens. Only inputs handled outside Enhanced Input (the shop's 1-5, Shift-click) list fixed keys, and
keyless inputs (soft lock, docking, gates) a dimmed note. The game finds the asset through
UStartechSettings::ControlsHelp (DefaultGame.ini). Results are reported with unreal.log_warning, because plain
unreal.log does not reach stdout.

To describe a new input: add a row to ROWS (group, name, description, its IA_ names), run this script.

Descriptions and tips may name a key through its action: {Interact} is replaced, when the screen opens, by the
current key labels of IA_Interact ("F"; several keys as "W / S"). Use it instead of typing a key that comes from
IMC_Ship. Keys hard-coded in C++ (the hub's Esc, the shop's 1-5) stay plain text.
"""

import unreal

FOLDER = "/Game/UI/Controls"
NAME = "DA_ControlsHelp"
ACTIONS = "/Game/Inputs/Actions/"
CONTEXTS = ["/Game/Inputs/IMC_Ship"]

GROUPS = [
    ("Flight", "Flight"),
    ("Combat", "Combat"),
    ("ActionBar", "Action bar"),
    ("Station", "Station and interface"),
    ("Camera", "Camera"),
]

# (group, name, description, actions, fixed keys, key note, combination, row key labels)
ROWS = [
    ("Flight", "Throttle", "Hold W to fly forward, S to reverse. Let go of both to brake to a stop.",
     ["IA_Throttle"], [], "", False, []),
    ("Flight", "Turn / strafe", "Turn left and right. With a target locked (Tab), strafe around it instead.",
     ["IA_Turn"], [], "", False, []),
    ("Flight", "Climb / dive", "Fly up and down.",
     ["IA_Vertical"], [], "", False, []),
    ("Flight", "Boost", "Hold for a burst of speed. It drains the boost gauge under your speed, which refills when you let go.",
     ["IA_Boost"], [], "", False, []),

    ("Combat", "Soft lock", "The enemy nearest the centre of your view is targeted automatically.",
     [], [], "Auto", False, []),
    ("Combat", "Lock target", "Lock the current target: your ship faces it, A/D strafe around it. Press again to release.",
     ["IA_TargetLock"], [], "", False, []),
    ("Combat", "Fire", "Hold to fire the active weapon group at your target. With no target, it fires straight ahead.",
     ["IA_Fire"], [], "", False, []),
    ("Combat", "Cycle ammo", "Select the previous or next ammo on the action bar.",
     ["IA_CycleAmmo"], [], "", False, []),

    ("ActionBar", "Laser ammo", "Load LL1–LL4 into the lasers and make them the firing group. LL1 is free and unlimited.",
     ["IA_Slot1", "IA_Slot2", "IA_Slot3", "IA_Slot4"], [], "", False, []),
    ("ActionBar", "Drainer ammo", "Load SD1–SD2 into the shield drainer and make it the firing group.",
     ["IA_Slot5", "IA_Slot6"], [], "", False, []),

    ("Station", "Dock", "Fly into a station's docking ring to dock. Your hull repairs while docked.",
     [], [], "Fly in", False, []),
    ("Station", "Station hub", "While docked, open the station: shop, quests, upgrades. {Interact} or Esc closes it. Undock with the hub's Undock button.",
     ["IA_Interact"], [], "", False, []),
    ("Station", "Jump gate", "Fly into a gate to warp to the next sector. Not possible in combat.",
     [], [], "Fly in", False, []),
    ("Station", "Pause menu", "Pause the game.",
     ["IA_Pause"], [], "", False, []),
    # Handled by UStationHubWidget / FShopQuantity::FromKey; Startech.Controls.RealDataCoversEveryInput checks they match.
    ("Station", "Shop quantity", "In the shop: buy or sell ×1, ×10, ×100, ×1,000 or Max.",
     [], ["One", "Two", "Three", "Four", "Five"], "", False, []),
    # Handled by UShopEntryWidget (GetLastClickModifiers).
    ("Station", "Trade max", "In the shop: Shift-click Buy or Sell to trade the most you can.",
     [], ["LeftShift", "LeftMouseButton"], "", True, [("LeftShift", "Shift"), ("LeftMouseButton", "click")]),

    ("Camera", "Switch view", "Switch between the chase camera and the cockpit.",
     ["IA_CameraSwitch"], [], "", False, []),
    ("Camera", "Look", "Look around. While a target is locked, the camera follows the target.",
     ["IA_Look"], [], "", False, []),
]

# Keycap labels shorter than the engine's display names; any other key uses the engine's name.
KEY_LABELS = [
    ("Mouse2D", "Mouse"),
    ("MouseWheelAxis", "Wheel"),
    ("SpaceBar", "Space"),
    ("LeftMouseButton", "Left Mouse"),
    ("Escape", "Esc"),
]

TIPS_TITLE = "How combat works"
TIPS = [
    "Lock a target with Tab and fire: your shots find it without aiming.",
    "Locked on, A/D strafe around the target; W/S push in and pull back to hold your distance.",
    "An enemy glowing red is charging a heavy shot: change how you're moving to dodge it.",
    "Dock at Terra to repair, sell cargo and restock.",
]
OTHER_GROUP = "Other"
UNBOUND = "Unbound"


def report(message):
    unreal.log_warning("[ControlsHelp] " + message)


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


def make_label(key_name, label):
    entry = unreal.ControlsKeyLabel()
    entry.set_editor_property("key", make_key(key_name))
    entry.set_editor_property("label", unreal.Text(label))
    return entry


def get_or_create():
    path = FOLDER + "/" + NAME
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        report(NAME + " exists; rewriting it.")
        return load(path)
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.ControlsHelpData)
    asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(NAME, FOLDER, unreal.ControlsHelpData, factory)
    if asset is None:
        raise RuntimeError("Could not create " + path)
    report("Created " + NAME + ".")
    return asset


def build(asset):
    asset.set_editor_property("mapping_contexts", [load(path) for path in CONTEXTS])

    groups = []
    for group_id, display in GROUPS:
        group = unreal.ControlsHelpGroup()
        group.set_editor_property("group_id", group_id)
        group.set_editor_property("display_name", unreal.Text(display))
        groups.append(group)
    asset.set_editor_property("groups", groups)

    rows = []
    for group_id, display, description, actions, fixed, note, combination, row_labels in ROWS:
        row = unreal.ControlsHelpRow()
        row.set_editor_property("group_id", group_id)
        row.set_editor_property("display_name", unreal.Text(display))
        row.set_editor_property("description", unreal.Text(description))
        row.set_editor_property("actions", [load(ACTIONS + name) for name in actions])
        row.set_editor_property("fixed_keys", [make_key(name) for name in fixed])
        row.set_editor_property("key_note", unreal.Text(note))
        row.set_editor_property("join", unreal.ControlsKeyJoin.COMBINATION if combination else unreal.ControlsKeyJoin.ALTERNATIVES)
        row.set_editor_property("row_key_labels", [make_label(key, label) for key, label in row_labels])
        rows.append(row)
    asset.set_editor_property("rows", rows)

    asset.set_editor_property("key_labels", [make_label(key, label) for key, label in KEY_LABELS])
    asset.set_editor_property("other_group_name", unreal.Text(OTHER_GROUP))
    asset.set_editor_property("unbound_key_text", unreal.Text(UNBOUND))
    asset.set_editor_property("tips_title", unreal.Text(TIPS_TITLE))
    asset.set_editor_property("tips", [unreal.Text(tip) for tip in TIPS])
    unreal.EditorAssetLibrary.save_loaded_asset(asset, False)
    report("Wrote {} groups, {} rows, {} key labels, {} tips.".format(len(GROUPS), len(ROWS), len(KEY_LABELS), len(TIPS)))


def mappings_of(context):
    try:
        return context.get_editor_property("default_key_mappings").get_editor_property("mappings")
    except Exception:
        return context.get_editor_property("mappings")


def verify(asset):
    described = set()
    for row in asset.get_editor_property("rows"):
        for action in row.get_editor_property("actions"):
            described.add(action.get_path_name())
    missing = []
    for context in asset.get_editor_property("mapping_contexts"):
        for mapping in mappings_of(context):
            action = mapping.get_editor_property("action")
            if action is not None and action.get_path_name() not in described and action.get_name() not in missing:
                missing.append(action.get_name())
    if missing:
        report("NOT described (they show under '{}'): {}".format(OTHER_GROUP, ", ".join(missing)))
    else:
        report("Every mapped action has a row.")


asset = get_or_create()
build(asset)
verify(load(FOLDER + "/" + NAME))
