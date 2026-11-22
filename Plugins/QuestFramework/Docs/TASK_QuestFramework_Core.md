# Task: QuestFramework plugin (core)

Create a new, reusable runtime plugin, **QuestFramework**, in `Startech/Plugins/QuestFramework`. It must contain **nothing specific to Startech** (no ships, sectors, ProcSector or space terms), so it can be reused in other projects. Startech connects to it in a separate task (`Docs/Tasks/TASK_Quests_Startech.md`).

Follow the code conventions in `Plugins/ProcSector/CLAUDE.md` (they're the project owner's standing rules):
- no `TMap`; small `TArray`s with linear search
- const correctness throughout
- all logic in C++
- single responsibility, factories for creation
- events instead of polling
- a `/** */` doc comment on every class, member and function

Module `QuestFramework`, export macro `QUESTFRAMEWORK_API`, log category `LogQuestFramework`. Dependencies: Core, CoreUObject, Engine, GameplayTags, DeveloperSettings.

## Concepts

- **Quest kinds are categories, given as Gameplay Tags:** `Quest.Category.Story`, `Quest.Category.Bounty`, `Quest.Category.Timed`. The plugin declares these, plus the event tags below, as native tags.
- **A quest** has one or more objectives, optional prerequisites, an optional time limit, rewards, and an optional turn-in requirement.
- **The game reports what happens** as generic `FQuestEvent`s, tagged. Objectives decide whether an event counts. The plugin never references game classes.
- **Things only the game knows** are reached through small interfaces and delegates:
  - item counts and removal
  - granting rewards
  - whether turning in is allowed right now (for example, docked)

## Data (`Public/QuestTypes.h` and asset classes)

- **`EQuestStatus`:** `Locked` (prerequisites missing), `Available`, `Active`, `ReadyToTurnIn`, `Completed`, `Failed`.
- **`FQuestEvent`:**
  - `FGameplayTag EventTag`: `Quest.Event.TargetDestroyed`, `Quest.Event.ItemCollected` or `Quest.Event.LocationReached`; games may add more
  - `FGameplayTagContainer SubjectTags`: what was destroyed or collected, for example `Enemy.Chaser`
  - `FName ContextId`: where it happened, for example a sector or zone id; None when unknown
  - `int32 Amount = 1`
- **`FQuestReward`:** `FGameplayTag RewardTag` (game-defined, for example `Reward.Credits`, `Reward.Item.IronOre`) and `int32 Amount`.
- **`UQuestDefinition` (`UPrimaryDataAsset`):**
  - `FName QuestId` (unique)
  - `FText Title`, `FText Description`
  - `FGameplayTag Category`
  - `TArray<TObjectPtr<UQuestObjective>> Objectives` (`Instanced`, `EditInlineNew`)
  - `bool bObjectivesInOrder`: when true, only the first incomplete objective counts events
  - `float TimeLimitSeconds = 0`: 0 means no limit
  - `TArray<FName> PrerequisiteQuestIds`: all must be `Completed`
  - `bool bRepeatable`, `float RepeatCooldownSeconds`
  - `bool bRequiresTurnIn = true`: false completes automatically once all objectives are done
  - `TArray<FQuestReward> Rewards`
  - `IsDataValid`: checks unique objective setup, `RequiredCount > 0`, no prerequisite on itself
