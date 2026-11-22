"""Creates the boost input for the upgrades and boost task (done by script, not by hand).

Run headless with the editor closed:
  UnrealEditor-Cmd.exe "<Project>.uproject" -run=pythonscript
      -script="<Project>/Tools/Ship/create_boost_input.py" -unattended -nopause -nosplash -stdout

Does, idempotently:
  /Game/Inputs/Actions/IA_Boost   a Boolean input action (a copy of IA_Interact with its triggers and modifiers
                                  cleared), created if missing
  /Game/Inputs/IMC_Ship           maps Left Shift to IA_Boost (an existing Left Shift mapping of IA_Boost is replaced)

The ship binds IA_Boost through UStartechSettings::BoostInputAction (DefaultGame.ini), not through BP_Ship.
It is held, not tapped: the ship binds Started (key down) and Completed (key up), so no Hold trigger is needed
(a Hold trigger would delay the boost by its threshold).
Results are reported with unreal.log_warning, because plain unreal.log does not reach stdout.
"""

import unreal

ACTIONS = "/Game/Inputs/Actions"
BOOST = ACTIONS + "/IA_Boost"
TEMPLATE = ACTIONS + "/IA_Interact"
CONTEXT = "/Game/Inputs/IMC_Ship"


def report(message):
    unreal.log_warning("[BoostInput] " + message)


def load(path):
    asset = unreal.EditorAssetLibrary.load_asset(path)
    if asset is None:
        raise RuntimeError("Asset not found: " + path)
    return asset


def shift_key():
    key = unreal.Key()
    key.import_text("LeftShift")
    if "LeftShift" not in str(key.export_text()):
        raise RuntimeError("Could not make the Left Shift key")
    return key


def make_boost_action():
    if unreal.EditorAssetLibrary.does_asset_exist(BOOST):
        report("IA_Boost exists.")
        return load(BOOST)
    action = unreal.EditorAssetLibrary.duplicate_asset(TEMPLATE, BOOST)
    if action is None:
        raise RuntimeError("Could not create " + BOOST)
    action.set_editor_property("value_type", unreal.InputActionValueType.BOOLEAN)
    action.set_editor_property("triggers", [])
    action.set_editor_property("modifiers", [])
    unreal.EditorAssetLibrary.save_loaded_asset(action, False)
    report("Created IA_Boost.")
    return action


def map_shift(action):
    context = load(CONTEXT)
    key = shift_key()
    context.unmap_key(action, key)
    context.map_key(action, key)
    unreal.EditorAssetLibrary.save_loaded_asset(context, False)
    report("Mapped Left Shift to IA_Boost in IMC_Ship.")


def verify():
    context = load(CONTEXT)
    action = load(BOOST)
    # UE 5.8 keeps the mappings in DefaultKeyMappings.Mappings; older versions had a top-level Mappings array.
    try:
        mappings = context.get_editor_property("default_key_mappings").get_editor_property("mappings")
    except Exception:
        mappings = context.get_editor_property("mappings")
    found = False
    for mapping in mappings:
        mapped_action = mapping.get_editor_property("action")
        if mapped_action is not None and mapped_action.get_path_name() == action.get_path_name() \
                and "LeftShift" in str(mapping.get_editor_property("key").export_text()):
            found = True
    report("IA_Boost value type: {}".format(action.get_editor_property("value_type")))
    report("Left Shift -> IA_Boost in IMC_Ship: {}".format("yes" if found else "NOT FOUND"))


map_shift(make_boost_action())
verify()
report("Done.")
