# QuestFramework: data-driven quests for any game

QuestFramework is a reusable runtime plugin for quests: authored quests with tagged objectives, prerequisites, time limits, category limits and turn-in, and generated bounties from templates. It knows nothing about the game that uses it. The game reports what happens as tagged events, and supplies its inventory, its rewards and its turn-in rule through a small bridge.

Target engine: Unreal Engine 5.8. Module `QuestFramework`, dependencies Core, CoreUObject, Engine, GameplayTags, DeveloperSettings (and Projects, privately, only to register the test tags).

## Contents

1. Concepts
2. How a game integrates
3. Authoring quests
4. Authoring bounty templates
5. Settings
6. Console commands, the overlay and the test content
7. Saving
8. Known limits
9. API reference

---

## 1. Concepts

### Quests, categories and statuses

A quest is a `UQuestDefinition` data asset: an id, a title and description, a **category**, one or more **objectives**, optional **prerequisites**, an optional **time limit**, **rewards**, and whether it must be **turned in**.

Categories are Gameplay Tags. The plugin declares three natively: `Quest.Category.Story`, `Quest.Category.Bounty` and `Quest.Category.Timed`. Each category can have a limit on how many of its quests are active at once (defaults: Story 1, Bounty 3, Timed 1). Each limit also has a plural **display name** used in messages: accepting past the limit is refused with "Limit reached: {Max} {DisplayName}", for example "Limit reached: 3 bounties". Without a display name, the last segment of the category tag is used.

Each quest has an `EQuestStatus`:

```
Locked ──(prerequisites completed)──> Available ──Accept──> Active ──(all objectives done)──> ReadyToTurnIn ──TurnIn──> Completed
                                          ^                   │  ^                                │                       │
                                          │                   │  └──(items lost)──────────────────┘                       │
                                          ├──Abandon──────────┘                                                           │
                                          ├──(time ran out: Failed, then at once if not repeatable)                        │
                                          └──(repeatable: after the cooldown, from Completed or Failed)─────────────────────┘
```

- **Locked**: at least one prerequisite quest has never been completed.
- **Available**: can be accepted.
- **Active**: accepted; its objectives count events.
- **ReadyToTurnIn**: every objective is done. A quest with `bRequiresTurnIn = false` skips this and completes immediately.
- **Completed**: turned in. A repeatable quest stays here during its cooldown, then becomes Available.
- **Failed**: its time ran out. A repeatable quest stays here during its cooldown; a non-repeatable one returns to Available at once (listeners still see the Failed event).

### Asking before acting

`CanAccept(QuestId, OutReason)` and `CanTurnIn(QuestId, OutReason)` answer whether `Accept` or `TurnIn` would succeed now, with the refusal reason, without changing anything. Use them to disable a button and show the reason before the player clicks. `Accept` and `TurnIn` call them first, so the query and the action never disagree.

- `CanAccept` refuses when the quest is unknown, locked, already active or ready, completed and not repeatable, cooling down, or its category is full.
- `CanTurnIn` refuses when the quest is unknown or not ready, the turn-in gate refuses, its inventory objectives would no longer be complete (read from the inventory without updating progress), or items must be taken and no inventory provider is set. When `TurnIn` is refused because the inventory no longer completes the quest, the quest goes back to Active.

### Events

The game never calls into objectives. It reports what happened with `UQuestSubsystem::ReportEvent(FQuestEvent)`:

| Field | Meaning |
|---|---|
| `EventTag` | What happened: `Quest.Event.TargetDestroyed`, `Quest.Event.ItemCollected`, `Quest.Event.LocationReached`, or a game's own tag. |
| `SubjectTags` | What it happened to, for example `Enemy.Bandit` or `Item.IronOre`. |
| `ContextId` | Where it happened (a zone, region or level id). None when unknown. |
| `Amount` | How many times. Default 1. |

### Objectives (Strategy pattern)

Each objective kind is a `UQuestObjective` subclass that decides whether an event counts:

| Kind | Counts | Notes |
|---|---|---|
| **Destroy** (`UQuestObjective_Destroy`) | `TargetDestroyed` events whose subject has any of `TargetTags` | Parent tags match children: `Enemy` matches `Enemy.Bandit`. Optional `RequiredContextId`. |
| **Collect** (`UQuestObjective_Collect`) | The inventory count of `ItemTag` (capped) | Re-read on accept, on every event (including `ItemCollected`), and on `RefreshInventoryObjectives`, so selling or losing items lowers it. `bConsumeOnTurnIn` takes the items on turn-in. |
| **Visit** (`UQuestObjective_Visit`) | `LocationReached` events with its `ContextId` | Usually `RequiredCount = 1`. |

With `bObjectivesInOrder`, only the first incomplete objective counts events; later inventory objectives read 0 until it is their turn.

Objective descriptions may use `{Current}` and `{Required}`. An empty description shows "{Current} / {Required}".

### Time

A timed quest's clock starts on accept and runs on the world's delta time: it pauses while the game is paused and follows time dilation. It is stored as time remaining, not as a timestamp. `OnQuestTimeChanged` fires on accept and then once per whole second, not every frame. At 0 the quest fails. Cooldowns count down the same way. The subsystem only ticks while a clock or cooldown is running.

### Bounties

A `UBountyTemplate` describes a family of bounties: target options (subject tags, count range, reward per target, weight) and location options (context id, reward multiplier, weight), an optional time-limit range and extra rewards. `FBountyFactory::Generate(Template, Seed)` builds a quest from it, the same quest for the same template and seed every time, with id `Bounty_<TemplateId>_<Seed>`. `UBountyBoardSubsystem` keeps a few generated offers registered as ordinary quests; the game decides when to call `Refresh`.

