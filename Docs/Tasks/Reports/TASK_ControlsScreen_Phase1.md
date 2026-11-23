# TASK_ControlsScreen: Phase 1 report

Both the main menu and the pause menu now have a **Controls** button. It opens one shared screen that lists every input
in five groups, under a "How combat works" box. The keys are read from IMC_Ship each time the screen opens. The names,
descriptions, short key labels and tips are in `DA_ControlsHelp`, written by a script. No Blueprint needed a manual
edit.

Your Phase 0 answers were all applied:

- descriptions only in `DA_ControlsHelp`
- short key labels, with the engine's name as the fallback
- merged rows, and mouse look under Camera
- dimmed "Auto" and "Fly in" caps
- full screen over each menu
- IA_SelectSlot kept
- your wording, unchanged

## How it works

### Data: `DA_ControlsHelp`

`/Game/UI/Controls/DA_ControlsHelp`, class `UControlsHelpData`, chosen in `UStartechSettings::ControlsHelp`
(`DefaultGame.ini`). It holds:

- `MappingContexts`: IMC_Ship
- `Groups`: Flight, Combat, Action bar, Station and interface, Camera
- `Rows`: one per input (see the next section)
- `KeyLabels`: Mouse, Wheel, Space, Left Mouse, Esc
- the tips' title and lines
- the "Other" and "Unbound" texts

A row takes its keys from one of three places:

- **Actions:** one or more Input Actions, with their keys read from the contexts.
- **FixedKeys:** inputs handled outside Enhanced Input, joined by "/" or "+".
- **KeyNote:** a dimmed cap for an input with no key.

A row can also set `RowKeyLabels`. Trade max uses them to show "Shift + click".

`Tools/Menu/setup_controls_help.py` rewrites the asset from its tables on every run (idempotent). It then checks that
every mapped action has a row. Its report: `Wrote 5 groups, 18 rows, 5 key labels, 4 tips. Every mapped action has a
row.`

### The list: `FControlsList` (`Controls/ControlsList.h`)

`FControlsList::Build` turns the data plus the contexts' current mappings into groups of rows. It has no widget code,
so the screen and the tests share it.

- **Keys:** mapping order, each key once. Labels come from the row, then `KeyLabels`, then
  `FKey::GetDisplayName(false)`.
- **Unmapped:** a row whose actions have no key shows a dimmed "Unbound" cap.
- **Undescribed:** a mapped action that no row names goes under "Other", with its asset name minus "IA_" and its keys.
  It logs `[Controls] IA_X (keys) has no row in DA_ControlsHelp`.
- **Unknown group:** a row naming a group that doesn't exist is logged.

### Widgets (`UI/Controls/`)

- `UControlsScreenWidget`: a centred panel 960 px wide, at most 780 px tall. It has the title, the tips box, the
  scrolling list and a Secondary Back button.
  - It rebuilds on every `Open()`, so a changed key shows the next time it opens.
  - Back or Esc calls `Close()`, which fires `OnClosed`.
- `UControlsRowWidget`: one per row. It shows the keycaps (210 px column), the name (170 px) and the description
  (fills the rest, wraps). Rebinding later belongs here.
- `UControlsKeyCapWidget`: a cut-corner `UHubPanelWidget` around the label, at 55% opacity when dimmed.

Every colour and font comes from `DA_HudStyle` through `UHubWidgetBase`.

### Menus

- `UControlsScreenWidget::AddToMenu(Menu)`, called from each menu's `NativeOnInitialized`, adds the screen to the
  Blueprint's root canvas. It goes in collapsed, full screen, at Z order 100. **No Blueprint was edited.**
- **Main menu:**
  - Controls sits between Load and Quit (`UMainMenuHomePanel`).
  - Opening it collapses the PageSwitcher.
  - Closing restores the switcher's earlier visibility and shows the home page.
- **Pause menu:**
  - Controls sits between Resume and Quit to main menu (`UPauseMenuPanel`).
  - Opening it collapses the PausePanel.
  - Closing restores the panel; the game stays paused.
