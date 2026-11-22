# SaveFramework

A reusable, game-agnostic save system for Unreal Engine 5.8. Each system in a game saves and loads its own data by registering as a **save participant**. The framework handles slots, debounced asynchronous saves, backups, play time, and loading before the game world exists.

Module `SaveFramework` (Runtime). Dependencies: Core, CoreUObject, Engine, DeveloperSettings.

---

## Concepts

### Slots
The project chooses how many slots it offers (`NumSlots`, default 3). Each slot file holds one save: a **header** (`FSaveSlotHeader`: display name, play time, date, versions) and one **record** per participant. Slot `N` is stored as `<SlotNamePrefix>N` through the platform save system (on Windows `Saved/SaveGames/Slot0.sav`), and its backup as `<SlotNamePrefix>N_Backup`.

### Participants
A participant is any UObject implementing `ISaveParticipant`: a component, a subsystem, an actor. It has a stable id (for example `Player.Wallet`), writes its data to bytes and reads them back. The framework stores the bytes as an opaque `FSaveRecord` together with the participant's **version** number and never interprets them.

### Field-level robustness
Participants normally keep their saved state in a `USTRUCT` and convert it with `FSaveSerialization`. That uses Unreal's tagged property serialization restricted to `SaveGame` properties, so fields are matched by name:
- a field removed from the struct is skipped when old data is read;
- a field added to the struct keeps its default value when old data is read.

Neither breaks old saves. Only changes in meaning (a renamed field, a changed unit, a split value) need the version number and a conversion.

### Late loading
Loading a slot normally happens in a menu, before the game world and its systems exist. `LoadSlot` only reads the file and keeps the records pending. Each participant receives its data when it registers:
- it has a record → `ReadSaveData(Data, SavedVersion)`;
- it has none → `OnNewGame()`.

A participant registered before the slot was loaded receives its data at the moment the slot is loaded or created. Records are applied in registration order. **Participants must not depend on each other's load order.** If one ever has to (for example to validate against another system), it listens to `OnAllParticipantsLoaded`, which fires once all ids listed in `ExpectedParticipantIds` have received their data. With an empty list it fires as soon as the slot is activated.

### Saving
`RequestSave(Reason)` is the normal call: call it whenever something worth keeping changes. Requests within `AutosaveDebounceSeconds` (default 1 s, real time, also while paused) merge into one save. The save collects a fresh record from every registered participant, carries over unchanged the records of participants that are not registered right now, and writes the slot asynchronously. Only one save is ever in flight; requests during it queue exactly one more.

`SaveNow(Reason)` saves synchronously, for quitting. When the game instance shuts down with a slot active, the subsystem calls it itself (`bSaveOnShutdown`, default true).

A participant that unregisters (for example when its level unloads) has its current data captured at that moment, so it is in the next save even though it is gone.

### Backups and corruption
Before a slot is overwritten, its current file is copied to `<Slot>_Backup`, but only if that file is intact, so a damaged file never replaces a good backup (`bKeepBackup`, default true). Every slot file carries a checksum over its records. When the main file is missing, unreadable or fails its checksum, `LoadSlot` uses the backup, logs a warning and reports it through `WasActiveSlotRestoredFromBackup` (and `FSaveSlotInfo::bRestoredFromBackup` in menus). When both are damaged, `LoadSlot` returns false with a reason.

`FSaveSerialization` data has its own checksum too: `BytesToStruct` returns false on corrupt data, without crashing and without touching the destination.

### Play time
Play time grows while a slot is active and the game is not paused, using the world's delta time (so it follows time dilation). It is stored in the header with each save.

---

## Adding a participant

A complete participant: a struct with `SaveGame` fields, the five interface functions, and registration.

```cpp
USTRUCT() struct FWalletSave
{
    GENERATED_BODY()
    UPROPERTY(SaveGame) int32 Credits = 0;
    UPROPERTY(SaveGame) TArray<FName> UnlockedItems;
};

UCLASS() class UWalletComponent : public UActorComponent, public ISaveParticipant
{
    GENERATED_BODY()
public:
    FWalletSave State;
    virtual FName GetSaveParticipantId() const override { return TEXT("Player.Wallet"); }
    virtual int32 GetSaveVersion() const override { return 1; }
    virtual void WriteSaveData(TArray<uint8>& Out) const override { FSaveSerialization::StructToBytes(State, Out); }
    virtual void ReadSaveData(const TArray<uint8>& Data, int32 SavedVersion) override
    {
        FWalletSave Loaded;                                    // starts at defaults
        State = FSaveSerialization::BytesToStruct(Data, Loaded) ? Loaded : FWalletSave();
    }
    virtual void OnNewGame() override { State = FWalletSave(); }
    virtual void BeginPlay() override { Super::BeginPlay(); USaveSubsystem::Get(this)->RegisterParticipant(this); }
    virtual void EndPlay(const EEndPlayReason::Type R) override { USaveSubsystem::Get(this)->UnregisterParticipant(this); Super::EndPlay(R); }
};
```

