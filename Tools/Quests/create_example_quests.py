"""Creates Startech's example quests in /Game/Quests, one of each kind.

Run headless with the editor closed (after apply_quest_tags.py, though this script does not depend on it):
  UnrealEditor-Cmd.exe "<Project>.uproject" -run=pythonscript
      -script="<Project>/Tools/Quests/create_example_quests.py" -unattended -nopause -nosplash -stdout
or from the editor's Python console: exec(open(r"<path>/create_example_quests.py").read())

It builds:
  DA_Quest_FirstContract      Story "First contract". In order: visit Rust, then hold 20 Item.Mineral.Ferrite
                              (consumed on turn-in). 1500 credits + 200 Item.Ammo.LL1. Turn-in.
  DA_Quest_DrifterSweep       Timed "Drifter sweep". Destroy 8 Enemy.Drifter anywhere within 240 s.
                              Repeatable, 300 s cooldown. 1200 credits. Turn-in.
  DA_BountyTemplate_Standard  Bounty template "Standard": drifters 6-12 (80 cr, w3), chasers 5-10 (140 cr, w2),
                              ranged ships 4-8 (200 cr, w1); in Terra (x1.0), in Rust (x1.6) or anywhere (x1.0),
                              each weight 1. No time limit. Turn-in.

The bounty title is "Destroy {Count} {Target} {Location}" with the "in" inside each location's name ("in Rust",
"anywhere"), so an anywhere bounty reads "Destroy 8 drifters anywhere" rather than "... in anywhere".
Config/DefaultGame.ini points QuestFramework at /Game/Quests and this template.
Results are reported with unreal.log_warning, because plain unreal.log does not reach stdout in a commandlet.
"""

import unreal

FOLDER = "/Game/Quests"


def report(message):
    unreal.log_warning("[StartechQuests] " + message)


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


def reward(tag_name, amount):
    value = unreal.QuestReward()
    value.set_editor_property("reward_tag", tag(tag_name))
    value.set_editor_property("amount", amount)
    return value


def make_asset(name, asset_class):
    """Creates (or replaces) a data asset of asset_class in FOLDER."""
    path = "{}/{}".format(FOLDER, name)
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.EditorAssetLibrary.delete_asset(path)
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", asset_class)
    asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, FOLDER, asset_class, factory)
    if asset is None:
        raise RuntimeError("Could not create " + path)
    return asset


def objective(definition, objective_class, description, required, **properties):
    """An instanced objective owned by the definition, so it is saved inside the asset."""
    instance = unreal.new_object(objective_class, outer=definition)
    instance.set_editor_property("description", description)
    instance.set_editor_property("required_count", required)
    for key, value in properties.items():
        instance.set_editor_property(key, value)
    return instance


def make_quest(name, quest_id, title, category, objectives_builder, **properties):
    definition = make_asset(name, unreal.QuestDefinition)
    definition.set_editor_property("quest_id", quest_id)
    definition.set_editor_property("title", title)
    definition.set_editor_property("category", tag(category))
    definition.set_editor_property("objectives", objectives_builder(definition))
    for key, value in properties.items():
        definition.set_editor_property(key, value)
    unreal.EditorAssetLibrary.save_loaded_asset(definition, False)
    return definition


def target(tag_name, display_name, weight, min_count, max_count, reward_per_unit):
    value = unreal.BountyTargetOption()
    value.set_editor_property("subject_tags", tags(tag_name))
    value.set_editor_property("display_name", display_name)
    value.set_editor_property("weight", weight)
    value.set_editor_property("min_count", min_count)
    value.set_editor_property("max_count", max_count)
    value.set_editor_property("reward_per_unit", reward_per_unit)
    return value


def location(context_id, display_name, weight, multiplier):
    value = unreal.BountyLocationOption()
    if context_id:
        value.set_editor_property("context_id", context_id)
    value.set_editor_property("display_name", display_name)
    value.set_editor_property("weight", weight)
    value.set_editor_property("reward_multiplier", multiplier)
    return value