---

## 2. How a game integrates

The whole game side is: implement two small interfaces, optionally bind a turn-in rule, report events, listen to events.

```cpp
// 1. Bridge: an object that implements IQuestInventoryProvider and IQuestRewardGranter.
UQuestSubsystem* const Quests = GetGameInstance()->GetSubsystem<UQuestSubsystem>();
Quests->SetInventoryProvider(MyInventoryBridge);   // IQuestInventoryProvider*, held weakly
Quests->SetRewardGranter(MyRewardBridge);           // IQuestRewardGranter*, held weakly
Quests->SetTurnInGate(FQuestTurnInGate::CreateWeakLambda(this, [this](FName QuestId, FText& OutReason)
{
	if (IsAtQuestGiver()) { return true; }
	OutReason = NSLOCTEXT("Game", "TurnInAway", "Return to a quest giver to turn this in.");
	return false;
}));

// 2. Report what happens.
FQuestEvent Event;
Event.EventTag = QuestTags::Event_TargetDestroyed;
Event.SubjectTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Enemy.Bandit")));
Event.ContextId = CurrentZoneId;
Quests->ReportEvent(Event);

// 3. Listen.
Quests->OnQuestStatusChanged.AddDynamic(this, &UMyQuestLog::HandleStatusChanged);
```

### The bridge

| Piece | What the game provides |
|---|---|
| `IQuestInventoryProvider` | `GetItemCount(ItemTag)` and `RemoveItems(ItemTag, Count)`. The game maps item tags to its own items. Without a provider, Collect objectives read 0 and quests that consume items cannot be turned in. |
| `IQuestRewardGranter` | `GrantReward(FQuestReward)`: the game decides what each reward tag means (credits, an item, reputation). Without a granter, rewards are only logged. |
| `FQuestTurnInGate` | `bool (FName QuestId, FText& OutReason)`: whether turning in is allowed right now (for example, only at a quest giver). Unbound allows every turn-in. It is not consulted when a quest completes by itself. |

Bridge objects are held weakly: keep them alive yourself. Bind the gate with `CreateUObject`, `CreateWeakLambda` or `BindUObject` so it does not keep anything alive either.

### Events to report

- `Quest.Event.TargetDestroyed` with the destroyed thing's tags as subject.
- `Quest.Event.ItemCollected` with the item tag as subject when the player picks something up. Call `RefreshInventoryObjectives()` when items are lost, sold or used, since nothing is collected then.
- `Quest.Event.LocationReached` with the place's id as context.
- Your own event tags for your own objective kinds (see "Adding an objective kind" below).

### Events to listen to

| Event | When |
|---|---|
| `OnQuestStatusChanged(QuestId, NewStatus)` | Every status change. |
| `OnObjectiveProgress(QuestId, ObjectiveIndex, Current, Required)` | Every objective count change, and once per objective on accept. |
| `OnQuestTimeChanged(QuestId, SecondsRemaining)` | On accept and once per whole second of a timed quest. |
| `OnQuestUnregistered(QuestId)` | A quest left the registry (a generated bounty turned in, abandoned or dropped from the board). |
| `UBountyBoardSubsystem::OnBoardRefreshed` | After a board refresh. |

Every event is broadcast after the call that caused it has finished changing state, so a listener may call back into the subsystem (for example, accept the next quest of a chain) without breaking anything.

### Adding an objective kind

Subclass `UQuestObjective` (in the game module is fine), override `EvaluateEvent` (and `IsObjectiveValid`). For inventory-like progress override `TracksInventory` and `QueryCurrent`; for side effects on turn-in, `NeedsInventoryOnTurnIn` and `ApplyTurnIn`. It appears in the objective kind dropdown of every quest.

---

## 3. Authoring quests

1. Content Browser > Add > Miscellaneous > Data Asset > **QuestDefinition**. Put it under a folder listed in the settings' Quest Definition Paths (default `/Game/Quests`).
2. Set **Quest Id** (unique), **Title**, **Description**, **Category**.
3. Add **Objectives**: each entry has a kind dropdown (Destroy, Collect, Visit) and its fields.
4. Rules: **Objectives In Order**, **Time Limit Seconds** (0 = none), **Prerequisite Quest Ids**, **Repeatable** and **Repeat Cooldown Seconds**, **Requires Turn In**.
5. **Rewards**: a tag and an amount each. The tags are your game's (for example `Reward.Credits`).
6. Right-click > Asset Actions > Validate: `IsDataValid` checks the id, that each objective is set, used once and valid (RequiredCount above 0, tags or ids filled in), and that the quest is not its own prerequisite.

Quests are found by folder at game-instance start. A packaged build must cook those folders: add them to Project Settings > Packaging > Additional Asset Directories to Cook (see Known limits).

---

## 4. Authoring bounty templates

Data Asset > **BountyTemplate**:

| Field | Meaning |
|---|---|
| Template Id | Unique; part of every generated id and stored in saves. |
| Category | Default `Quest.Category.Bounty`, so the Bounty limit applies. |
| Title Format | `{Count}`, `{Target}`, `{Location}` are filled in. Default "Destroy {Count} {Target} in {Location}". |
| Description Format | Same arguments; may be empty. |
| Objective Format | `{Target}` and `{Location}` are filled in at generation, `{Current}` and `{Required}` as it progresses. |
| Targets | Subject tags, display name, weight, min and max count, reward per target. |
| Locations | Context id (None = anywhere), display name, weight, reward multiplier. This is the hook for regions of different danger. |
| Min/Max Time Limit Seconds | Max 0 means untimed. Otherwise a whole number of seconds in the range. |
| Credit Reward Tag | The tag the per-target reward is given as, for example `Reward.Credits`. |
| Extra Rewards | Added to every bounty. |
| Requires Turn In | Whether generated bounties have to be turned in. |

