# Task: saving in Startech, main menu, new game and pause menu

Read Startech's root `CLAUDE.md`, and the `CLAUDE.md` of SaveFramework, QuestFramework and ProcSector. **The SaveFramework core task must be done.** Don't change ProcSector, QuestFramework or SaveFramework unless something needed is missing; then stop and report it.

## Decisions already made

- **Slots:** three, one pilot per slot. The main menu offers **Continue** (most recent save), **New game** (choose a slot, enter a callsign) and **Load** (choose a slot).
- **Saving:** autosave only, with no manual save button. Saves happen on docking, undocking, quest turn-in, arrival in a sector (`OnSectorReady`, after warps and respawns) and quitting.
- **After loading:** the player always starts docked at the home station (Terra). Position, current sector and mid-flight state aren't saved.
- **Death cargo is not saved:** it expires after 2 minutes anyway.
- **Never save mid-sequence:** while dead, respawning or warping. Defer the save until the sequence ends.

## Phase 0: discover, report, then stop

Report, with file and line references, where each of these lives today and how it's initialised for a new game:
- the callsign and ship id (`UPlayerProfileSubsystem`)
- credits
- cargo items and capacity
- ammo counts per type
- owned weapons, and the loadout or fitting (mounts and what's on them)
- hull and shield values, and whether docking repairs them
- the selected ammo type

Also report:
- the maps and the current start flow (which map opens, the game mode, any game instance class)
- whether a pause or escape menu exists, and how Escape is used today (the hub closes on Escape)
- the APIs to use:
  - ProcSector: `bAutoGenerateOnBeginPlay`, `GetMutableStateStore()`, `RequestSector`
  - QuestFramework: `GetAllStates`, `RestoreStates`, and the bounty board's save support

Then stop.

## Phase 1: participants

One participant per area, each a small class with one job, registering when its owner becomes available:

| Participant id | Saves | Notes |
|---|---|---|
| `Startech.Profile` | callsign | ship id is derived; don't store it |
| `Startech.Wallet` | credits | |
| `Startech.Cargo` | items by tag and count | capacity comes from the ship; don't store it |
| `Startech.Arsenal` | owned weapons, loadout, ammo counts per tag, selected ammo | weapons and ammo stored by tag or a stable id, never by object pointer |
| `Startech.Ship` | hull and shield | if docking already repairs fully, report it and skip this participant |
| `Startech.Quests` | `UQuestSubsystem::GetAllStates`, the bounty board's state (seed counter and current offers) | restores before the hub can open |
| `Startech.Sectors` | ProcSector's `FSectorStateStore` (`GetAllStates`, `SetAllStates`) | remembered loot and stations; mined asteroids if any |

**New game defaults** (`OnNewGame`): whatever the game uses today, gathered in one place: starting credits, starter weapon and ammo. Put the values in `UStartechSettings` and report them.

## Phase 2: start flow

**Main menu map `L_MainMenu`,** with a menu widget `WBP_MainMenu` (C++ parent `UMainMenuWidget`). Same approach as the hub: panels build their content in C++, and the Blueprint only places them. Same sci-fi style and `DA_HudStyle` colours.

- **Title:** "Startech".
- **Continue:** loads the most recently saved slot. Hidden if no slot is occupied.
- **New game:** opens the slot picker.
  - **Slot cards:** each shows "Empty", or the pilot name, play time (h:mm) and last saved date.
  - **Choosing an occupied slot** asks "Overwrite <name>? This can't be undone." with Overwrite and Cancel.
  - **Then callsign entry:**
    - 3–16 characters: letters, digits and single spaces
    - trimmed
    - the inline error "Use 3–16 letters, digits or spaces" until valid
    - **Start** creates the game
- **Load:** the same slot cards, occupied only. Each card has Load and a quiet Delete (with confirmation).
- **Quit:** closes the game.
- **Restored from backup:** if a slot was restored this way, show "Restored from backup" on its card.

**Game map start:**
- ProcSector's `bAutoGenerateOnBeginPlay` is off. Startech waits for all participants to load (`OnAllParticipantsLoaded`), then requests the home sector. On `OnSectorReady`, it places the player docked at the station.
- **Starting the game map without the menu** (Play In Editor) creates a temporary new game in a test slot, marked as such, so testing in the editor still works. That slot isn't shown in the menu. Report how you marked it.

## Phase 3: autosave and pause menu

**Autosave:** `RequestSave` with a reason at each trigger:
- `Docked`
- `Undocked`
- `QuestTurnedIn`
- `SectorReady`
- `Quit`, using `SaveNow`

If a request comes during a death, respawn or warp, it waits until that ends.

**Pause menu** (`WBP_PauseMenu`, C++ parent):
- Escape in flight opens it. Escape in the hub still closes the hub, as now.
- It pauses the game.
- **Resume**
- **Quit to main menu:** saves, closes the active slot, opens `L_MainMenu`
- **Quit game:** saves, then quits
- Quitting while not docked shows a warning first: "You'll resume docked at Terra station. Cargo in space and unfinished trips are lost." Your cargo hold, credits and quests are kept.

**Save indicator:** while a save is in flight and for 1 s after, a small "Saving…" then "Saved" in secondary text, above the cargo and credits panel in the HUD. Nothing appears on success beyond that, and a red "Save failed" stays for 5 s on failure.

## Manual editor steps (list them in the summary)

- Create `L_MainMenu` and set it as the Game Default Map; the game map stays the Editor Startup Map.
- Build `WBP_MainMenu` and `WBP_PauseMenu` from exact layout tables, as in earlier tasks.
- Any asset settings, such as ProcSector's auto-generate setting, if it lives in a config file you couldn't change.

## Conventions

The same as before: no `TMap`, const correctness, logic in C++, doc comments, Blueprint only for layout.

## Acceptance

1. Builds cleanly.
2. Summary: every participant, what it stores, and where it registers.
3. A test plan in the summary:
   - **New game:** enter a callsign. You start docked at Terra with the default credits, starter weapon and ammo. The player card shows the name.
   - **Saving and continuing:** play for a while (collect minerals, buy ammo, accept First contract and finish one objective, pick up remembered loot in Rust, have a bounty active). Dock, then quit to the main menu and press Continue. Everything matches: credits, cargo, ammo, weapons and loadout, quest progress, the same bounty offers, and the loot in Rust is still gone. You're docked at Terra.
   - **Timed quests:** accept Drifter sweep, quit at 2:30 left, then continue. It's still active at 2:30.
   - **Slots:** fill all 3 with different pilots. Each loads its own state. Overwriting asks for confirmation, and Delete empties a slot.
   - **Quit game** from the pause menu, then restart: Continue shows the latest slot.
   - **Backup:** `Save.CorruptActiveForTest`, then quit and continue. It loads from the backup and the card shows "Restored from backup".
   - **Play In Editor** on the game map still works without the menu.
   - **Deferred save:** die in Rust. No save happens during the death or respawn; one happens after respawn (on `SectorReady`).
