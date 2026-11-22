# SaveFramework plugin

Applies to all work inside `Plugins/SaveFramework`.

SaveFramework is a reusable, game-agnostic save system: numbered slots, each holding a header plus one opaque, versioned record per "save participant". Systems save and load their own data; the framework handles slots, debounced async saves, backups, late loading and play time. The plugin knows nothing about the host game.

- Engine: Unreal Engine 5.8 (C++). Runtime module `SaveFramework`, export macro `SAVEFRAMEWORK_API`, log category `LogSaveFramework`.
- Dependencies: Core, CoreUObject, Engine, DeveloperSettings. Nothing else.
- Host project: Startech, which connects to it in its own module. **The plugin must never reference Startech, ProcSector, QuestFramework or any game term.**
- Full guide and API reference: `Docs/DOCUMENTATION.md`. The code is the source of truth when they disagree.

## Folder layout (`Source/SaveFramework/Public` and `Private` mirror each other)

| Path | Contents |
|---|---|
| `SaveTypes.h` | Every plain data struct and enum: `FSaveSlotHeader`, `FSaveRecord`, `FSaveSlotInfo`, `ESaveSlotLoadResult`. Keep it the single home for data structs. |
| `SaveParticipant.h` | `ISaveParticipant`, the interface every saving system implements. |
| `SaveSerialization.h` | `FSaveSerialization`: USTRUCT to bytes and back (tagged, SaveGame-only, versioned, checksummed). |
| `SaveFrameworkSaveGame.h` | `USaveFrameworkSaveGame`, the object written to a slot file, with its integrity checksum. |
| `SaveFrameworkSettings.h` | `USaveFrameworkSettings` (Project Settings > Plugins > Save Framework). |
| `Factories/` | `FSaveGameFactory`: creates records, headers and sealed save games. |
| `Storage/` | `FSaveSlotStorage`: every file operation (load with fallback, backup, sync/async write, delete). |
| `Runtime/` | `USaveSubsystem`, the facade the game talks to. |
| `Private/Debug/` | Console commands, the test participant and its test structs (non-shipping use only). |
| `Private/Tests/` | Automation tests (`SaveFramework.*`). |

## How it works

1. A menu calls `CreateNewGame` or `LoadSlot`, usually before the game world exists. Loading reads the file through `FSaveSlotStorage::LoadWithFallback` and keeps the records in `KnownRecords`.
2. Each system calls `RegisterParticipant` when ready. It immediately gets `ReadSaveData` (its record exists) or `OnNewGame`. Participants already registered when a slot is activated get theirs then, in registration order.
3. `RequestSave` starts a debounce ticker (`FTSTicker`, real time, runs while paused). When it fires, every registered participant's record replaces its entry in `KnownRecords`, and the whole array is written asynchronously. Records of absent participants are written unchanged.
4. `UnregisterParticipant` captures the participant's current record into `KnownRecords`, so leaving a level never loses data.
5. Play time is added in `Tick` (`FTickableGameObject`, world delta, not while paused, only while a slot is active).

## Rules that must not be broken

### Game-agnostic
- No game class, module or term in the plugin. The project supplies everything through participants and settings.

### Data safety
- Every property of anything written to a file must have the `SaveGame` specifier, including members of nested structs: `USaveGame` files and `FSaveSerialization` both use `ArIsSaveGame`, which silently skips the rest.
- `USaveFrameworkSaveGame::Checksum` stays the **last** property: a truncated file loses it and fails `IsIntact`. Never add a property after it.
- Always `Seal` a save game after the last change (the factory does it). Never write one that is not sealed.
- The backup is only ever replaced by an intact main file (`BackupExisting`). Keep that check.
- Changing `FSaveSerialization`'s envelope or `USaveFrameworkSaveGame`'s layout breaks existing saves. Raise `EnvelopeVersion` / `CurrentFrameworkVersion` and keep reading the old layout.
- Participant ids are permanent. Renaming one orphans its saved record.

### Responsibilities
- The subsystem decides when; `FSaveSlotStorage` does all file work; `FSaveGameFactory` creates every save object. No file access in the subsystem, no policy in storage beyond `bKeepBackup`.
- At most one async save in flight (`bSaveInFlight`); a request during it sets `bSaveQueued`, never starts a second write.
- Participants are held weakly and applied in registration order. Never promise a load order between participants; `OnAllParticipantsLoaded` is the only ordering tool.

## Code conventions (the project owner's standing rules)

- No `TMap`. Use a `TArray` with linear search (`FindByPredicate`, `IndexOfByPredicate`); the collections are small.
- Const correctness throughout: const parameters, const getters, const locals where possible.
- All logic in C++. Blueprints only call the subsystem and display.
- Single responsibility, factories for creation, events instead of polling. Patterns in use: Facade (`USaveSubsystem`), Factory (`FSaveGameFactory`), Observer (the `On...` events), Command (console commands).
- Every class, struct, enum value, `UPROPERTY`, function and member variable in a public header has a `/** */` doc comment directly above it. The API reference in the documentation follows these comments.
- `#include "X.generated.h"` is the last include in a header. Forward declarations in headers, full includes in `.cpp` files.
- Log through `UE_LOG(LogSaveFramework, ...)`.

## Building and checking

The editor must be closed (or Live Coding disabled) before a command-line build:

```
"C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" StartechEditor Win64 Development -Project="D:\Unreal Projects\Startech\Startech.uproject" -WaitMutex
```

Automation tests (editor closed):

```
"C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\Unreal Projects\Startech\Startech.uproject" -ExecCmds="Automation RunTests SaveFramework; Quit" -unattended -nullrhi -stdout
```

Full cycle in a running game (results in `Saved/Logs/Startech.log`, category `LogSaveFramework`):

```
UnrealEditor-Cmd.exe "<uproject>" -game -nullrhi -unattended -stdout "-ExecCmds=Save.New 2 Test, Save.Test.Register, Save.Test.Set 5 a, Save.Now, Save.Close, Save.Load 2, quit"
```

All `-ExecCmds` run on the first frame, so use `Save.Now` there; debounced saves need the game to keep running.

When behaviour or the public API changes, update the matching prose section of `Docs/DOCUMENTATION.md` and its API reference entries, using the same wording as the doc comments.
