"""Replaces the weapon strip with the action bar in WB_GameHUD (the layout step of the action bar task, by script).

Run headless with the editor closed:
  UnrealEditor-Cmd.exe "<Project>.uproject" -run=pythonscript
      -script="<Project>/Tools/HUD/replace_weapon_strip.py [apply]" -unattended -nopause -nosplash -stdout

Without "apply" it only lists WB_GameHUD's widget tree. With "apply" it calls
UHudLayoutEditLibrary::ApplyActionBarLayout (C++, editor only), which:
  - (first run, 2026-11-21, done: removed the weapon strip and the old WB_ActionbarSlot widgets; their classes are
    gone now, so later runs only place the bar and the readout)
  - adds an Invalidation Box holding UHudActionBarWidget: bottom centre, alignment (0.5, 1), position (0, -28),
    size 376 x 96,
  - moves the speed readout right of the bar: bottom centre, alignment (0, 1), position (204, -42), size 120 x 64,
then compiles and saves WB_GameHUD and lists the tree again. Safe to run twice.
Results are reported with unreal.log_warning, because plain unreal.log does not reach stdout.
"""

import sys

import unreal

HUD = "/Game/Blueprints/Widgets/WB_GameHUD"


def report(message):
    unreal.log_warning("[HudLayout] " + message)


def describe(blueprint, title):
    report("--- " + title)
    for line in unreal.HudLayoutEditLibrary.describe_widget_tree(blueprint):
        report(line)


hud = unreal.EditorAssetLibrary.load_asset(HUD)
if hud is None:
    raise RuntimeError("Asset not found: " + HUD)

describe(hud, "WB_GameHUD now")

if "apply" in sys.argv[1:]:
    result = unreal.HudLayoutEditLibrary.apply_action_bar_layout(hud)
    # Python returns the out parameter alongside the return value: (ok, report) in some engine versions, the report
    # alone in others.
    if isinstance(result, tuple):
        ok, changes = result
    elif isinstance(result, (list, unreal.Array)):
        ok, changes = True, result
    else:
        ok, changes = bool(result), []
    for line in changes:
        report(str(line))
    if any("ERRORS" in str(line) for line in changes):
        ok = False
    if ok:
        unreal.EditorAssetLibrary.save_loaded_asset(hud, False)
        report("Saved " + HUD)
        describe(hud, "WB_GameHUD after")
    else:
        report("Not saved: the change could not be applied.")

report("Done.")