Reward = count × reward per target × location multiplier, rounded.

Then add the template to Project Settings > Plugins > Quest Framework > Bounty Templates. Saved bounties are regenerated from the template with the same id, so do not rename template ids once saves exist. Changing a template's options changes the bounties its seeds produce.

---

## 5. Settings

Project Settings > Plugins > Quest Framework (`UQuestFrameworkSettings`, saved in `DefaultGame.ini`):

| Setting | Default | Meaning |
|---|---|---|
| Quest Definition Paths | `/Game/Quests` | Folders searched (with subfolders) for quest definitions. |
| Category Limits | Story 1, Bounty 3, Timed 1 | Most active quests per category. A category without an entry has no limit. Each entry's Display Name is the plural used in "Limit reached: {Max} {DisplayName}" (empty: the tag's last segment). |
| Bounty Board Size | 3 | Offers after a refresh. |
| Bounty Templates | empty | The templates the board draws from and saved bounties are regenerated from. |

**Why folders and not the Asset Manager:** the plugin finds quests with an Asset Registry query over the configured folders. That works with no project-wide Asset Manager setup (no Primary Asset Type entry to add to every project that uses the plugin), and quest folders are small. The cost is that a packaged build must cook those folders explicitly. `UQuestDefinition` still reports the primary asset type `QuestDefinition`, so a project that prefers the Asset Manager for cooking and chunking can add that type without code changes.

---

## 6. Console commands, the overlay and the test content

Non-shipping builds. Open the console in Play mode with the tilde key.

| Command | Effect |
|---|---|
| `Quest.List` | Log every quest with its status, objective progress, time left and cooldown. |
| `Quest.Accept <Id>` | Accept a quest (the reason is logged if refused). |
| `Quest.Abandon <Id>` | Abandon an active quest. |
| `Quest.TurnIn <Id>` | Turn in a quest (the reason is logged if refused). |
| `Quest.Event <EventTag> <SubjectTag\|None> [ContextId] [Amount]` | Report an event by hand. |
| `Quest.Bounty.Refresh` | Refresh the bounty board. |
| `Quest.Bounty.List` | Log the current offers with their rewards. |
| `Quest.Debug.UseTestBridge` | Register a stand-in inventory and reward granter (rewards are logged). For testing without a game bridge. |
| `Quest.Debug.Give <ItemTag> <Count>` | Change the stand-in inventory; a gain is reported as an `ItemCollected` event, a loss (negative) refreshes inventory objectives. |
| `Quest.Debug 1` / `0` | Show or hide the overlay of active quests with objective progress and time left. |

### Test content

`Tools/create_test_content.py` builds the test assets in the plugin's own content folder, `/QuestFramework/Test`. Run it with the editor closed:

```
UnrealEditor-Cmd.exe "<Project>.uproject" -run=pythonscript -script="<Project>/Plugins/QuestFramework/Tools/create_test_content.py" -unattended -nopause -nosplash -stdout
```

It creates three quests and a bounty template, using the test tags in `Plugins/QuestFramework/Config/Tags/QuestFrameworkTest.ini` (registered by the module in non-shipping builds):

| Asset | Quest |
|---|---|
| `DA_TestQuest_Destroy` | `Test_Destroy`, Story: destroy 3 `QuestTest.Enemy.Drone` (children count). Turn in for 500 `QuestTest.Reward.Credits`. |
| `DA_TestQuest_Collect` | `Test_Collect`, Story, needs `Test_Destroy`: hold 5 `QuestTest.Item.Ore`, consumed on turn-in. |
| `DA_TestQuest_Timed` | `Test_Timed`, Timed, 60 s, repeatable (30 s cooldown), completes by itself. In order: reach `TestZone`, then destroy 2 drones in `TestZone`. |
| `DA_TestBountyTemplate` | Template `TestBounty`: 2–4 drones (100 each, weight 3) or 1–2 heavy drones (300 each); any zone or `TestZone` (×1.5). |

Instanced objectives can be created from Python: `unreal.new_object(unreal.QuestObjective_Destroy, outer=definition)` and assigning the list to `objectives` saves them inside the asset. Gameplay tags are built with `GameplayTag.import_text('(TagName="...")')`.

To use the test content, the settings must list `/QuestFramework/Test` and the template:

```ini
[/Script/QuestFramework.QuestFrameworkSettings]
+QuestDefinitionPaths=(Path="/Game/Quests")
+QuestDefinitionPaths=(Path="/QuestFramework/Test")
+BountyTemplates=/QuestFramework/Test/DA_TestBountyTemplate.DA_TestBountyTemplate
```

### Test plan

