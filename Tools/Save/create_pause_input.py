"""Creates the pause input for the save task's pause menu (the "editor step", done by script).

Run headless with the editor closed:
  UnrealEditor-Cmd.exe "<Project>.uproject" -run=pythonscript
      -script="<Project>/Tools/Save/create_pause_input.py" -unattended -nopause -nosplash -stdout

Does, idempotently:
  /Game/Inputs/Actions/IA_Pause   a Boolean input action (a copy of IA_Interact with its triggers and modifiers
                                  cleared), created if missing
  /Game/Inputs/IMC_Ship           maps Escape to IA_Pause (an existing Escape mapping of IA_Pause is replaced)

The ship binds IA_Pause through UStartechSettings::PauseInputAction (DefaultGame.ini), not through BP_Ship.
Results are reported with unreal.log_warning, because plain unreal.log does not reach stdout.
"""

import unreal

ACTIONS = "/Game/Inputs/Actions"
PAUSE = ACTIONS + "/IA_Pause"
TEMPLATE = ACTIONS + "/IA_Interact"
CONTEXT = "/Game/Inputs/IMC_Ship"


def report(message):
    unreal.log_warning("[PauseInput] " + message)


def load(path):
    asset = unreal.EditorAssetLibrary.load_asset(path)
    if asset is None:
        raise RuntimeError("Asset not found: " + path)
    return asset


def escape_key():
    key = unreal.Key()
    key.import_text("Escape")
    if "Escape" not in str(key.export_text()):
        raise RuntimeError("Could not make the Escape key")
    return key


def make_pause_action():
    if unreal.EditorAssetLibrary.does_asset_exist(PAUSE):
        report("IA_Pause exists.")
        return load(PAUSE)
    action = unreal.EditorAssetLibrary.duplicate_asset(TEMPLATE, PAUSE)
    if action is None:
        raise RuntimeError("Could not create " + PAUSE)
    action.set_editor_property("value_type", unreal.InputActionValueType.BOOLEAN)
    action.set_editor_property("triggers", [])
    action.set_editor_property("modifiers", [])
    unreal.EditorAssetLibrary.save_loaded_asset(action, False)
    report("Created IA_Pause.")
    return action


def map_escape(action):
    context = load(CONTEXT)
    key = escape_key()
    context.unmap_key(action, key)
    context.map_key(action, key)
    unreal.EditorAssetLibrary.save_loaded_asset(context, False)
    report("Mapped Escape to IA_Pause in IMC_Ship.")


def verify():
    context = load(CONTEXT)
    action = load(PAUSE)
    # UE 5.8 keeps the mappings in DefaultKeyMappings.Mappings; older versions had a top-level Mappings array.
    try:
        mappings = context.get_editor_property("default_key_mappings").get_editor_property("mappings")
    except Exception:
        mappings = context.get_editor_property("mappings")
    found = False
    for mapping in mappings:
        mapped_action = mapping.get_editor_property("action")
        if mapped_action is not None and mapped_action.get_path_name() == action.get_path_name() \
                and "Escape" in str(mapping.get_editor_property("key").export_text()):
            found = True
    report("IA_Pause value type: {}".format(action.get_editor_property("value_type")))
    report("Escape -> IA_Pause in IMC_Ship: {}".format("yes" if found else "NOT FOUND"))


map_escape(make_pause_action())
verify()
report("Done.")