Then, whenever the wallet changes: `USaveSubsystem::Get(this)->RequestSave(TEXT("Wallet"));`

Notes:
- Register when the object is ready to receive data, and unregister before it is destroyed (`EndPlay`, `Deinitialize`). A participant destroyed without unregistering keeps its last *saved* data, not its latest changes.
- `ReadSaveData` and `OnNewGame` may run more than once in an object's life (a different slot is loaded). Each must fully reset the state.
- The id must be unique. A second object registering with an id already in use is refused with an error.
- A menu flow: `GetSlotInfos` to list slots → `CreateNewGame(Index, Name)` or `LoadSlot(Index, Reason)` → open the game level. Back to the menu: `SaveNow`, then `CloseActiveSlot`.

---

## Versioning and migration

1. **Adding or removing a field: nothing to do.** New fields take their struct default when old data is read; removed fields are ignored. Keep the version unchanged.
2. **Changing what a field means**, renaming it, or splitting one value into several: raise `GetSaveVersion()` and convert in `ReadSaveData` when `SavedVersion` is older:

   ```cpp
   // v1 stored Fuel in litres; v2 stores a 0..1 fraction.
   if (SavedVersion < 2) { Loaded.Fuel = Loaded.Fuel / MaxFuelLitres; }
   ```
   For a rename, keep reading the old field under its old name (keep it in the struct as a deprecated `SaveGame` field, or read an old struct type), copy it to the new field, and drop the old one after a few releases.
3. **Replacing the whole format**: keep the old struct type, read the bytes into it when `SavedVersion` is old, and convert to the new struct.
4. A record whose version is *newer* than the code (a save from a newer build) logs a warning and is still given to `ReadSaveData`; the participant decides what to do.
5. Never reuse or rename a participant id: the old record would be orphaned (it is still carried over unchanged, but nobody reads it).

## What never to store

- **Raw object pointers** (`UObject*`, `AActor*`, `TObjectPtr`, `TWeakObjectPtr`): they are stored as object paths at best, and runtime objects (spawned actors, pooled actors, widgets) never resolve after a restart. Store a stable id instead (a name, a GUID, a gameplay tag) and resolve it when loading.
- **Asset references as hard pointers**: use `TSoftObjectPtr` / `FSoftObjectPath`, which are stored as paths.
- **Derived or cached values** that can be rebuilt from other data (totals, UI state, caches).
- **Values that are not `SaveGame`**: they are silently skipped, including members of nested structs. `FGameplayTag`, `FGameplayTagContainer`, math types (`FVector`, `FRotator`, `FTransform`), `FName`, `FString`, `FText`, `FDateTime`, `FGuid` and soft paths are all safe.
- **Session handles** (timer handles, delegate handles, net ids), and anything platform- or machine-specific.
- **TMap**: not used in this project. Use a `TArray` of structs.

---

## Console commands (non-shipping)