```
Quest.Debug.UseTestBridge
Quest.Debug 1
Quest.List                                         Test_Collect Locked, the others Available
Quest.Accept Test_Collect                          refused: prerequisites
Quest.Accept Test_Destroy
Quest.TurnIn Test_Destroy                          refused: not ready
Quest.Event Quest.Event.TargetDestroyed QuestTest.Enemy.Drone.Heavy None 2     2/3 (child tag counts)
Quest.Event Quest.Event.TargetDestroyed QuestTest.Item.Ore None 5              no change
Quest.Event Quest.Event.TargetDestroyed QuestTest.Enemy.Drone                  3/3, ReadyToTurnIn
Quest.TurnIn Test_Destroy                          500 credits logged; Test_Collect becomes Available
Quest.Accept Test_Collect
Quest.Debug.Give QuestTest.Item.Ore 5              5/5, ReadyToTurnIn
Quest.Debug.Give QuestTest.Item.Ore -1             4/5, back to Active
Quest.Debug.Give QuestTest.Item.Ore 2              5/5
Quest.TurnIn Test_Collect                          5 ore removed, rewards logged
Quest.Accept Test_Timed                            clock starts at 1:00 on the overlay
Quest.Event Quest.Event.TargetDestroyed QuestTest.Enemy.Drone TestZone 2       no change: visit comes first
Quest.Event Quest.Event.LocationReached None TestZone
Quest.Event Quest.Event.TargetDestroyed QuestTest.Enemy.Drone TestZone 2       completes by itself, reward logged
Quest.Accept Test_Timed                            refused: cooldown
(after 30 s) Quest.Accept Test_Timed, wait 60 s    Failed, then cooldown; pausing the game stops the clock
Quest.Bounty.Refresh / Quest.Bounty.List           3 offers; Accept one, Refresh again: the accepted one stays
Accept 4 bounties                                  the fourth is refused: Bounty limit 3
```

---

## 7. Saving

No save system is included; the plugin is save-ready:

- `UQuestSubsystem::GetAllStates()` returns every quest's `FQuestRuntimeState` (all fields `SaveGame`, plain values).
- `UBountyBoardSubsystem::GetNextSeed()` is the board's only state.

On load, call `UBountyBoardSubsystem::SetNextSeed` and `UQuestSubsystem::RestoreStates(States)`. Restore removes this session's generated bounties, regenerates saved ones from `SourceTemplateId` and `GenerationSeed`, starts quests missing from the save fresh, skips saved ids that no longer exist (with a warning), clamps progress to the current objectives, and broadcasts `OnQuestStatusChanged` for every quest.

---

## 8. Known limits

- Packaged builds: quest folders must be cooked (Additional Asset Directories to Cook), or registered with the Asset Manager as primary asset type `QuestDefinition`.
- The test tags (`QuestTest.*`) are registered in non-shipping builds only, and the test content uses them.
- The board's offers are "generated quests that are Available". A non-repeatable timed bounty that fails returns to Available, so it shows as an offer again until the next refresh.
- Category limits match the category tag exactly, not its children.
- Turning in consumes items through the provider without a transaction: if `RemoveItems` fails after the counts were checked, the quest still completes and a warning is logged.

---

## 9. API reference

### `QuestTypes.h`

**`EQuestStatus`** (enum class): Where a quest stands for the player.

- `Locked`: At least one prerequisite quest has not been completed yet.
- `Available`: Can be accepted.
- `Active`: Accepted; its objectives count events.
- `ReadyToTurnIn`: Every objective is done; waiting for the player to turn it in.
- `Completed`: Turned in (or completed automatically). A repeatable quest stays here during its cooldown.
- `Failed`: Its time ran out. A repeatable quest stays here during its cooldown; others return to Available at once.

**`FQuestEvent`** (struct): Something that happened in the game, reported to UQuestSubsystem::ReportEvent. The plugin never sees game classes: objectives decide from the tags alone whether an event counts.

- `EventTag` (`FGameplayTag`): What happened: Quest.Event.TargetDestroyed, Quest.Event.ItemCollected, Quest.Event.LocationReached, or a game's own.
- `SubjectTags` (`FGameplayTagContainer`): What it happened to: what was destroyed or collected, for example Enemy.Bandit.
- `ContextId` (`FName`): Where it happened, for example a zone or region id. None when unknown.
- `Amount` (`int32`): How many times it happened.

**`FQuestReward`** (struct): One reward. The game decides what the tag means when it grants it (credits, an item, reputation).

- `RewardTag` (`FGameplayTag`): Game-defined kind of reward, for example Reward.Credits or Reward.Item.IronOre.
- `Amount` (`int32`): How much of it.

**`FQuestRuntimeState`** (struct): Everything that changes about one quest while playing. Plain values only, so an array of these is the whole quest save: see UQuestSubsystem::GetAllStates and RestoreStates.

- `QuestId` (`FName`): The quest this state belongs to.
- `Status` (`EQuestStatus`): Where the quest stands.
- `ObjectiveProgress` (`TArray<int32>`): Current count of each objective, in the definition's order, capped at its RequiredCount.
- `TimeRemainingSeconds` (`float`): Seconds left on an active timed quest. Stored as time left rather than a timestamp, so it saves cleanly.
- `TimesCompleted` (`int32`): How many times the quest has been completed. Prerequisites are met once this is above zero.
- `CooldownRemainingSeconds` (`float`): Seconds before a repeatable quest can be accepted again.
- `SourceTemplateId` (`FName`): For a generated bounty: the template it came from, so it can be regenerated after loading. None otherwise.
- `GenerationSeed` (`int32`): For a generated bounty: the seed it was generated with.

**`FQuestCategoryLimit`** (struct): The most quests of one category that may be active (or ready to turn in) at once.

- `Category` (`FGameplayTag`): The category, for example Quest.Category.Bounty. Matched exactly.
- `Max` (`int32`): The limit. A category with no entry has none.
- `DisplayName` (`FText`): Plural name of the category's quests in messages, for example "bounties" in "Limit reached: 3 bounties". When empty, the last segment of the category tag is used.

**`FBountyTargetOption`** (struct): One kind of target a bounty template can ask for.

