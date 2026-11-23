# Task: Controls screen (main menu and pause menu)

Read Startech's root `CLAUDE.md` first. Reuse `DA_HudStyle`, `UHubButtonWidget` and the hub and menu widgets' patterns. Don't change the plugins.

## Goal

A **Controls** button in the main menu opens a screen that lists every input the player can use and what it does, grouped by purpose. New players (and the friend testing the build) should be able to learn the game from it without asking.

## Decisions already made

- **Read the keys from the game, don't type them in.** The screen builds its list from the Enhanced Input mapping contexts the game uses (`IMC_Ship` and any others), so it can never show a wrong key after a binding changes. Key names come from the engine's key display names ("Left Shift", "Mouse Wheel Up").
- **Descriptions live in data, next to the action.** Use each Input Action's own description fields (Player Mappable Key Settings display name and category, or `ActionDescription`), filled in by a script. If that doesn't fit the existing setup, use one small data asset (`DA_ControlsHelp`: action → display name, description, category, order). Nothing is hard-coded in the widget.
- **An action with no description still shows,** with its asset name and the key, so a new input is never silently missing. Log a warning for it.
- **Groups, in this order** (adjust names to what Phase 0 finds):
  1. **Flight:** forward, backward, turning, up and down, boost
  2. **Combat:** soft and hard lock, cycling targets, firing, free fire with the crosshair
  3. **Action bar:** keys 1–4 lasers (LL1–LL4), 5–6 drainer (SD1–SD2), and what selecting a group does
  4. **Station and interface:** open the hub at a station, warp at a jump gate, the pause menu, keys 1–5 for shop quantities and Shift-click for Max
  5. **Camera:** switching view, and anything else camera-related
- **Each row:** the key or keys on the left, styled like small keycaps; the action name; a one-line description of what it does. Several keys for one action are shown together ("W / S").
- **A short "How combat works" box** at the top or side, three or four lines: lock a target to fire without aiming, push and pull (strafe around your target and adjust your distance), watch for a red glow and change your movement to dodge charged shots, and dock at Terra to repair and sell. Text in the same data asset or a string table, not in code.
- **Also in the pause menu:** the same screen, reachable with a Controls button, so players can check a key in the middle of a session. Back returns to wherever it was opened from.
- **Style:** the hub's sci-fi style from `DA_HudStyle`, a Back button using `UHubButtonWidget`, Esc also goes back. Scrolls if the list is long.
- **Gamepad:** if the mapping contexts have gamepad keys, show them in a second key column. If not, don't add any.
- **No key rebinding in this task.** It may come later; the screen should be easy to extend with it (one row widget per action).

## Phase 0: discover, report, then stop

Report, with file and line references:
1. Every mapping context and Input Action the player can use, with their keys and modifiers, and which ones are active when (flight, hub, menus).
2. Which inputs are handled outside Enhanced Input (Blueprint key events, widget key handling such as the shop's 1–5 keys and Shift-click, Esc), so they can be listed too.
3. Whether the Input Actions already use Player Mappable Key Settings or have descriptions.
4. How the main menu and pause menu are built (C++ or the user's widget Blueprints), and how a button and a new screen would be added to each. If it needs a manual step in a Blueprint, say exactly what.
5. Your proposed list: group, keys, name and description for every action, for me to check before it goes in.

Then stop.

## Phase 1: build it

- The descriptions (script), the Controls screen, the row widget, and the buttons in the main menu and pause menu.
- Tests: the screen lists every action from the active mapping contexts; an action with no description still appears; changing a key in a mapping context changes what the screen shows.
- A real check: open it from the main menu and from the pause menu, take a screenshot of each, and confirm Back and Esc return correctly.

## Done when

- the main menu and pause menu both have a Controls button that opens the screen
- every input the player can use is listed with the right key and a clear description
- `CLAUDE.md` explains where the descriptions live and how to add one for a new input