def build():
    make_quest(
        "DA_Quest_FirstContract", "FirstContract", "First contract", "Quest.Category.Story",
        lambda d: [
            objective(d, unreal.QuestObjective_Visit, "Visit Rust", 1, context_id="Rust"),
            objective(d, unreal.QuestObjective_Collect, "Ferrite held: {Current} / {Required}", 20,
                      item_tag=tag("Item.Mineral.Ferrite"), consume_on_turn_in=True),
        ],
        description="Jump to Rust, gather 20 Ferrite and bring it back to the station.",
        objectives_in_order=True, requires_turn_in=True,
        rewards=[reward("Reward.Credits", 1500), reward("Item.Ammo.LL1", 200)])

    make_quest(
        "DA_Quest_DrifterSweep", "DrifterSweep", "Drifter sweep", "Quest.Category.Timed",
        lambda d: [objective(d, unreal.QuestObjective_Destroy, "Drifters destroyed: {Current} / {Required}", 8,
                             target_tags=tags("Enemy.Drifter"))],
        description="Destroy 8 drifters anywhere within four minutes, then dock to collect.",
        time_limit_seconds=240.0, repeatable=True, repeat_cooldown_seconds=300.0, requires_turn_in=True,
        rewards=[reward("Reward.Credits", 1200)])

    template = make_asset("DA_BountyTemplate_Standard", unreal.BountyTemplate)
    template.set_editor_property("template_id", "Standard")
    template.set_editor_property("category", tag("Quest.Category.Bounty"))
    template.set_editor_property("title_format", "Destroy {Count} {Target} {Location}")
    template.set_editor_property("description_format",
                                 "Destroy {Count} {Target} {Location}, then dock at the station to collect.")
    template.set_editor_property("objective_format", "{Target} destroyed {Location}: {Current} / {Required}")
    template.set_editor_property("credit_reward_tag", tag("Reward.Credits"))
    template.set_editor_property("min_time_limit_seconds", 0.0)
    template.set_editor_property("max_time_limit_seconds", 0.0)
    template.set_editor_property("requires_turn_in", True)
    template.set_editor_property("targets", [
        target("Enemy.Drifter", "drifters", 3.0, 6, 12, 80),
        target("Enemy.Chaser", "chasers", 2.0, 5, 10, 140),
        target("Enemy.Ranged", "ranged ships", 1.0, 4, 8, 200),
    ])
    template.set_editor_property("locations", [
        location("Terra", "in Terra", 1.0, 1.0),
        location("Rust", "in Rust", 1.0, 1.6),
        location(None, "anywhere", 1.0, 1.0),
    ])
    unreal.EditorAssetLibrary.save_loaded_asset(template, False)


def verify():
    """Reloads every asset and reports its contents, to prove the instanced objectives were saved."""
    for name in ("DA_Quest_FirstContract", "DA_Quest_DrifterSweep"):
        asset = unreal.EditorAssetLibrary.load_asset("{}/{}".format(FOLDER, name))
        objectives = asset.get_editor_property("objectives")
        kinds = ", ".join("{} x{}".format(o.get_class().get_name(), o.get_editor_property("required_count"))
                          for o in objectives if o is not None)
        rewards = ", ".join("{} x{}".format(r.get_editor_property("reward_tag").export_text(),
                                            r.get_editor_property("amount"))
                            for r in asset.get_editor_property("rewards"))
        report("{} ({}, {}): {} objective(s): {}; outer of first: {}; time {} s; repeat {} / {} s; rewards {}".format(
            name, asset.get_editor_property("quest_id"), asset.get_editor_property("category").export_text(),
            len(objectives), kinds, objectives[0].get_outer().get_name() if objectives else "none",
            asset.get_editor_property("time_limit_seconds"), asset.get_editor_property("repeatable"),
            asset.get_editor_property("repeat_cooldown_seconds"), rewards))
    template = unreal.EditorAssetLibrary.load_asset(FOLDER + "/DA_BountyTemplate_Standard")
    targets = ", ".join("{} {}-{} {}cr w{}".format(t.get_editor_property("display_name"),
                                                    t.get_editor_property("min_count"), t.get_editor_property("max_count"),
                                                    t.get_editor_property("reward_per_unit"), t.get_editor_property("weight"))
                        for t in template.get_editor_property("targets"))
    locations = ", ".join("{} [{}] x{} w{}".format(l.get_editor_property("display_name"),
                                                    l.get_editor_property("context_id"),
                                                    l.get_editor_property("reward_multiplier"), l.get_editor_property("weight"))
                          for l in template.get_editor_property("locations"))
    report("DA_BountyTemplate_Standard ({}): targets {}; locations {}".format(
        template.get_editor_property("template_id"), targets, locations))


build()
verify()
report("Done.")