- `SubjectTags` (`FGameplayTagContainer`): What has to be destroyed; an event matches if its subject has any of these (parents match children).
- `DisplayName` (`FText`): Name used for {Target} in the title, for example "bandits".
- `Weight` (`float`): Relative chance of being picked.
- `MinCount` (`int32`): Fewest targets asked for.
- `MaxCount` (`int32`): Most targets asked for.
- `RewardPerUnit` (`int32`): Reward per target, before the location's multiplier.

**`FBountyLocationOption`** (struct): One place a bounty template can send the player.

- `ContextId` (`FName`): The context id the kills must happen in. None means anywhere.
- `DisplayName` (`FText`): Name used for {Location} in the title, for example "the northern marsh" or "anywhere".
- `Weight` (`float`): Relative chance of being picked.
- `RewardMultiplier` (`float`): Multiplies the target reward, so dangerous places pay more.

### `QuestTags.h`

Namespace **`QuestTags`**: The native Gameplay Tags the plugin declares: quest categories and the built-in event kinds. Games may add more.

- `Category_Story` (`Quest.Category.Story`): Category: story quests. Default limit of one active at a time.
- `Category_Bounty` (`Quest.Category.Bounty`): Category: bounties, usually generated by the bounty board. Default limit of three active.
- `Category_Timed` (`Quest.Category.Timed`): Category: timed quests. Default limit of one active.
- `Event_TargetDestroyed` (`Quest.Event.TargetDestroyed`): Event: something was destroyed. Its subject tags say what. Counted by Destroy objectives.
- `Event_ItemCollected` (`Quest.Event.ItemCollected`): Event: an item was collected. Collect objectives re-read the inventory on it.
- `Event_LocationReached` (`Quest.Event.LocationReached`): Event: a place was reached. Its context id says which. Completes Visit objectives.

### `QuestBridge.h`

- `DECLARE_DELEGATE_RetVal_TwoParams(bool, FQuestTurnInGate, FName, FText&)`: Asked by the quest subsystem whether a quest may be turned in right now (for example, only at a quest giver). Return false and fill OutReason to refuse. Bind with BindUObject or BindWeakLambda so it holds no strong reference.

**`IQuestInventoryProvider`** (interface, native only): The game's inventory as quests see it. Implement on any UObject and register it with UQuestSubsystem::SetInventoryProvider.

- `int32 GetItemCount(const FGameplayTag& ItemTag) const`: How many of the item the player holds now.
- `bool RemoveItems(const FGameplayTag& ItemTag, int32 Count)`: Takes Count of the item away, for objectives that consume items on turn-in. Returns false if it could not.

**`IQuestRewardGranter`** (interface, native only): Gives quest rewards to the player. Implement on any UObject and register it with UQuestSubsystem::SetRewardGranter.

- `void GrantReward(const FQuestReward& Reward)`: Gives the player one reward. The game decides what its tag means.

### `QuestFrameworkLog.h`

- `LogQuestFramework`: Log category used by every QuestFramework class. Filter the Output Log by "LogQuestFramework".

### `QuestFrameworkSettings.h`

**`UQuestFrameworkSettings`** (class): Quest Framework settings (Project Settings > Plugins > Quest Framework), saved in DefaultGame.ini.

Variables:

- `QuestDefinitionPaths` (`TArray<FDirectoryPath>`): Folders searched (with subfolders) for UQuestDefinition assets when the game starts.
- `CategoryLimits` (`TArray<FQuestCategoryLimit>`): The most quests of a category active at once. A category with no entry has no limit.
- `BountyBoardSize` (`int32`): How many offers the bounty board holds after a refresh.
- `BountyTemplates` (`TArray<TSoftObjectPtr<UBountyTemplate>>`): The templates the bounty board draws from, and that saved bounties are regenerated from.

Functions:

- `UQuestFrameworkSettings()`: Places the settings under Plugins and sets the default category limits (Story 1, Bounty 3, Timed 1).
- `static const UQuestFrameworkSettings& Get()`: The settings' class default object.
- `TArray<const UBountyTemplate*> LoadBountyTemplates() const`: Loads (if needed) and returns every valid template in BountyTemplates.
- `const UBountyTemplate* FindBountyTemplate(FName TemplateId) const`: The template in BountyTemplates with this TemplateId, loaded, or null.
- `int32 GetCategoryLimit(const FGameplayTag& Category) const`: The limit for Category, or INDEX_NONE when it has none.
- `const FQuestCategoryLimit* FindCategoryLimit(const FGameplayTag& Category) const`: The limit entry for Category, or null when it has none.

### `Definitions/QuestDefinition.h`

**`UQuestDefinition`** (class, `UPrimaryDataAsset`): One quest, authored as a data asset (or generated at runtime by FBountyFactory). Read-only while playing: what changes is kept in FQuestRuntimeState by UQuestSubsystem.

Variables:

- `PrimaryAssetType` (`static const FPrimaryAssetType`): Primary asset type of every quest definition, "QuestDefinition".
- `QuestId` (`FName`): Unique id of the quest, used by the API, prerequisites and saves.
- `Title` (`FText`): Name shown to the player.
- `Description` (`FText`): Longer text shown to the player.
- `Category` (`FGameplayTag`): The quest's kind, for example Quest.Category.Story. Category limits apply per category.
- `Objectives` (`TArray<TObjectPtr<UQuestObjective>>`, instanced): What the quest asks for. Pick a kind for each entry (Destroy, Collect, Visit).
- `bObjectivesInOrder` (`bool`): When true, only the first incomplete objective counts events, so the objectives are done one after another.
- `TimeLimitSeconds` (`float`): Seconds to finish once accepted; 0 means no limit. The quest fails when it runs out.
- `PrerequisiteQuestIds` (`TArray<FName>`): Quests that must have been completed before this one can be accepted.
- `bRepeatable` (`bool`): Can be accepted again after completing (or failing), once RepeatCooldownSeconds have passed.
- `RepeatCooldownSeconds` (`float`): Seconds after completing or failing a repeatable quest before it can be accepted again.
- `bRequiresTurnIn` (`bool`): When true the player turns the quest in (UQuestSubsystem::TurnIn); when false it completes once every objective is done.
- `Rewards` (`TArray<FQuestReward>`): Given through the game's reward granter on completion.
- `SourceTemplateId` (`FName`): For a generated bounty: the template it came from. None for authored quests.
- `GenerationSeed` (`int32`): For a generated bounty: the seed it was generated with.

