# HUD task 1 of 2: the data the HUD needs, as events

Read Startech's root `CLAUDE.md` first, and `Plugins/ProcSector/CLAUDE.md` for anything touching the plugin. All their rules apply.

This task builds only the **data side** of the new HUD. Every value the HUD shows is exposed by gameplay code as an event (a delegate that fires when the value changes), so widgets never poll game state. Task 2 builds the widgets on top of this. **Do not create or change any widget in this task.**

## The HUD this serves

Sci-fi interface style. In brief:

- **Player card, top left:** portrait, the player's chosen name, segmented shield and hull.
- **Sector name, top centre,** with a banner on arrival.
- **Reticle:** when locked, the target's shield and hull as arcs. Below them, the target's name and distance, coloured by range state. Soft and hard lock look different.
- **3D radar, bottom left:** blips with stalks showing above or below.
- **Cargo and credits, bottom right,** with a pickup feed.
- **World markers:** gates, the station and death cargo, on screen only, with distance; death cargo also shows an expiry countdown.
- **Feedback:**
  - directional shield-hit ripple and hull-hit flash
  - low-hull warning below 25%
  - out-of-bounds red tint with countdown
  - HUD folds away during a warp and boots up on arrival
  - cockpit-glass frame in first person

## Phase 0: discover, report, then stop

Before changing anything, read the Startech source and report, with file and line references:

1. **Player shield and hull:** where they live (component, class), and what events exist today.
2. **Enemy shield and hull:** whether enemies use the same component. Task 2 reads a locked target's shield and hull through it.
3. **Damage:** where it is applied, and whether the source location and damage type (shield or hull) are known at that point.
4. **Cargo:** capacity, used amount, item add and remove, and existing events.
5. **Credits:** where they're stored, and existing events.
6. **Targeting:** the targeting component, how soft and hard lock are represented, and its existing events. Also the targetable component's flag and event from the pooling task.
7. **Weapon range:** the player's weapons and how their range is known. Enemy attack range in `EnemyData`.
8. **Radar:** the current radar code and how it gathers contacts.
9. **Out of bounds:** the current warning, red flash and destroy logic, and where its timer lives.
10. **Death cargo tracker:** from the earlier task, and how it exposes live pickups and expiry.
11. **Camera:** first and third person switching, and whether an event exists.
12. **Existing HUD:** current widgets and HUD class, and which ones the new HUD replaces.
13. **Player name:** whether any profile or save object exists yet.

Write this as a short inventory, then **stop and wait for confirmation**. If something below doesn't fit what you found, say so in the report instead of forcing it.

## Phase 1: events and queries

Add only what's missing. Reuse existing delegates where they already carry what's needed. All delegates are dynamic multicast where Blueprint needs them, native otherwise.

### Ship status (player and enemies share this)

On the shield and hull owner (likely the shared ship health component):

- `OnShieldChanged(float Current, float Max)` and `OnHullChanged(float Current, float Max)`
- `OnDamageTaken(float Amount, bool bHitShield, FVector SourceLocation)`, for the directional hit effects. If the source is unknown, pass the ship's own location; task 2 then shows a non-directional flash.
- `OnLowHullChanged(bool bIsLow)`, firing when hull crosses a threshold property `LowHullFraction = 0.25`. It fires on change, not every hit.
- Getters for current and max values, so a widget can initialise without waiting for an event.

### Cargo and credits

- Cargo: `OnCargoChanged(int32 Used, int32 Capacity)`, plus `OnItemAdded(FText DisplayName, int32 Quantity)` for the pickup feed.
- Credits: `OnCreditsChanged(int64 NewTotal, int64 Delta)`. Use the type credits already have if it differs; report it.
- Getters for initial values.

### Player identity

The name is chosen at the start of a game and fixed after that. Add a `UGameInstanceSubsystem`, `UPlayerProfileSubsystem` (or extend an existing profile object if Phase 0 finds one):

- `GetCallsign()`, `SetCallsign(FText)`, `OnCallsignChanged(FText)`. The default is "Captain" until a name is set.
- `GetShipId()`: a short generated tag like "SN-04", fixed per profile.
- The name-entry screen itself is out of scope. `SetCallsign` is the hook it will call later.

### Targeting

