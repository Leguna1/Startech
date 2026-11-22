# Task: SaveFramework plugin (core)

Create a new, reusable runtime plugin, **SaveFramework**, in `Startech/Plugins/SaveFramework`. It must contain **nothing specific to Startech**: no ships, sectors, quests or references to other plugins. Any project uses it by registering "save participants". Startech connects to it in `Docs/Tasks/TASK_Save_Startech.md`.

Follow the code conventions in `Plugins/ProcSector/CLAUDE.md`:
- no `TMap`
- const correctness throughout
- logic in C++
- single responsibility, factories for creation
- events instead of polling
- a `/** */` doc comment on everything

Module `SaveFramework`, export macro `SAVEFRAMEWORK_API`, log category `LogSaveFramework`. Dependencies: Core, CoreUObject, Engine, DeveloperSettings.

## Concepts

- **Slots:** the project chooses how many (Startech uses 3). Each slot holds one save: a header (metadata) plus one record per participant.
- **Participants:** each system saves and loads its own data. The framework stores records as opaque bytes and never interprets them.
- **Field-level robustness:** participants serialize `USTRUCT`s through Unreal's tagged property serialization, restricted to `SaveGame`-flagged properties. Adding or removing a field never breaks old saves. For changes that need converting, each record carries a per-participant version number.
- **Late loading:** loading a slot happens before the game world exists (in a menu). Records are applied when each participant registers. A participant that registers after loading gets its record immediately; one with no record gets `OnNewGame`.

## Data

- **`FSaveSlotHeader` (`USTRUCT`):**
  - `int32 SlotIndex`
  - `FString DisplayName`: the project sets it, for example the pilot name
  - `double PlayTimeSeconds`
  - `FDateTime SavedUtc`
  - `int32 FrameworkVersion`
  - `FString ProjectVersion`: from the settings
- **`FSaveRecord` (`USTRUCT`):** `FName ParticipantId`, `int32 Version`, `TArray<uint8> Data`.
- **`USaveFrameworkSaveGame` (`USaveGame`):** `FSaveSlotHeader Header` and `TArray<FSaveRecord> Records`.
- **`FSaveSlotInfo`**, for menus: the header, plus `bool bOccupied` and `bool bRestoredFromBackup`.

## Participant interface (`ISaveParticipant`, a `UInterface`)

| Function | Purpose |
|---|---|
| `FName GetSaveParticipantId() const` | Unique and stable, for example `Player.Wallet` |
| `int32 GetSaveVersion() const` | The participant's current data version |
| `void WriteSaveData(TArray<uint8>& OutData) const` | Write this participant's data |
| `void ReadSaveData(const TArray<uint8>& Data, int32 SavedVersion)` | Restore it, converting from an older version if needed |
| `void OnNewGame()` | Reset to new-game defaults |

**Helpers** in `FSaveSerialization`:
- `template<typename T> static void StructToBytes(const T&, TArray<uint8>&)` and `static bool BytesToStruct(const TArray<uint8>&, T&)`.
- They use tagged property serialization with `ArIsSaveGame`, so only `SaveGame` properties are stored, and names (`FName`), soft object paths and gameplay tags survive. `BytesToStruct` returns false on corrupt data, without crashing.

## Runtime (`USaveSubsystem`, a `UGameInstanceSubsystem`)

**Slots:**
- `GetSlotInfos()`: reads every slot's header. Cache headers, and refresh after each save or delete.
- `CreateNewGame(SlotIndex, DisplayName)`: makes the slot active with empty records and a header. Every participant gets `OnNewGame` on registration. Nothing is written until the first save.
- `LoadSlot(SlotIndex)`: makes the slot active and keeps its records pending. Returns false with a reason if the slot is empty or both the file and its backup are corrupt.
- `DeleteSlot(SlotIndex)`: also deletes its backup.
- `GetActiveSlot()`, `HasActiveSlot()`, `CloseActiveSlot()` (for returning to a menu).

**Participants:**
- `RegisterParticipant(UObject*)`: idempotent, kept as a weak reference. On registration, applies the pending record or calls `OnNewGame`.
- `UnregisterParticipant(UObject*)`.
- **Order:** apply records in registration order. Document that participants must not depend on each other's load order. If one ever has to, it should listen to `OnAllParticipantsLoaded`, which the subsystem fires once every participant listed in the settings has registered.

**Saving:**
- `RequestSave(FName Reason)`: debounced, merging requests within `AutosaveDebounceSeconds` (default 1.0). Collects records from all registered participants, then saves asynchronously (`AsyncSaveGameToSlot`).
  - At most one save in flight; a request during a save queues one more.
  - Records of participants that aren't currently registered are carried over unchanged from the loaded data, so nothing is lost because a system wasn't present.
- `SaveNow(FName Reason)`: synchronous, for shutdown. On game instance shutdown, save automatically if a slot is active and the setting `bSaveOnShutdown` is true (default true).
- **Backup:** before overwriting, copy the existing file to `<Slot>_Backup`. On load, if the main file is corrupt or a record fails, fall back to the backup, log it, and set `bRestoredFromBackup`.

**Play time:** accumulates while a slot is active and the game isn't paused, using world delta time, and is stored in the header.

**Events:**
- `OnSaveStarted(Reason)`, `OnSaveCompleted(bool bSuccess, Reason)`
- `OnSlotLoaded(SlotIndex)`, `OnSlotsChanged()`
- `OnAllParticipantsLoaded()`

## Settings (`USaveFrameworkSettings`)

- `NumSlots` (default 3)
- `SlotNamePrefix` (default "Slot")
- `AutosaveDebounceSeconds`
- `bKeepBackup` (default true)
- `bSaveOnShutdown`
- `ProjectVersion`
- `ExpectedParticipantIds` (for `OnAllParticipantsLoaded`)

## Debug (non-shipping)

| Command | Effect |
|---|---|
| `Save.Now` | Save immediately |
| `Save.List` | Log every slot with its name, play time, date and size |
| `Save.Info` | Log the active slot, plus registered and pending participants |
| `Save.Delete <Index>` | Delete a slot |
| `Save.CorruptActiveForTest` | Truncate the active file, to test the backup |

## Documentation

- `Plugins/SaveFramework/CLAUDE.md`: architecture, rules, conventions, build command.
- `Plugins/SaveFramework/Docs/DOCUMENTATION.md`:
  - concepts
  - how a project adds a participant, with a complete 20-line example (a struct with `SaveGame` fields, the three functions, registering)
  - versioning and migration guidance
  - what never to store, such as raw object pointers
  - the console commands
  - an API reference

## Acceptance

1. Builds cleanly, with no references outside the plugin.
2. A small test participant inside the plugin (a non-shipping test class, registered only via a console command `Save.Test.Register`) proves the whole cycle:
   - write, save, reload, read back
   - adding a field to its struct keeps old saves loading
   - a corrupt file falls back to the backup
3. Summary: the public API and the integration example.