Functions:

- `bool IsGenerated() const`: True if the quest was generated from a bounty template.
- `bool HasTimeLimit() const`: True if the quest has a time limit.
- `virtual FPrimaryAssetId GetPrimaryAssetId() const override`: Type "QuestDefinition", named after the asset, so the Asset Manager can find quests too.
- `virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override` (editor): Checks the quest id is set, there is at least one objective, every objective is set, used once and valid for its kind (RequiredCount above 0 among others), and the quest is not its own prerequisite.

### `Objectives/QuestObjective.h`

**`UQuestObjective`** (abstract class, `EditInlineNew`, `DefaultToInstanced`): One thing a quest asks for (Strategy pattern: one subclass per kind of objective). Objectives are instanced inside their UQuestDefinition and hold no runtime state: progress lives in FQuestRuntimeState, and an objective only says how much an event adds, or reads its progress from the inventory.

Variables:

- `Description` (`FText`): Text shown for the objective. {Current} and {Required} are replaced by the progress. Empty shows "{Current} / {Required}".
- `RequiredCount` (`int32`): How many are needed to complete it.

Functions:

- `virtual int32 EvaluateEvent(const FQuestEvent& Event) const`: How much Event adds to this objective's progress; 0 if it does not count. Event-counted objectives override it.
- `virtual bool TracksInventory() const`: True if progress is read from the inventory (QueryCurrent) instead of counted from events.
- `virtual int32 QueryCurrent(const IQuestInventoryProvider& Inventory) const`: For an inventory objective: its progress now, read from the game's inventory and capped at RequiredCount.
- `virtual bool NeedsInventoryOnTurnIn() const`: True if turning the quest in has to take something from the inventory (see ApplyTurnIn).
- `virtual bool ApplyTurnIn(IQuestInventoryProvider& Inventory) const`: Called once as the quest is turned in, after every check passed: takes consumed items. Returns false on failure.
- `virtual FText GetProgressText(int32 Current) const`: The description with {Current} and {Required} filled in.
- `virtual bool IsObjectiveValid(FText& OutError) const`: False with a message when the objective is set up wrong. The base checks RequiredCount. Used by IsDataValid.

### `Objectives/QuestObjective_Destroy.h`

**`UQuestObjective_Destroy`** (class): Destroy RequiredCount things: counts Quest.Event.TargetDestroyed events whose subject matches TargetTags.

- `TargetTags` (`FGameplayTagContainer`): An event counts if its subject has any of these. A parent tag matches its children (Enemy matches Enemy.Bandit).
- `RequiredContextId` (`FName`): If set, only events with this context id count (for example, kills in one region). None counts them anywhere.
- `virtual int32 EvaluateEvent(const FQuestEvent& Event) const override`: The event's Amount for a matching TargetDestroyed event, else 0.
- `virtual bool IsObjectiveValid(FText& OutError) const override`: Also needs at least one target tag.

### `Objectives/QuestObjective_Collect.h`

**`UQuestObjective_Collect`** (class): Hold RequiredCount of an item. Progress is the current inventory count (capped), re-read on accept, on Quest.Event.ItemCollected events and on UQuestSubsystem::RefreshInventoryObjectives, so selling or losing items lowers it again.

- `ItemTag` (`FGameplayTag`): The item to hold, as the game's inventory provider names it.
- `bConsumeOnTurnIn` (`bool`): If true, turning the quest in takes RequiredCount of the item from the inventory.
- `virtual bool TracksInventory() const override`: Always true: progress comes from the inventory.
- `virtual int32 QueryCurrent(const IQuestInventoryProvider& Inventory) const override`: The inventory's count of ItemTag, capped at RequiredCount.
- `virtual bool NeedsInventoryOnTurnIn() const override`: bConsumeOnTurnIn.
- `virtual bool ApplyTurnIn(IQuestInventoryProvider& Inventory) const override`: Removes RequiredCount of the item when bConsumeOnTurnIn is set.
- `virtual bool IsObjectiveValid(FText& OutError) const override`: Also needs a valid item tag.

### `Objectives/QuestObjective_Visit.h`

**`UQuestObjective_Visit`** (class): Go somewhere: counts Quest.Event.LocationReached events with ContextId. RequiredCount is normally 1.

- `ContextId` (`FName`): The place to reach, as the game reports it in the event's ContextId.
- `virtual int32 EvaluateEvent(const FQuestEvent& Event) const override`: The event's Amount for a LocationReached event with this context id, else 0.
- `virtual bool IsObjectiveValid(FText& OutError) const override`: Also needs a context id.

### `Runtime/QuestSubsystem.h`