| Command | Effect |
|---|---|
| `Save.Now` | Save the active slot immediately. |
| `Save.List` | Log every slot with its name, play time, date and size (and its backup's size). |
| `Save.Info` | Log the active slot, plus registered and pending participants. |
| `Save.Delete <Index>` | Delete a slot and its backup. |
| `Save.CorruptActiveForTest` | Truncate the active slot's file to half, to test the backup. `Save.Load` it afterwards. |
| `Save.New <Index> [Name]` | Start a new game in a slot. |
| `Save.Load <Index>` | Load a slot. |
| `Save.Close` | Close the active slot without saving. |
| `Save.Test.Register` / `Save.Test.Unregister` | Register or unregister the built-in test participant (`SaveFramework.Test`). |
| `Save.Test.Set <Counter> [Note]` | Change the test participant's data and request a save. |
| `Save.Test.Print` | Log the test participant's data. |
| `Save.Test.WriteV1 <0\|1>` | Make the test participant write its old version-1 layout (without `AddedField`), to test loading old data. |

Typical check of the whole cycle:

```
Save.New 2 Test, Save.Test.Register, Save.Test.Set 5 first, Save.Now, Save.Test.Set 9 second, Save.Now,
Save.Close, Save.Load 2                      -> Loaded: Counter=9
Save.CorruptActiveForTest, Save.Load 2       -> restored from backup, Counter=5
Save.Test.WriteV1 1, Save.Test.Set 77 x, Save.Now, Save.Close, Save.Test.WriteV1 0,
Save.Load 2                                  -> Converted version 1 data; AddedField defaulted to 42
```

Automation tests: `Automation RunTests SaveFramework` (serialization round trip, field changes, corrupt data, backup fallback).

---

## API reference

### `ISaveParticipant` (`SaveParticipant.h`)
A system that saves and loads its own data. Implement it on any UObject (a component, a subsystem, an actor) and call `USaveSubsystem::RegisterParticipant` when the object is ready, `UnregisterParticipant` before it goes away. Native only: Blueprints cannot implement it.

| Member | Description |
|---|---|
| `FName GetSaveParticipantId() const` | Unique, stable id of this participant's record, for example "Player.Wallet". Never change it once shipped. |
| `int32 GetSaveVersion() const` | The current version of this participant's data. Raise it when old data needs converting in ReadSaveData. |
| `void WriteSaveData(TArray<uint8>& OutData) const` | Writes this participant's data, usually with FSaveSerialization::StructToBytes. |
| `void ReadSaveData(const TArray<uint8>& Data, int32 SavedVersion)` | Restores this participant's data. SavedVersion is the GetSaveVersion that wrote it; convert when it is older. |
| `void OnNewGame()` | Resets to new-game defaults. Called when the active slot holds no record for this participant. |

### `FSaveSerialization` (`SaveSerialization.h`)
Converts USTRUCTs to bytes and back with Unreal's tagged property serialization, restricted to SaveGame properties. The bytes carry the engine and custom versions they were written with, plus a checksum.

| Member | Description |
|---|---|
| `template<T> static void StructToBytes(const T& Value, TArray<uint8>& OutData)` | Writes the SaveGame properties of Value into OutData (replacing its contents). |
| `template<T> static bool BytesToStruct(const TArray<uint8>& Data, T& OutValue)` | Reads Data into OutValue. Fields missing from Data keep their current value in OutValue. Returns false, leaving OutValue unchanged, when Data is empty, corrupt or truncated. |
| `static void StructToBytes(const UScriptStruct* Struct, const void* Value, TArray<uint8>& OutData)` | Writes the SaveGame properties of Value (an instance of Struct) into OutData. |
| `static bool BytesToStruct(const TArray<uint8>& Data, const UScriptStruct* Struct, void* OutValue)` | Reads Data into OutValue (an instance of Struct). Returns false, leaving OutValue unchanged, on bad data. |

### `USaveSubsystem` (`Runtime/SaveSubsystem.h`, `UGameInstanceSubsystem`)
The facade of the framework: slots, participants, saving, play time.

| Member | Description |
|---|---|
| `static USaveSubsystem* Get(const UObject* WorldContext)` | The save subsystem of WorldContext's game instance, or null outside a running game. |
| `TArray<FSaveSlotInfo> GetSlotInfos() const` | One entry per slot (NumSlots), read from disk the first time and cached; refreshed after each save or delete. |
| `void RefreshSlotInfos()` | Re-reads every slot header from disk and fires OnSlotsChanged. |
| `bool CreateNewGame(int32 SlotIndex, const FString& DisplayName)` | Makes SlotIndex the active slot with no records. Every participant gets OnNewGame (registered ones now, later ones on registration). Nothing is written until the first save. False when SlotIndex is out of range. |
| `bool LoadSlot(int32 SlotIndex, FText& OutReason)` | Reads SlotIndex (falling back to its backup) and makes it the active slot. Registered participants receive their record now, later ones on registration. False with OutReason when the slot is empty, out of range or unusable. |
| `bool DeleteSlot(int32 SlotIndex)` | Deletes SlotIndex and its backup. Closes the slot first when it is the active one. True when it is gone. |
| `int32 GetActiveSlot() const` | The active slot's index, or INDEX_NONE. |
| `bool HasActiveSlot() const` | True while a slot is active (after CreateNewGame or LoadSlot, until CloseActiveSlot). |
| `void CloseActiveSlot()` | Forgets the active slot and its records without saving (for returning to a menu; save first if needed). |
| `FSaveSlotHeader GetActiveHeader() const` | The active slot's header, with the current play time. Default when no slot is active. |
| `void SetActiveDisplayName(const FString& DisplayName)` | Changes the name the active slot shows in menus from its next save on. |
| `bool WasActiveSlotRestoredFromBackup() const` | True when the active slot was read from its backup because the main file was corrupt. |
| `FString GetSlotName(int32 SlotIndex) const` | The file name of SlotIndex (SlotNamePrefix + index). |
| `void RegisterParticipant(UObject* Participant)` | Registers Participant (a UObject implementing ISaveParticipant), held weakly. Idempotent. While a slot is active it immediately receives its record (ReadSaveData) or OnNewGame. Refused when another registered object already uses the same participant id. |
| `void UnregisterParticipant(UObject* Participant)` | Unregisters Participant. While a slot is active its current data is kept and written with the next save. |
| `bool IsParticipantRegistered(const UObject* Participant) const` | True when Participant is registered. |
| `TArray<FName> GetRegisteredParticipantIds() const` | Ids of the registered participants, in registration order. |
| `TArray<FName> GetPendingParticipantIds() const` | Ids of the active slot's records whose participant is not registered: their data waits for registration. |
| `void RequestSave(FName Reason)` | Asks for a save of the active slot. Requests within AutosaveDebounceSeconds of the first are merged; the save then runs asynchronously. A request while a save is writing queues exactly one more save. Ignored with no slot. |
| `bool SaveNow(FName Reason)` | Saves the active slot synchronously (for shutdown or quitting), after any save in flight. True on success. |
| `bool IsSaveInFlight() const` | True while an asynchronous save is writing. |
| `OnSaveStarted(FName Reason)` | A save started collecting records. |
| `OnSaveCompleted(bool bSuccess, FName Reason)` | A save finished writing, successfully or not. |
| `OnSlotLoaded(int32 SlotIndex)` | A slot became the active slot (LoadSlot or CreateNewGame). |
| `OnSlotsChanged()` | The slot list changed (after a save, a delete or RefreshSlotInfos). |
| `OnAllParticipantsLoaded()` | Every id in ExpectedParticipantIds has registered and received its data for the active slot. Fires once per slot. |
| `Initialize` / `Deinitialize` | Resets the runtime state / saves the active slot when bSaveOnShutdown is set, then stops the debounce timer. |
| `Tick(float DeltaTime)` | Adds the world's delta time to the active slot's play time. Not called while paused. |

### `USaveFrameworkSettings` (`SaveFrameworkSettings.h`)
Project Settings > Plugins > Save Framework, saved in DefaultGame.ini.

| Member | Description |
|---|---|
| `int32 NumSlots` (3) | How many save slots the game offers. Slot indices run from 0 to NumSlots - 1. |
| `FString SlotNamePrefix` ("Slot") | Prefix of each slot's file name: slot 0 is "<Prefix>0", its backup "<Prefix>0_Backup". |
| `float AutosaveDebounceSeconds` (1.0) | RequestSave calls within this many seconds of the first one are merged into a single save. |
| `bool bKeepBackup` (true) | Before a slot is overwritten, copy its last intact file to "<Slot>_Backup", used when the main file is corrupt. |
| `bool bSaveOnShutdown` (true) | Save the active slot synchronously when the game instance shuts down. |
| `FString ProjectVersion` ("1.0") | The project's own version, written into every slot header (for example "0.3.1"). |
| `TArray<FName> ExpectedParticipantIds` | Participant ids that must all have registered before OnAllParticipantsLoaded fires for the active slot. |
| `static const USaveFrameworkSettings& Get()` | The settings' class default object. |
| `FString MakeSlotName(int32 SlotIndex) const` | The file name of slot SlotIndex: SlotNamePrefix followed by the index. |
| `bool IsValidSlotIndex(int32 SlotIndex) const` | True when SlotIndex is between 0 and NumSlots - 1. |

### Data (`SaveTypes.h`)

**`FSaveSlotHeader`**: metadata of one saved slot, shown in menus without applying the save.

| Field | Description |
|---|---|
| `int32 SlotIndex` | The slot this save belongs to (0 to NumSlots - 1). |
| `FString DisplayName` | Name shown in menus. The project sets it, for example the pilot name. |
| `double PlayTimeSeconds` | Seconds played in this slot while the game was not paused. |
| `FDateTime SavedUtc` | When the slot was last written, in UTC. |
| `int32 FrameworkVersion` | The framework's file format version when the slot was written (USaveFrameworkSaveGame::CurrentFrameworkVersion). |
| `FString ProjectVersion` | USaveFrameworkSettings::ProjectVersion when the slot was written. |

**`FSaveRecord`**: one participant's saved data. The framework stores the bytes and never interprets them.

| Field | Description |
|---|---|
| `FName ParticipantId` | The participant that wrote the record (ISaveParticipant::GetSaveParticipantId). |
| `int32 Version` | The participant's data version when it wrote the record (ISaveParticipant::GetSaveVersion). |
| `TArray<uint8> Data` | The participant's opaque data, usually written by FSaveSerialization::StructToBytes. |

**`FSaveSlotInfo`**: a slot as a menu sees it.

| Field | Description |
|---|---|
| `FSaveSlotHeader Header` | The slot's header. Only SlotIndex is meaningful when bOccupied is false. |
| `bool bOccupied` | True when the slot holds a readable save (its main file or its backup). |
| `bool bRestoredFromBackup` | True when the main file was corrupt and this information comes from the backup. |

**`ESaveSlotLoadResult`**: outcome of reading a slot from disk. `Loaded` (the main file was intact), `RestoredFromBackup` (the main file was missing or corrupt; the intact backup is returned), `Empty` (neither the main file nor a backup exists), `Corrupt` (a file exists, but neither it nor the backup is intact).

### `USaveFrameworkSaveGame` (`SaveFrameworkSaveGame.h`, `USaveGame`)
The object written to a slot file: the header, one record per participant, and a checksum.

| Member | Description |
|---|---|
| `static constexpr int32 CurrentFrameworkVersion` (1) | The file format version written into new headers. Raise it only when the layout of this class changes. |
| `FSaveSlotHeader Header` | The slot's metadata. |
| `TArray<FSaveRecord> Records` | One record per participant, in no particular order. |
| `uint32 Checksum` | Checksum of Header.SlotIndex and every record (Seal). Declared last so it is serialized last: a truncated file loses it, and IsIntact then fails. |
| `uint32 ComputeChecksum() const` | The checksum of the current contents. Never 0, so a missing checksum never matches. |
| `void Seal()` | Stores ComputeChecksum in Checksum. Call after the last change, before writing. |
| `bool IsIntact() const` | True when the framework version is known and Checksum matches the contents. |
| `const FSaveRecord* FindRecord(FName ParticipantId) const` | The record of ParticipantId, or null. |

### `FSaveGameFactory` (`Factories/SaveGameFactory.h`)
Creates every save object the framework writes.

| Member | Description |
|---|---|
| `static FSaveRecord MakeRecord(const ISaveParticipant& Participant)` | A record holding Participant's current data, id and version. |
| `static FSaveSlotHeader MakeHeader(int32 SlotIndex, const FString& DisplayName, double PlayTimeSeconds)` | A header for SlotIndex stamped with the current time, framework version and project version. |
| `static USaveFrameworkSaveGame* MakeSaveGame(const FSaveSlotHeader& Header, const TArray<FSaveRecord>& Records)` | A new save game (in the transient package) holding Header and Records, sealed with its checksum. |

### `FSaveSlotStorage` (`Storage/SaveSlotStorage.h`)
Every file operation on slots. Stateless; works on slot names so tests can use their own files. User index 0.

| Member | Description |
|---|---|
| `static FString GetBackupSlotName(const FString& SlotName)` | The backup file name of SlotName: "<SlotName>_Backup". |
| `static bool DoesSlotExist(const FString& SlotName)` | True when the main file or the backup of SlotName exists. |
| `static USaveFrameworkSaveGame* LoadIntact(const FString& FileName)` | The save game in file FileName, or null (with a warning) when it is missing, unreadable or not intact. |
| `static ESaveSlotLoadResult LoadWithFallback(const FString& SlotName, USaveFrameworkSaveGame*& OutSaveGame)` | Reads SlotName, falling back to its backup when the main file is missing or corrupt. OutSaveGame is set for Loaded and RestoredFromBackup, null otherwise. |
| `static bool BackupExisting(const FString& SlotName)` | Copies the main file of SlotName to its backup, only when the main file is intact. True when copied. |
| `static bool SaveSync(USaveFrameworkSaveGame& SaveGame, const FString& SlotName)` | Backs up (when bKeepBackup is set), then writes SaveGame to SlotName synchronously. True on success. |
| `static void SaveAsync(USaveFrameworkSaveGame& SaveGame, const FString& SlotName, TFunction<void(bool)> OnCompleted)` | Backs up (when bKeepBackup is set), serializes SaveGame now, and writes it on a worker thread. OnCompleted runs on the game thread with the result. |
| `static bool DeleteSlot(const FString& SlotName)` | Deletes the main file and the backup of SlotName. True when nothing of it remains. |
| `static int32 GetFileSize(const FString& FileName)` | Size in bytes of file FileName, or 0 when it does not exist. |
| `static bool TruncateForTest(const FString& FileName)` | Truncates file FileName to half its size, to test the backup. True when the file existed. |