On the targeting component:

- `ETargetLockMode { None, Soft, Hard }`
- `OnTargetChanged(AActor* NewTarget, ETargetLockMode Mode)`, firing on any change of target or mode
- `OnHardLockEngaged()` and `OnHardLockReleased()`, for task 2's snap animation and tone
- Getters for the current target and mode

### Range state

A small C++ component on the player, `UTargetRangeComponent`, owning one question: where do I stand relative to my target's range and mine?

- `ETargetRangeState { OutOfRange, PlayerAdvantage, InEnemyRange }`:
  - `InEnemyRange`: the target can hit the player (its range from `EnemyData`).
  - `PlayerAdvantage`: the player can hit the target, but the target can't hit the player.
  - `OutOfRange`: otherwise.
- **Player range:** the longest range among the currently usable weapons. Report how you determined it.
- Evaluate on a timer at 10 Hz while a target exists, not on tick. Fire `OnRangeStateChanged(ETargetRangeState)` only when the state changes.
- `GetDistanceToTarget()`: a cheap getter. Task 2 reads it each frame for the number; that is the one value allowed to be read continuously, since it changes constantly.

### Sector, warp and bounds

- **Sector:** task 2 reads the plugin directly:
  - `USectorDirectorSubsystem::OnSectorReady`
  - `GetCatalog()->FindSector(id)` for `DisplayName` and `Gates.Num()`
  - `UWarpTransitionComponent::OnWarpStarted` and `OnWarpFinished`

  No new code is needed unless something is missing. Don't change the plugin for the HUD.
- **Out of bounds:** expose `OnOutOfBoundsChanged(bool bOutside)` and `GetOutOfBoundsSecondsRemaining()` from wherever the existing warning lives. Remove any old HUD drawing of the warning from that code; task 2 draws it. Report what you removed.

### World markers

A `UWorldSubsystem`, `UHudMarkerSubsystem`, holding a `TArray` (no `TMap`) of marker entries:

- the actor (weak pointer)
- a label (`FText`)
- a kind (`EHudMarkerKind { Gate, Station, DeathCargo }`)
- an optional expiry time

It exposes `Register`, `Unregister`, `GetMarkers()`, and `OnMarkersChanged`.

- **Gates:** on `OnSectorReady`, the subsystem unregisters old gates and registers every `AJumpGate` (via `TActorIterator`) with the label "Gate · " plus the destination's display name. No plugin change.
- **Station:** the station class registers itself on `OnAcquiredFromPool`, and unregisters on `OnReleasedToPool` and `EndPlay`.
- **Death cargo:** the tracker registers each pickup with its expiry time, and unregisters it when it's collected, expires or is stored on warp.

### Radar contacts

If the radar already gathers contacts, keep that and make sure each contact gives:
- its world position
- whether it's hostile, loot or neutral
- whether it's targetable

Task 2 computes the above-or-below stalk from positions. Report the existing shape. Don't rewrite it if it already works.

### Camera mode

`OnCameraModeChanged(bool bFirstPerson)`, or reuse an existing event.

## Phase 2: test commands

Claude Code can't play the game, and the person testing needs quick ways to trigger every HUD state. Add console commands under `Startech.HUD.`, available in non-shipping builds only:

| Command | Effect |
|---|---|
| `Startech.HUD.Damage <shield> <hull>` | Applies damage to the player from 1000 units ahead (fires real damage events) |
| `Startech.HUD.Cargo <amount>` | Adds that many units of a test item |
| `Startech.HUD.Credits <amount>` | Adds or removes credits |
| `Startech.HUD.Callsign <name>` | Sets the callsign |
| `Startech.HUD.OutOfBounds <0/1>` | Forces the out-of-bounds state on or off |

## Conventions

- No `TMap`.
- Const correctness throughout.
- All logic in C++.
- A `/** */` doc comment on every new class, member and function.
- Events fire only on real changes, never every frame.

## Acceptance

1. Builds cleanly.
2. Summary lists every new or changed event and getter, grouped as above, with its class and header, so task 2 can use it directly.
3. The test commands work. List them in the summary with the expected log output, and log every event once in non-shipping builds, under a `LogStartechHUD` category, so they can be checked in the Output Log before any widget exists.