- **Esc:** both menus' `NativeOnKeyDown` send Esc to the screen first while it is open. In the pause menu, Esc
  therefore returns to the menu; a second Esc resumes.

### Test aids (non-shipping)

- `Startech.Test.Controls [log|open|back]` logs every row as drawn, or makes a real Slate click on the menu's Controls
  button or Back.
- `Startech.Test.Sequence <a;wait 2;b>` runs commands spaced out in real time, and it works on the menu map.
- The runner itself, `FStartechCommandQueue` (`Flow/StartechCommandQueue.h`), was moved out of
  `UStartechStartFlowSubsystem`, which now uses it too. `Startech.Flow.OnStartCommands` behaves as before; the pause
  check below used it.

## The list as shown (from the running game)

| Group | Keys | Name | Description |
|---|---|---|---|
| Flight | W / S | Throttle | Hold W to fly forward, S to reverse. Let go of both to brake to a stop. |
| Flight | A / D | Turn / strafe | Turn left and right. With a target locked (Tab), strafe around it instead. |
| Flight | Space / Left Ctrl | Climb / dive | Fly up and down. |
| Flight | Left Shift | Boost | Hold for a burst of speed. It drains the boost gauge under your speed, which refills when you let go. |
| Combat | *Auto* | Soft lock | The enemy nearest the centre of your view is targeted automatically. |
| Combat | Tab | Lock target | Lock the current target: your ship faces it, A/D strafe around it. Press again to release. |
| Combat | Left Mouse | Fire | Hold to fire the active weapon group at your target. With no target, it fires straight ahead. |
| Combat | Wheel | Cycle ammo | Select the previous or next ammo on the action bar. |
| Action bar | 1 / 2 / 3 / 4 | Laser ammo | Load LL1–LL4 into the lasers and make them the firing group. LL1 is free and unlimited. |
| Action bar | 5 / 6 | Drainer ammo | Load SD1–SD2 into the shield drainer and make it the firing group. |
| Station and interface | *Fly in* | Dock | Fly into a station's docking ring to dock. Your hull repairs while docked. |
| Station and interface | F | Station hub | While docked, open the station: shop, quests, upgrades. F or Esc closes it. Undock with the hub's Undock button. |
| Station and interface | *Fly in* | Jump gate | Fly into a gate to warp to the next sector. Not possible in combat. |
| Station and interface | Esc | Pause menu | Pause the game. |
| Station and interface | 1 / 2 / 3 / 4 / 5 | Shop quantity | In the shop: buy or sell ×1, ×10, ×100, ×1,000 or Max. |
| Station and interface | Shift + click | Trade max | In the shop: Shift-click Buy or Sell to trade the most you can. |
| Camera | G | Switch view | Switch between the chase camera and the cockpit. |
| Camera | Mouse | Look | Look around. While a target is locked, the camera follows the target. |

There are no gamepad mappings, so there is no gamepad column (as decided).

## Tests

`Automation RunTests Startech.Controls`: **5/5 pass**. `Startech.Flow` (1) and `Startech.Shop` (5) also pass after the
command-runner refactor.

| Test | Checks |
|---|---|
| `ListsEveryMappedAction` | With a transient context and data, every mapped action appears, the groups keep their order, and a row's keys come out in mapping order ("W / S"). |
| `UndescribedActionStillShows` | An action no row names shows under "Other", with its asset name and key, no description, and the logged warning. |
| `KeyChangeShows` | Unmapping W and mapping Up in the context changes the row from "W / S" to "S / Up" on the next build. |
| `KeyLabelsAndNotes` | A short label replaces the engine name, a row label wins, engine names are the fallback, "+" joins a combination, a key note is a dimmed cap, and an unmapped action shows "Unbound". |
| `RealDataCoversEveryInput` | The real `DA_ControlsHelp` reads BP_ShipPawn's own `MappingContext` and describes every mapped action. Its five groups are in order, every row has a name, a description and a key, there are four tips, and the shop-quantity row's keys match `FShopQuantity::FromKey`, in order. |

