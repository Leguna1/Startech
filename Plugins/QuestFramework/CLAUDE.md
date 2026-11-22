# QuestFramework plugin

Applies to all work inside `Plugins/QuestFramework`.

QuestFramework is a reusable, game-agnostic quest runtime: data-asset quests with tagged objectives, prerequisites, time limits, category limits and turn-in, plus generated bounties. The game reports what happens as tagged `FQuestEvent`s and supplies inventory, rewards and the turn-in rule through a small bridge. The plugin knows nothing about the host game.

- Engine: Unreal Engine 5.8 (C++). Runtime module `QuestFramework`, export macro `QUESTFRAMEWORK_API`, log category `LogQuestFramework`.
- Dependencies: Core, CoreUObject, Engine, GameplayTags, DeveloperSettings. Nothing else.
- Host project: Startech, which connects to it in its own module. **The plugin must never reference Startech, ProcSector or anything space-specific** (no ships, sectors, docking). Use neutral words: target, item, context, place.
- Full guide and API reference: `Docs/DOCUMENTATION.md`. The code is the source of truth when they disagree.

## Folder layout (`Source/QuestFramework/Public` and `Private` mirror each other)

| Path | Contents |
|---|---|
| `QuestTypes.h` | Every plain data struct and enum: `EQuestStatus`, `FQuestEvent`, `FQuestReward`, `FQuestRuntimeState`, `FQuestCategoryLimit`, `FBountyTargetOption`, `FBountyLocationOption`. Keep it the single home for data structs. |
| `QuestTags.h` | Native Gameplay Tags: `Quest.Category.*`, `Quest.Event.*` (defined in `QuestFrameworkModule.cpp`). |
| `QuestBridge.h` | The game bridge: `IQuestInventoryProvider`, `IQuestRewardGranter`, `FQuestTurnInGate`. |
| `QuestFrameworkSettings.h` | `UQuestFrameworkSettings` (Project Settings > Plugins > Quest Framework). |
| `Definitions/` | `UQuestDefinition` (the quest data asset). |
| `Objectives/` | `UQuestObjective` and one subclass per kind (Destroy, Collect, Visit). |
| `Runtime/` | `UQuestSubsystem`, the facade the game talks to. |
| `Bounties/` | `UBountyTemplate`, `FBountyFactory`, `UBountyBoardSubsystem`. |
| `Debug/` | Console commands, the `Quest.Debug` overlay, the stand-in test bridge. |
| `Tools/` | Editor Python: `create_test_content.py` builds the test quests in `/QuestFramework/Test`. |
| `Config/Tags/` | Test-only gameplay tags (`QuestTest.*`). |

## How it works

1. At game-instance start `UQuestSubsystem` loads every `UQuestDefinition` under the settings' `QuestDefinitionPaths` (Asset Registry, recursive) and gives each a `FQuestRuntimeState`: `Locked` or `Available`.
2. `Accept` → `Active`. `ReportEvent` asks each counting objective `EvaluateEvent`; inventory objectives (`TracksInventory`) instead re-read the inventory with `QueryCurrent`.
3. All objectives done → `ReadyToTurnIn` (or completes at once when `bRequiresTurnIn` is false). `TurnIn` checks the gate, applies `ApplyTurnIn` (consumed items), grants rewards → `Completed` (or the repeat cooldown) and unlocks dependants.
4. Time limits and cooldowns count down in `Tick` (FTickableGameObject, world delta, not while paused).
5. The bounty board registers generated quests (`FBountyFactory`) with the quest subsystem like any other quest.

## Rules that must not be broken

### Game-agnostic
- No game class, module or term in the plugin. Anything only the game knows goes through `QuestBridge.h` or an event tag.
- Bridge objects are held weakly (`TWeakInterfacePtr`, delegates bound with `BindUObject`/`BindWeakLambda`).

### State
- Definitions and objectives are read-only while playing. Every changing value lives in `FQuestRuntimeState`, which holds only plain `SaveGame` values: it is the whole save.
- `Definitions` and `States` in `UQuestSubsystem` are parallel arrays. Change them together (`RegisterDefinition`, `UnregisterDefinition`, `RestoreStates`), never one alone.
- Status changes go through `SetStatus`, progress through `SetProgress`; both queue notifications.

### Events
- Every `On...` event is queued and broadcast by `FlushNotifications` at the end of the public call that caused it, so listeners can call back in safely. Never broadcast directly while iterating `States`.
- Never remove definitions while iterating `States`: collect ids, act after the loop (see `ReportEvent`).
- No polling for gameplay: the only tick is the timer tick, and it only runs while a clock or cooldown is running (`IsTickable`). The debug overlay is the one exception.

### Determinism (bounties)
- `FBountyFactory::Generate` must return the same quest for the same template and seed: saved bounties are regenerated from `SourceTemplateId` and `GenerationSeed`.
- The draw order is fixed: target, count, location, time. Add new draws at the end only. `PickWeighted` always makes exactly one draw.
- Changing a template's options changes the bounties its existing seeds generate. Say so in the summary when it happens.

### Responsibilities
- Objectives are strategies: a new kind is a new `UQuestObjective` subclass in `Objectives/`, never a switch in the subsystem.
- The subsystem is the facade; the board only generates and registers; the factory only builds.

## Code conventions (the project owner's standing rules)

- No `TMap`. Use a `TArray` with linear search (`FindByPredicate`, `IndexOfByPredicate`); the collections are small.
- Const correctness throughout: const parameters, const getters, const locals where possible.
- All logic in C++. Blueprints only display.
- Single responsibility, factories for creation, events instead of polling. Patterns in use: Facade (`UQuestSubsystem`), Strategy (objectives), Factory (`FBountyFactory`), Observer (the `On...` events), Command (console commands).
- Every class, struct, enum value, `UPROPERTY`, function and member variable in a public header has a `/** */` doc comment directly above it. The API reference in the documentation follows these comments.
- `#include "X.generated.h"` is the last include in a header. Forward declarations in headers, full includes in `.cpp` files.
- Log through `UE_LOG(LogQuestFramework, ...)`.

## Building and checking

The editor must be closed (or Live Coding disabled) before a command-line build:

```
"C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" StartechEditor Win64 Development -Project="D:\Unreal Projects\Startech\Startech.uproject" -WaitMutex
```

Test content (editor closed):

```
"C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\Unreal Projects\Startech\Startech.uproject" -run=pythonscript -script="D:\Unreal Projects\Startech\Plugins\QuestFramework\Tools\create_test_content.py" -unattended -nopause -nosplash -stdout
```

Claude Code cannot open the editor or edit `.uasset` files by hand. When a change needs editor work, finish the C++ (or an editor script in `Tools/`) and end the summary with exact manual steps.

When behaviour or the public API changes, update the matching prose section of `Docs/DOCUMENTATION.md` and its API reference entries, using the same wording as the doc comments.