- `DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnQuestStatusChanged, FName, QuestId, EQuestStatus, NewStatus)`: Broadcast when a quest's status changes.
- `DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnQuestObjectiveProgress, FName, QuestId, int32, ObjectiveIndex, int32, Current, int32, Required)`: Broadcast when an objective's count changes.
- `DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnQuestTimeChanged, FName, QuestId, float, SecondsRemaining)`: Broadcast once per whole second while a timed quest's clock runs, and when it is accepted.
- `DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnQuestUnregistered, FName, QuestId)`: Broadcast when a quest leaves the registry (a generated bounty turned in, abandoned or dropped from the board).

**`UQuestSubsystem`** (class, `UGameInstanceSubsystem`, `FTickableGameObject`): The quest runtime (Facade): the registry of quest definitions, every quest's FQuestRuntimeState, and the rules that move quests between statuses. The game reports tagged events with ReportEvent and supplies inventory, rewards and the turn-in rule through the bridge setters; it listens to the On... events to update its UI. Every event is broadcast after the call that caused it has finished changing state, so listeners may call back in.

Events:

- `OnQuestStatusChanged` (`FOnQuestStatusChanged`): Fires when a quest's status changes.
- `OnObjectiveProgress` (`FOnQuestObjectiveProgress`): Fires when an objective's count changes.
- `OnQuestTimeChanged` (`FOnQuestTimeChanged`): Fires once per whole second of a timed quest's clock, and on accept.
- `OnQuestUnregistered` (`FOnQuestUnregistered`): Fires when a quest leaves the registry.

Functions:

- `virtual void Initialize(FSubsystemCollectionBase& Collection) override`: Loads every UQuestDefinition under the settings' QuestDefinitionPaths and sets each quest Locked or Available.
- `virtual void Deinitialize() override`: Drops every definition, state and bridge reference.
- `bool RegisterDefinition(UQuestDefinition* Definition)`: Adds a definition at runtime (for example a generated bounty). Fails if it has no id or the id is taken.
- `bool UnregisterDefinition(FName QuestId)`: Removes a definition and its state, and broadcasts OnQuestUnregistered. False if the id is unknown.
- `void SetInventoryProvider(IQuestInventoryProvider* Provider)`: The game's inventory, held weakly. Null clears it. Collect objectives read 0 without one.
- `void SetRewardGranter(IQuestRewardGranter* Granter)`: The game's reward granter, held weakly. Null clears it. Without one, rewards are only logged.
- `void SetTurnInGate(const FQuestTurnInGate& Gate)`: The turn-in rule. Bind it with BindUObject or BindWeakLambda so it is weak. Unbound allows every turn-in.
- `bool CanAccept(FName QuestId, FText& OutReason) const`: Whether Accept would succeed now, without changing anything. False with a reason when the quest is unknown, locked, already active or completed, cooling down, or its category is full ("Limit reached: 3 bounties"). Accept runs the same checks, so a UI that shows this reason never disagrees with the action.
- `bool CanTurnIn(FName QuestId, FText& OutReason) const`: Whether TurnIn would succeed now, without changing anything. False with a reason when the quest is unknown or not ready, the turn-in gate refuses, its inventory objectives are no longer complete (read from the inventory), or items must be taken and no inventory is registered. TurnIn runs the same checks.
- `bool Accept(FName QuestId, FText& OutReason)`: Starts a quest. Fails with the reason from CanAccept when it can't be accepted.
- `bool Abandon(FName QuestId)`: Gives up an active quest: it becomes Available again (a generated bounty is removed). False if it was not active.
- `bool TurnIn(FName QuestId, FText& OutReason)`: Hands in a quest that is ready: checks CanTurnIn, takes consumed items, grants the rewards, marks it Completed (or starts its repeat cooldown) and unlocks the quests that depended on it. When refused because the inventory no longer completes it, the quest goes back to Active.
- `void ReportEvent(const FQuestEvent& Event)`: Reports something that happened. Every active quest's counting objectives evaluate it.
- `void RefreshInventoryObjectives()`: Re-reads every inventory objective of the active quests. Call it when items are lost, sold or used.
- `bool GetState(FName QuestId, FQuestRuntimeState& OutState) const`: Copies a quest's state into OutState. False if the quest is unknown.
- `const FQuestRuntimeState* FindState(FName QuestId) const`: A quest's state, or null if it is unknown. Valid until the registry changes.
- `TArray<FName> GetQuestsByStatus(EQuestStatus Status) const`: Ids of every quest with the status, in registry order.
- `UQuestDefinition* GetDefinition(FName QuestId) const`: The quest's definition, or null if it is unknown.
- `const TArray<TObjectPtr<UQuestDefinition>>& GetAllDefinitions() const`: Every registered definition, in registry order.
- `int32 CountActiveInCategory(FGameplayTag Category) const`: How many quests of the category are Active or ReadyToTurnIn.
- `TArray<FQuestRuntimeState> GetAllStates() const`: Every quest's state: the whole quest save. Store it with the game's own save system.
- `void RestoreStates(const TArray<FQuestRuntimeState>& SavedStates)`: Replaces every state with a saved one. Generated bounties are removed first, then the saved ones are regenerated from their template and seed. Quests missing from the save start fresh; saved ids that no longer exist are skipped with a warning. Broadcasts OnQuestStatusChanged for every quest afterwards.
- `virtual void Tick(float DeltaTime) override`: Counts down time limits and cooldowns by the world's delta time; not called while the game is paused.
- `virtual TStatId GetStatId() const override`: Stat id for the tick.
- `virtual ETickableTickType GetTickableTickType() const override`: Never for the class default object, otherwise conditional on IsTickable.
- `virtual bool IsTickable() const override`: True once initialized while any quest has a running clock or cooldown.
- `virtual UWorld* GetTickableGameObjectWorld() const override`: The game instance's world, so the tick follows that world's pause and time dilation.

