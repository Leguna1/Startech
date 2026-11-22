"""Creates the QuestFramework test content in /QuestFramework/Test (the plugin's own Content folder).

Run headless with the editor closed:
  UnrealEditor-Cmd.exe "<Project>.uproject" -run=pythonscript
      -script="<Project>/Plugins/QuestFramework/Tools/create_test_content.py" -unattended -nopause -nosplash -stdout
or from the editor's Python console: exec(open(r"<path>/create_test_content.py").read())

It builds:
  DA_TestQuest_Destroy  Story. Destroy 3 QuestTest.Enemy.Drone (children count too). Turn in for 500 credits.
  DA_TestQuest_Collect  Story. Needs Destroy completed. Hold 5 QuestTest.Item.Ore, consumed on turn-in.
  DA_TestQuest_Timed    Timed, 60 s, repeatable (30 s cooldown), no turn-in. In order: visit TestZone, then
                        destroy 2 drones in TestZone.
  DA_TestBountyTemplate Bounty template TestBounty: 2-4 drones or 1-2 heavy drones, anywhere or in TestZone (x1.5).

The test tags come from Plugins/QuestFramework/Config/Tags/QuestFrameworkTest.ini. Results are reported with
unreal.log_warning, because plain unreal.log does not reach stdout in a commandlet.
"""

import unreal

FOLDER = "/QuestFramework/Test"


def report(message):
    unreal.log_warning("[QuestTest] " + message)


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


def build():
    make_quest(
        "DA_TestQuest_Destroy", "Test_Destroy", "Clear the drones", "Quest.Category.Story",
        lambda d: [objective(d, unreal.QuestObjective_Destroy, "Drones destroyed: {Current} / {Required}", 3,
                             target_tags=tags("QuestTest.Enemy.Drone"))],
        description="Destroy three drones, then turn the quest in.",
        rewards=[reward("QuestTest.Reward.Credits", 500)])

    make_quest(
        "DA_TestQuest_Collect", "Test_Collect", "Ore delivery", "Quest.Category.Story",
        lambda d: [objective(d, unreal.QuestObjective_Collect, "Ore held: {Current} / {Required}", 5,
                             item_tag=tag("QuestTest.Item.Ore"), consume_on_turn_in=True)],
        description="Bring five ore. Unlocked by Clear the drones.",
        prerequisite_quest_ids=["Test_Destroy"],
        rewards=[reward("QuestTest.Reward.Credits", 300), reward("QuestTest.Reward.Item.Ore", 1)])

    make_quest(
        "DA_TestQuest_Timed", "Test_Timed", "Patrol the zone", "Quest.Category.Timed",
        lambda d: [
            objective(d, unreal.QuestObjective_Visit, "Reach TestZone", 1, context_id="TestZone"),
            objective(d, unreal.QuestObjective_Destroy, "Drones destroyed in TestZone: {Current} / {Required}", 2,
                      target_tags=tags("QuestTest.Enemy.Drone"), required_context_id="TestZone"),
        ],
        description="Reach TestZone, then destroy two drones there, within a minute.",
        objectives_in_order=True, time_limit_seconds=60.0, repeatable=True, repeat_cooldown_seconds=30.0,
        requires_turn_in=False,
        rewards=[reward("QuestTest.Reward.Credits", 200)])

    template = make_asset("DA_TestBountyTemplate", unreal.BountyTemplate)
    template.set_editor_property("template_id", "TestBounty")
    template.set_editor_property("credit_reward_tag", tag("QuestTest.Reward.Credits"))

    drone = unreal.BountyTargetOption()
    drone.set_editor_property("subject_tags", tags("QuestTest.Enemy.Drone"))
    drone.set_editor_property("display_name", "drones")
    drone.set_editor_property("weight", 3.0)
    drone.set_editor_property("min_count", 2)
    drone.set_editor_property("max_count", 4)
    drone.set_editor_property("reward_per_unit", 100)

    heavy = unreal.BountyTargetOption()
    heavy.set_editor_property("subject_tags", tags("QuestTest.Enemy.Drone.Heavy"))
    heavy.set_editor_property("display_name", "heavy drones")
    heavy.set_editor_property("weight", 1.0)
    heavy.set_editor_property("min_count", 1)
    heavy.set_editor_property("max_count", 2)
    heavy.set_editor_property("reward_per_unit", 300)
    template.set_editor_property("targets", [drone, heavy])

    anywhere = unreal.BountyLocationOption()
    anywhere.set_editor_property("display_name", "any zone")
    anywhere.set_editor_property("weight", 1.0)
    zone = unreal.BountyLocationOption()
    zone.set_editor_property("context_id", "TestZone")
    zone.set_editor_property("display_name", "TestZone")
    zone.set_editor_property("weight", 1.0)
    zone.set_editor_property("reward_multiplier", 1.5)
    template.set_editor_property("locations", [anywhere, zone])
    unreal.EditorAssetLibrary.save_loaded_asset(template, False)


def verify():
    """Reloads every quest and reports its objectives, to prove the instanced objectives were saved."""
    for name in ("DA_TestQuest_Destroy", "DA_TestQuest_Collect", "DA_TestQuest_Timed"):
        asset = unreal.EditorAssetLibrary.load_asset("{}/{}".format(FOLDER, name))
        objectives = asset.get_editor_property("objectives")
        kinds = ", ".join("{} x{}".format(o.get_class().get_name(), o.get_editor_property("required_count"))
                          for o in objectives if o is not None)
        report("{} ({}): {} objective(s): {}; outer of first: {}".format(
            name, asset.get_editor_property("quest_id"), len(objectives), kinds,
            objectives[0].get_outer().get_name() if objectives else "none"))
    template = unreal.EditorAssetLibrary.load_asset(FOLDER + "/DA_TestBountyTemplate")
    report("DA_TestBountyTemplate: {} target(s), {} location(s)".format(
        len(template.get_editor_property("targets")), len(template.get_editor_property("locations"))))


build()
verify()
report("Done.")