- **`UQuestObjective` (abstract, `EditInlineNew`, `DefaultToInstanced`)**, Strategy pattern, one subclass per kind:
  - Shared fields: `FText Description` (supports `{Current}` and `{Required}`) and `int32 RequiredCount`.
  - `virtual int32 EvaluateEvent(const FQuestEvent& Event) const`: how much this event adds; 0 if it doesn't count.
  - `virtual bool TracksInventory() const`, and `virtual int32 QueryCurrent(const IQuestInventoryProvider&) const` for inventory-based objectives.
  - `virtual FText GetProgressText(int32 Current) const`.
  - **`UQuestObjective_Destroy`:** `FGameplayTagContainer TargetTags` (matches if the event's subject has any of them; parent tags match children) and an optional `FName RequiredContextId`. Counts `TargetDestroyed` events.
  - **`UQuestObjective_Collect`:** `FGameplayTag ItemTag` and `bool bConsumeOnTurnIn`. Progress is the current inventory count (capped at required), re-queried on `ItemCollected` events and on accept, so selling or losing items lowers it.
  - **`UQuestObjective_Visit`:** `FName ContextId`. Completes on a `LocationReached` event with that id.
- **`FQuestRuntimeState` (`USTRUCT`, all fields `SaveGame`-ready):**
  - `QuestId`, `Status`
  - `TArray<int32> ObjectiveProgress`
  - `float TimeRemainingSeconds` (stored as remaining time, not a timestamp, so it saves cleanly)
  - `int32 TimesCompleted`, `float CooldownRemainingSeconds`
  - for generated bounties: `FName SourceTemplateId` and `int32 GenerationSeed`, so they can be regenerated after loading

## Game bridge (`Public/QuestBridge.h`)

- **`IQuestInventoryProvider`:** `GetItemCount(FGameplayTag)` and `RemoveItems(FGameplayTag, int32)`.
- **`IQuestRewardGranter`:** `GrantReward(const FQuestReward&)`.
- **Turn-in gate:** a native single-cast delegate `bool (FName QuestId, FText& OutReason)`. Unbound means always allowed.

The game registers these with `UQuestSubsystem` (`SetInventoryProvider`, `SetRewardGranter`, `SetTurnInGate`), each as a weak reference.

## Runtime (`UQuestSubsystem`, a `UGameInstanceSubsystem`)

**Registry.** Loads every `UQuestDefinition` from the paths in the settings (Asset Manager primary asset type `QuestDefinition`, or the directory list; choose one and justify it), plus definitions registered at runtime (bounties).

**API:**
- `Accept(QuestId)`, which fails with a reason when locked, already active, on cooldown, or over its category limit
- `Abandon(QuestId)`
- `TurnIn(QuestId)`, which checks the gate, removes consumed items, grants rewards, marks the quest `Completed` (or starts its repeat cooldown) and unlocks dependent quests
- `ReportEvent(const FQuestEvent&)`
- `GetState(QuestId)`, `GetQuestsByStatus(EQuestStatus)` and `GetDefinition(QuestId)`

**Category limits:** a settings array of `{Tag, Max}`. Defaults: Story 1, Bounty 3, Timed 1.

**Timers:**
- A timed quest's clock starts on accept and pauses while the game is paused. Implement it with world time deltas, not wall-clock time.
- At 0, an active timed quest becomes `Failed`. A failed repeatable quest goes on its cooldown; a failed non-repeatable one returns to `Available`.

**Events:**
- `OnQuestStatusChanged(QuestId, NewStatus)`
- `OnObjectiveProgress(QuestId, ObjectiveIndex, Current, Required)`
- `OnQuestTimeChanged(QuestId, SecondsRemaining)`, fired once per whole second, not every frame

**Save readiness:**
- `GetAllStates()` and `RestoreStates(const TArray<FQuestRuntimeState>&)`.
- Restoring regenerates bounty definitions from their template and seed.
- No save system is built here.

## Bounties

- **`UBountyTemplate` (`UPrimaryDataAsset`):**
  - `FName TemplateId`, `FGameplayTag Category` (default Bounty)
  - `FText TitleFormat`, for example "Destroy {Count} {Target} in {Location}"
  - `TArray<FBountyTargetOption>`: subject tags, display name, weight, minimum and maximum count, reward per unit
  - `TArray<FBountyLocationOption>`: context id (None means anywhere), display name, weight, reward multiplier. This is the hook for future sector tiers.
  - an optional time-limit range (0 means none), so timed bounties work too
  - extra fixed rewards
  - `IsDataValid`
- **`FBountyFactory`**, Factory pattern: `static UQuestDefinition* Generate(const UBountyTemplate&, int32 Seed, UObject* Outer)`.
  - Deterministic for a template and seed.
  - Builds a transient definition with one Destroy objective and a credit reward from the formula. The reward tag comes from a template property, such as `Reward.Credits`.
  - The quest id is `Bounty_<TemplateId>_<Seed>`.
- **`UBountyBoardSubsystem` (`UGameInstanceSubsystem`):**
  - `Refresh()`: regenerates the unaccepted offers. The board size comes from settings, default 3. Seeds come from a counter stored in the board, so it's save-ready.
  - `GetOffers()` and `OnBoardRefreshed`.
  - The game decides when to refresh (Startech refreshes on docking).
  - Accepted bounties stay registered until turned in or abandoned.

## Settings (`UQuestFrameworkSettings`, Project Settings > Plugins > Quest Framework)

- quest asset paths
- category limits
- bounty board size
- the templates the board draws from

## Debug

**Console commands** (non-shipping builds):

| Command | Effect |
|---|---|
| `Quest.List` | Log every quest with its status, progress and time left |
| `Quest.Accept <Id>` | Accept a quest |
| `Quest.Abandon <Id>` | Abandon a quest |
| `Quest.TurnIn <Id>` | Turn in a quest |
| `Quest.Event <EventTag> <SubjectTag> [ContextId] [Amount]` | Report an event by hand |
| `Quest.Bounty.Refresh` | Refresh the bounty board |
| `Quest.Bounty.List` | Log the current offers |

**Overlay:** `Quest.Debug 1` shows active quests with objective progress and time left. That's the only quest display until the UI task exists.

## Documentation

- `Plugins/QuestFramework/CLAUDE.md` in the style of ProcSector's: architecture, rules, conventions, build command.
- `Plugins/QuestFramework/Docs/DOCUMENTATION.md`: concepts, how a game integrates (bridge, events, tags), authoring quests and bounty templates, the console commands, and an API reference for every public member.

## Acceptance

1. Builds cleanly. The plugin compiles with no reference to any Startech or ProcSector code.
2. Testable with console commands alone, using a test quest definition and bounty template created by an editor script in `Plugins/QuestFramework/Tools/`. Report whether instanced objectives could be created from Python; if not, give manual steps.
3. The summary lists every public type and function, with a 10-line integration example for a game (register the bridge, report an event).