### `Bounties/BountyTemplate.h`

**`UBountyTemplate`** (class, `UPrimaryDataAsset`): The recipe for generated bounties: which targets, how many, where, for how long and for what reward. FBountyFactory turns a template and a seed into a quest definition with one Destroy objective.

Variables:

- `TemplateId` (`FName`): Unique id; part of every generated quest id (Bounty_<TemplateId>_<Seed>) and stored in saves.
- `Category` (`FGameplayTag`): Category of the generated quests.
- `TitleFormat` (`FText`): Title of a generated quest. {Count}, {Target} and {Location} are filled in.
- `DescriptionFormat` (`FText`): Description of a generated quest, with the same arguments as the title. May be empty.
- `ObjectiveFormat` (`FText`): The objective's text. {Target} and {Location} are filled in when generated; {Current} and {Required} as it progresses.
- `Targets` (`TArray<FBountyTargetOption>`): The kinds of target a bounty may ask for; one is picked by weight.
- `Locations` (`TArray<FBountyLocationOption>`): The places a bounty may be set in; one is picked by weight. Empty means anywhere.
- `MinTimeLimitSeconds` (`float`): Shortest time limit, in whole seconds. With MaxTimeLimitSeconds at 0 the bounties are untimed.
- `MaxTimeLimitSeconds` (`float`): Longest time limit; 0 means no time limit.
- `CreditRewardTag` (`FGameplayTag`): The reward tag the per-target reward is given as, for example Reward.Credits.
- `ExtraRewards` (`TArray<FQuestReward>`): Fixed rewards added to every generated bounty.
- `bRequiresTurnIn` (`bool`): Whether the generated bounties have to be turned in (true) or complete when the last target is destroyed.

Functions:

- `UBountyTemplate()`: Sets the category to Quest.Category.Bounty.
- `virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override` (editor): Checks the template id, at least one target with subject tags and MinCount no greater than MaxCount, a positive total weight for targets (and locations, if any), the time range, and a credit tag when targets pay a reward.

### `Bounties/BountyFactory.h`

**`FBountyFactory`** (class): Builds bounty quests from templates (Factory pattern). Deterministic: the same template and seed always give the same quest, which is how saved bounties are regenerated after loading.

- `static UQuestDefinition* Generate(const UBountyTemplate& Template, int32 Seed, UObject* Outer)`: Generates a transient quest definition with one Destroy objective and a reward computed from the template: count x RewardPerUnit x the location's multiplier, given as CreditRewardTag, plus ExtraRewards. The quest id is Bounty_<TemplateId>_<Seed>. Returns null if the template has no targets.
- `static FName MakeQuestId(FName TemplateId, int32 Seed)`: The id Generate gives the quest for this template and seed.

### `Bounties/BountyBoardSubsystem.h`

- `DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBountyBoardRefreshed)`: Broadcast after the bounty board has been refreshed.

**`UBountyBoardSubsystem`** (class, `UGameInstanceSubsystem`): The bounty board: a few generated bounty offers the player can accept. Offers are registered with UQuestSubsystem as ordinary quests, so Accept, events and turn-in work on them unchanged. The board's only own state is the seed counter, so the offers themselves are saved with the quest states and regenerated from template and seed. The game decides when to refresh (for example, when the player visits the board).

- `OnBoardRefreshed` (`FOnBountyBoardRefreshed`): Fires after Refresh.
- `virtual void Initialize(FSubsystemCollectionBase& Collection) override`: Depends on UQuestSubsystem, and starts the seed counter at a random value so each new game differs.
- `void Refresh()`: Removes every offer that has not been accepted and generates new ones up to the settings' BountyBoardSize, each from a template picked with its seed, then broadcasts OnBoardRefreshed. Accepted bounties are kept.
- `TArray<UQuestDefinition*> GetOffers() const`: The current offers: generated quests that are registered and Available, oldest first.
- `int32 GetNextSeed() const`: The seed the next generated offer will use. Save it with the quest states.
- `void SetNextSeed(int32 Seed)`: Restores the seed counter from a save, so offers after loading continue the saved sequence.

### `Debug/QuestDebugSubsystem.h`

**`UQuestDebugSubsystem`** (class, `UTickableWorldSubsystem`): Draws an on-screen overlay of the active quests with their objective progress and time left. Only reads from UQuestSubsystem. Toggle with the console variable "Quest.Debug 1" (0 hides it). Non-shipping builds only.

- `virtual void Tick(float DeltaTime) override`: Prints the overlay lines for this frame when the console variable is on.
- `virtual TStatId GetStatId() const override`: Stat id required by the tickable interface.
- `virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override`: Game and Play-In-Editor worlds of non-shipping builds only.

### `Debug/QuestDebugBridge.h`

**`FQuestDebugItem`** (struct): One item count held by the debug bridge.

- `ItemTag` (`FGameplayTag`): The item.
- `Count` (`int32`): How many are held.

**`UQuestDebugBridge`** (class): A stand-in game bridge for testing the plugin with console commands alone: a tiny inventory (Quest.Debug.Give) and a reward granter that logs what it would give. Registered with Quest.Debug.UseTestBridge; never used otherwise.

- `virtual int32 GetItemCount(const FGameplayTag& ItemTag) const override`: The held count of the item.
- `virtual bool RemoveItems(const FGameplayTag& ItemTag, int32 Count) override`: Takes Count of the item if that many are held.
- `virtual void GrantReward(const FQuestReward& Reward) override`: Logs the reward.
- `int32 AddItems(const FGameplayTag& ItemTag, int32 Delta)`: Adds Delta of the item (negative removes, not below 0) and returns the new count.
