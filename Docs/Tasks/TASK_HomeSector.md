# Task: Terra is the home sector; dying anywhere returns you to its station

Read Startech's root `CLAUDE.md` first, and `Plugins/ProcSector/CLAUDE.md` for anything touching the plugin. All their rules apply.

This is a **Startech** task. The plugin already has everything needed (`USectorDirectorSubsystem::RequestSector`, `OnSectorReady`), so **don't change the plugin**. If something turns out to be missing there, stop and report it instead.

## Goal

- **One station:** only the home sector (Terra) has a station. Other sectors have none, so docking, selling, buying and repairs happen only at home.
- **Respawn at home:** dying in any sector respawns the player at the home sector's station. If the death happened elsewhere, the level regenerates as the home sector first, hidden behind the death or respawn screen.
- **Death cargo stays harsh, by decision:** it remains in the sector where the player died, with the normal 2-minute expiry counted in world time, whether or not the player is there. Dying far from home usually means losing the cargo. Don't change the tracker's expiry behaviour.

## Phase 0: discover, report, then stop

Report, with file and line references:

1. **Death and respawn today:** who handles them (game mode, player controller, pawn), any delay or death screen, and how the respawn location is chosen.
2. **How the station is found** for respawn and docking. Note anything that assumes a station exists in every sector, such as the HUD, shop, docking, markers, quest or tutorial code.
3. **Anything that runs on death:** the death cargo drop, the camera, the HUD reset, input.
4. **Possible clashes:**
   - What happens if the player dies during a warp (the director is busy, or the warp component is mid-fade).
   - What happens if the player dies while a respawn regeneration is already running.

Then stop and wait for confirmation.

## Phase 1: home sector and respawn

**Home sector setting.** A `HomeSectorId` property (default `Terra`) on the game mode or the relevant Startech settings class. Not in the plugin. Report where you put it and why.

**Respawn flow:**

1. **The player dies.** The existing death sequence runs unchanged: effects, cargo drop, any death delay.
2. **When the respawn is due:**
   - **Current sector is home:** respawn at the station exactly as today.
   - **Current sector is not home:**
     1. Keep the screen covered: fade to black, using the death screen if one exists.
     2. Call `RequestSector(HomeSectorId)`.
     3. Wait for `OnSectorReady(HomeSectorId)`.
     4. Find the station.
     5. Spawn or possess the player at the station's respawn point.
     6. Fade in.
3. **If `RequestSector` is refused because the director is busy** (for example, death during a warp): wait for the current `OnSectorReady`, then request home. Never leave the player stuck on a black screen. Add a timeout of 10 s that logs an error and respawns at the world origin as a last resort.
4. **The warp component** must end in a clean idle state if the player died mid-warp: input unlocked for the new pawn, no fade stuck on. Report what was needed.
5. **The HUD** after respawn shows the home sector banner and correct values, using the start-up fix done earlier. Check it doesn't need its own warp fold for this path; the death screen covers it.

**Station lookups.** Anything that assumed a station in every sector must handle having none:
- the docking prompt never appears
- the shop can't open
- no station marker is shown
- nothing errors or logs warnings every frame

## Phase 2: data

The station is placed by the presets' Core zone spawn entries. The Core zone will remain only in Terra's preset.

The `.uasset` files can't be edited, so list it as a manual step:
- In `DA_Preset_Rust` and any other non-home preset, delete the station spawn entry from the Core zone, or the whole Core zone if the station is its only entry.
- `DA_Preset_Terra` keeps it.

If the plugin's built-in test catalog (`FDefaultCatalogFactory`) is still used anywhere, leave it unchanged. It's test data in the plugin, and changing it would break the "don't change the plugin" rule. Mention it in the summary.

## Conventions

- No `TMap`.
- Const correctness throughout.
- Logic in C++.
- A `/** */` doc comment on everything new.

## Acceptance

1. Builds cleanly.
2. The summary lists the manual editor steps (the preset edits above) and every changed file.
3. A test plan in the summary:
   - Die in Terra: respawn at the station as before, with no regeneration.
   - Die in Rust: the screen stays covered, the level regenerates as Terra, you respawn at the station, and the HUD shows Terra with correct values.
   - Die in Rust carrying cargo, then fly back: the cargo is there only if you reach it within 2 minutes of the death; otherwise it's gone.
   - Rust has no station: no station marker, no docking prompt, no errors in the log.
   - Die by flying out of bounds in Rust: same as dying in Rust.
   - Die during a warp (`Startech.HUD.Damage 0 1000` right after entering a gate): you still end up at Terra's station, with no stuck fade or locked input.
   - Run `ProcSector.Goto Rust` while docked at Terra: you're undocked cleanly (from the earlier station undock fix) and nothing breaks.