Every unit-test run logs six `LogAutomationTest: Error: Condition failed` lines while the engine starts, before any
test runs. They are **already there in yesterday's logs** (`Startech-backup-2026.11.22-*.log`), so they aren't from
this task.

## Real check (windowed `-game`, 1600×900)

All clicks are real Slate mouse clicks (`Startech.Test.Controls open|back`), and Esc is a real key event
(`Startech.Test.KeyDown Escape`).

**Main menu** (`L_MainMenu`; logs `Saved/Logs/ControlsMenu.log`, `ControlsMenu2.log`):

- Controls is on the home page ([main_menu_home.png](ControlsScreen/main_menu_home.png)).
- Clicking it opens the screen with all 18 rows and 4 tips
  ([main_menu_controls.png](ControlsScreen/main_menu_controls.png)).
- Back closes it.
- Opening it again and pressing Esc closes it and shows the home page
  ([main_menu_after_escape.png](ControlsScreen/main_menu_after_escape.png)).
- Input stays UI-only, with the cursor on.

**Pause menu** (`/Game/StarterMap`, test slot; log `Saved/Logs/ControlsPause.log`):

- Undock, then real Esc: the pause menu opens.
- Clicking Controls opens the screen with 18 rows ([pause_menu_controls.png](ControlsScreen/pause_menu_controls.png)).
- Esc closes the screen and leaves you **in the pause menu**, still UI-only and paused.
- Controls again, then Back: you are back in the pause menu.
- Esc resumes the game. Input is handed back: the viewport takes input, the cursor is off and focus is on SViewport.

The first main-menu screenshot showed wrapped descriptions touching the next row. I added 6 px of vertical padding to
every column and narrowed the key column from 250 to 210 px. The pause screenshot and the second main-menu run show
the fixed layout.

## Files

New:

- `Source/Startech/Public/Controls/ControlsHelpData.h`, `Private/Controls/ControlsHelpData.cpp` (also `LogStartechControls`)
- `Source/Startech/Public/Controls/ControlsList.h`, `Private/Controls/ControlsList.cpp`
- `Source/Startech/Private/Controls/ControlsConsoleCommands.cpp`
- `Source/Startech/Public/UI/Controls/ControlsScreenWidget.h` / `.cpp`
- `Source/Startech/Public/UI/Controls/ControlsRowWidget.h` / `.cpp`
- `Source/Startech/Public/UI/Controls/ControlsKeyCapWidget.h` / `.cpp`
- `Source/Startech/Public/Flow/StartechCommandQueue.h`, `Private/Flow/StartechCommandQueue.cpp`
- `Source/Startech/Private/Tests/StartechControlsTests.cpp`
- `Tools/Menu/setup_controls_help.py`
- `Content/UI/Controls/DA_ControlsHelp.uasset` (made by the script)

Changed:

- `StartechSettings.h` (`ControlsHelp`) and `Config/DefaultGame.ini` (`ControlsHelp=...`)
- `MainMenuHomePanel.h/.cpp`, `MainMenuWidget.h/.cpp` (button, screen, Esc)
- `PauseMenuPanel.h/.cpp`, `PauseMenuWidget.h/.cpp` (button, screen, Esc)
- `StartechStartFlowSubsystem.h/.cpp` (uses `FStartechCommandQueue`; no behaviour change)
- `CLAUDE.md` (new "Controls screen" section: where the descriptions live and how to add a new input)

The editor build (`StartechEditor Win64 Development`) succeeded with no warnings.

## Notes

- The "Station hub" description says "F or Esc closes it". That F is text, not read from IMC_Ship, so it would go stale
  if Interact were rebound. The F keycap on the same row would update.
- Rebinding later: `UControlsRowWidget` is the place, and `FControlsList::GetMappedKeys` is the single point where
  player-mapped keys (Enhanced Input user settings) would override the context's keys.
