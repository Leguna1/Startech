# TASK_ControlsScreen: Phase 0 report

Discovery only; nothing in the project was changed. Keys and asset fields below come from a headless dump of the
assets (editor Python, `InputMappingContext` / `InputAction` / BP_ShipPawn defaults / DA_ActionBarLayout), not from
memory.

---

## 1. Mapping contexts and Input Actions

### There is one mapping context: `/Game/Inputs/IMC_Ship`

There are no others: no `PlayerMappableInputConfig` or key profile assets, no menu or hub context, and
`bEnableUserSettings` is not set (so Enhanced Input user settings and rebinding are off).

- **Added:** `AStartechShipPawn::PawnClientRestart` (`Source/Startech/Private/StartechShipPawn.cpp:592-607`), priority
  0, from `BP_ShipPawn.MappingContext` (`StartechShipPawn.h:309-310`). **It is never removed.**
- **Bindings:** `AStartechShipPawn::SetupPlayerInputComponent` (`StartechShipPawn.cpp:609-653`). The actions come from
  BP_ShipPawn properties (`StartechShipPawn.h:312-340`); IA_Boost and IA_Pause come from `UStartechSettings`
  (`StartechSettings.h:134,138`, `DefaultGame.ini:68-69`); IA_Slot1–6 come from DA_ActionBarLayout through
  `UActionBarComponent::SetupInput` (`ActionBarComponent.cpp:103-116`).

| Mapping (IMC_Ship) | Modifier | Action | Value type | Bound to | Trigger event |
|---|---|---|---|---|---|
| W | – | IA_Throttle | Boolean | `OnThrottle` → `SetThrottleInput` (`.cpp:725`) | Triggered + Completed |
| S | Negate | IA_Throttle | | (reverse; releasing both brakes, `StartechShipBase.cpp:232-242`) | |
| D | – | IA_Turn | Boolean | `OnTurn` → `RawTurnAxis` (`.cpp:726`) | Triggered + Completed |
| A | Negate | IA_Turn | | (turns in free flight, **strafes** with a hard lock, `.cpp:930-951`) | |
| Space Bar | – | IA_Vertical | Boolean | `OnVertical` (`.cpp:727`) | Triggered + Completed |
| Left Ctrl | Negate | IA_Vertical | | (down) | |
| G | – | IA_CameraSwitch | Boolean | `OnToggleView`: chase ↔ cockpit (`.cpp:734-737`) | Started |
| Mouse XY 2D-Axis | – | IA_Look | Axis2D | `OnLook`: free look, ignored with a hard lock or in the hub (`.cpp:973-988`) | Triggered |
| Left Mouse Button | – | IA_Fire | Boolean | `OnFireStarted/Completed` (`.cpp:709-710`) | Started / Completed |
| Escape | – | IA_Pause | Boolean | `OnPause` → `UStartechPauseMenuSubsystem::OpenPauseMenu` (`.cpp:656-663`) | Started |
| Mouse Wheel Axis | – | IA_CycleAmmo | Axis1D | `OnCycleSlot` → `UActionBarComponent::CycleActive` (`.cpp:713-723`) | Triggered |
| Tab | – | IA_TargetLock | Boolean | `UTargetingComponent::ToggleHardLock` (`.cpp:624`) | Started |
| F | – | IA_Interact | Boolean | `OnInteract`: opens the hub while docked (`.cpp:476-483`) | Started |
| 1 … 6 | – | IA_Slot1 … IA_Slot6 | Boolean | `UActionBarComponent::HandleSlotPressed(Index)` | Started |
| Left Shift | – | IA_Boost | Boolean | `OnBoostStarted/Completed` (`.cpp:666-677`) | Started / Completed |

The action bar slots (DA_ActionBarLayout):

| Key | Slot | Ammo | Group |
|---|---|---|---|
| 1 | IA_Slot1 | LL1 | Lasers |
| 2 | IA_Slot2 | LL2 | Lasers |
| 3 | IA_Slot3 | LL3 | Lasers |
| 4 | IA_Slot4 | LL4 | Lasers |
| 5 | IA_Slot5 | SD1 | Drainer |
| 6 | IA_Slot6 | SD2 | Drainer |

No action or mapping has triggers or action-level modifiers. **There are no gamepad keys anywhere**, so the screen gets
no gamepad column (as the spec says).

**Orphan:** `IA_SelectSlot` (Axis1D) is not mapped in IMC_Ship and nothing binds it.
`Tools/HUD/setup_action_bar.py:9,99-109` removed its mappings when IA_Slot1–6 replaced it. The screen lists only
mapped actions, so it won't appear either way. It could be deleted; see question 6.

### When each input is active

IMC_Ship stays registered for the pawn's whole life. What changes is the input mode and the pawn's own checks.

- **Main menu (L_MainMenu):** there is no ship pawn, so IMC_Ship is not registered at all. Input is UI-only
  (`StartechMenuGameMode.cpp:34`). **The menu's Controls screen therefore can't ask the Enhanced Input subsystem for
  keys** (see section 4).
- **Flight:** game-only (`FStartechInputState::SetGameOnly`), and every action works.
  - Boost is refused in the hub, the pause menu, a warp or when dead (`IsBoostAllowed`, `.cpp:679-693`).
  - Slot keys and the wheel do nothing while the hub or pause menu is open (`UActionBarComponent::IsInputBlocked`).
  - F only opens the hub when docked.
  - With a hard lock (the "combat stance"), A/D strafe, the nose and camera track the target, and mouse look is
    ignored (`StartechShipPawn.h:180`, `.cpp:904-913`).
- **Hub open:** UI-only (`.cpp:512`), so the viewport ignores IMC_Ship. The flight handlers also zero out when
  `IsHubOpen()`. The hub's own keys are in section 2.
- **Pause menu:** UI-only with the cursor shown (`StartechPauseMenuSubsystem.cpp:76`), and the game is paused.
  `UPauseMenuWidget::NativeOnKeyDown` handles Escape.
- **Dead or warping:** the bindings stay, but the pawn refuses them through `IsAlive` and `IsBoostAllowed`.

---

## 2. Inputs handled outside Enhanced Input

| Where | Input | What it does |
|---|---|---|
| `UStationHubWidget::NativeOnKeyDown` (`UI/Hub/StationHubWidget.cpp:65-81`) | **Escape** (hard-coded) or **the IA_Interact key** (read from IMC through `IsInteractKey`, `StartechShipPawn.cpp:559-565`) | closes the hub |
| same, then `UShopScreenWidget::HandleShortcutKey` → `FShopQuantity::FromKey` (`ShopQuantity.cpp:35-48`) | **1–5 and NumPad 1–5** (hard-coded `EKeys`) | picks ×1 / ×10 / ×100 / ×1,000 / Max, on the Shop tab only |
| `UShopEntryWidget` (`ShopEntryWidget.cpp:235,240`) via `UHubButtonWidget::GetLastClickModifiers` | **Shift + click** on Buy or Sell | trades Max for that click |
| `UHubButtonWidget` (`HubButtonWidget.cpp:262,275`), tab rail (`HubTabRailWidget.cpp:201`), quest rows (`QuestListRowWidget.cpp:146`) | Left mouse button | clicks |
| `UMainMenuWidget::NativeOnKeyDown` (`UI/Menu/MainMenuWidget.cpp:59-78`) | **Escape** | closes the dialog, or goes back one page |
| `UPauseMenuWidget::NativeOnKeyDown` (`UI/Pause/PauseMenuWidget.cpp:35-49`) | **Escape** | closes the dialog, or resumes |

**Things with no key at all:**

- **Docking:** fly into a station's docking zone; the overlap docks (`Station.cpp:176-188`). While docked the hull
  repairs (`Station.cpp:123`) and the banner shows the F prompt (`HudSectorBannerWidget.cpp:83`).
- **Undocking:** the hub's Undock button (`StartechShipPawn.cpp:540-556`), or flying out of the zone.
- **Warping:** fly into a jump gate's trigger (`ProcSector JumpGate.cpp:69-72`). It is refused in combat and retried
  while you stay inside.
- **Soft lock:** automatic. It picks the enemy nearest the centre of view within 12° and 200 m
  (`TargetingComponent.h:67,71`). Holding fire with a soft lock and A/D released turns the nose towards it (the fire
  assist, `StartechShipPawn.cpp:915-928`).

No Blueprint key events: `grep` of the C++ found only the handlers above. The widget Blueprints are layout only.

**Spec items that don't exist in the game:**

- **"Cycling targets":** there is no such input. Tab only pins or releases the current soft lock, and the soft lock
  follows the centre of the view.
- **"Free fire with the crosshair":** there is no cursor or crosshair aiming. With no target, shots go straight ahead
  of the ship (`TargetingComponent::GetAimPoint`, `TargetingComponent.cpp:69-80`), and the mouse only moves the camera.

In the proposed list below, these become descriptions of what does exist. See question 4.

---

## 3. Player Mappable Key Settings and descriptions

**None.** Every Input Action has:

- `ActionDescription` empty
- `PlayerMappableKeySettings` null

Every IMC_Ship mapping has `SettingBehavior = InheritSettingsFromAction` and no per-mapping settings.
`ContextDescription` is empty.

**What the engine fields could and couldn't hold:**

- `UPlayerMappableKeySettings` has `Name`, `DisplayName` and `DisplayCategory`, but no description and no order.
- `ActionDescription` is a single FText.
- Neither can hold:
  - the order within a group
  - the non-Enhanced-Input rows (hub Escape, shop 1–5, Shift-click, docking and warping)
  - rows that merge several actions (keys 1–4 → lasers)
  - the "How combat works" box

**Recommendation: `DA_ControlsHelp` (`UControlsHelpData`), the spec's fallback.**

- **Groups:** an ordered array, each with a display name.
- **Rows** (ordered), each with:
  - one or more Input Actions, whose keys come from the mapping contexts at runtime
  - or, for a row not on Enhanced Input, a fixed key list plus an optional "context" note (for example "Shop tab")
  - a display name
  - a one-line description
- **Tips:** the "How combat works" lines.
- **Contexts to read:** the list of mapping contexts (IMC_Ship).

`UStartechSettings::ControlsHelp` points at it. A script fills it (`Tools/UI/setup_controls_help.py`, idempotent).

Any mapped action that no row names still gets a row in an "Other" group: its asset name with "IA_" dropped, its keys,
and a logged warning.

Fixed keys on rows like shop 1–5 duplicate `FShopQuantity`. A unit test can check that they match
`FShopQuantity::FromKey`, so a code change can't leave the screen wrong.

---

## 4. How the menus are built, and adding a button and a screen

Both menus are C++ widgets placed in a thin Blueprint frame, the same pattern as the hub.

### Main menu

`WBP_MainMenu` (parent `UMainMenuWidget`, `UI/Menu/MainMenuWidget.h`) has this widget tree:

```
CanvasPanel
  Backdrop (UHubPanelWidget, full screen)
  PageSwitcher (UWidgetSwitcher, centred, auto-size)
    HomePanel (UMainMenuHomePanel)
    SlotsPanel (UMainMenuSlotsPanel)
    CallsignPanel (UMainMenuCallsignPanel)
  Dialog (UMainMenuDialog, full screen)
```

- **Buttons:** `UMainMenuHomePanel::BuildContent` (`MainMenuHomePanel.cpp:19-40`) adds them in C++: Continue,
  New game, Load, Quit. Each is a `UHubButtonWidget` with an `OnX` delegate that `UMainMenuWidget::NativeConstruct`
  binds (`MainMenuWidget.cpp:20-42`).
- **Adding Controls:** add `ControlsButton` (between Load and Quit) and `OnControls` in the home panel, all in C++.

### Pause menu

`WBP_PauseMenu` (parent `UPauseMenuWidget`, `UI/Pause/PauseMenuWidget.h`) has this widget tree:

```
CanvasPanel
  Backdrop (full screen)
  PausePanel (UPauseMenuPanel, centred, auto-size)
  Dialog (full screen)
```

- **Buttons:** `UPauseMenuPanel::BuildContent` (`PauseMenuPanel.cpp:24-61`): Resume, Quit to main menu, Quit game.
- **Adding Controls:** add a Controls button between Resume and Quit to main menu, all in C++.

### The screen

One `UControlsScreenWidget` (a `UHubWidgetBase`, `UI/Controls/`) is shared by both menus.

- **How each menu shows it:** `NativeConstruct` creates it in C++ and adds it to the Blueprint's root `CanvasPanel`,
  full screen and collapsed. This is the same move as `UStationHubWidget` adding `UpgradesTab` to its switcher
  (`StationHubWidget.cpp:24-28`).
- **Opening:** the menu hides its own page or panel and shows the screen.
- **Closing:** Back or Escape calls an `OnBack` delegate, and the menu shows its page or panel again.
- **Escape routing:** both menus' `NativeOnKeyDown` send Escape to the screen first while it is open. Otherwise the
  pause menu would resume and the main menu would do nothing.
- **Why not a PageSwitcher page:** the switcher auto-sizes and is centred, so a tall, scrolling list inside it would
  change the Home page's layout. A screen of its own on the canvas avoids that.
- **No manual Blueprint step is needed** for either menu. Optionally, the `Controls` slot could be marked
  `BindWidgetOptional`, so a later Blueprint can place it itself; the code would create it only when it is missing.

### Reading the keys

- **The problem:** the main menu has no pawn, so `UEnhancedInputLocalPlayerSubsystem` has no IMC_Ship registered there.
- **The fix:** the screen reads the mapping context assets listed in `DA_ControlsHelp`
  (`UInputMappingContext::GetMappings()`) in both menus. It shows the same thing in both, and a change to IMC_Ship
  shows at once.
- **Key names:** `FKey::GetDisplayName(false)`, as the action bar already does (`ActionBarComponent.cpp:169-170`).
- **Later rebinding:** the per-row widget would overlay the user's player-mapped keys at the same point.

### Pieces in place

- `DA_HudStyle` colours and font sizes through `UHubWidgetBase::GetHudStyle()` / `MakeText`
- `UHubPanelWidget` for the cut-corner panel
- `UScrollBox`, as in `QuestListWidget.cpp` and `HubUpgradesTabWidget.cpp`
- `UHubButtonWidget` (Quiet or Secondary) for Back

The keycap is a new small C++ widget: a chamfer panel (`FHudPanelBrush`) with the key text.

---

## 5. Proposed list (for you to check)

Keys show as the engine prints them (`GetDisplayName(false)`): "Space Bar", "Left Ctrl", "Mouse XY 2D-Axis" and
"Mouse Wheel Axis" are long or odd. **Proposal:** `DA_ControlsHelp` holds optional short labels per key ("Mouse",
"Wheel", "Space", "LMB"). The key itself still comes from IMC, and a key with no override uses the engine's name. See
question 2.

### Flight

| Keys | Name | Description |
|---|---|---|
| W / S | Throttle | Hold W to fly forward, S to reverse. Let go of both to brake to a stop. |
| A / D | Turn / strafe | Turn left and right. With a target locked (Tab), strafe around it instead. |
| Space / Left Ctrl | Climb / dive | Fly up and down. |
| Left Shift | Boost | Hold for a burst of speed. It drains the boost gauge under your speed, which refills when you let go. |
| Mouse | Look | Look around. While a target is locked, the camera follows the target. |

### Combat

| Keys | Name | Description |
|---|---|---|
| *(automatic)* | Soft lock | The enemy nearest the centre of your view is targeted automatically. |
| Tab | Lock target | Lock the current target: your ship faces it, A/D strafe around it. Press again to release. |
| Left Mouse | Fire | Hold to fire the active weapon group at your target. With no target, it fires straight ahead. |
| Wheel | Cycle ammo | Select the previous or next ammo on the action bar. |

The "Soft lock" row is info only. If you'd rather not have a keyless row, its text could move into the Tab row or the
tips; see question 4.

### Action bar

| Keys | Name | Description |
|---|---|---|
| 1 / 2 / 3 / 4 | Laser ammo | Load LL1–LL4 into the lasers and make them the firing group. LL1 is free and unlimited. |
| 5 / 6 | Drainer ammo | Load SD1–SD2 into the shield drainer and make it the firing group. |

The trigger fires only the active group, and each group remembers its own ammo. That could be its own info line or go
in the descriptions above.

### Station and interface

| Keys | Name | Description |
|---|---|---|
| *(fly in)* | Dock | Fly into a station's docking ring to dock. Your hull repairs while docked. |
| F | Station hub | While docked, open the station: shop, quests, upgrades. F or Esc closes it. Undock with the hub's Undock button. |
| *(fly in)* | Jump gate | Fly into a gate to warp to the next sector. Not possible in combat. |
| Esc | Pause menu | Pause the game. |
| 1 / 2 / 3 / 4 / 5 | Shop quantity | In the shop: buy or sell ×1, ×10, ×100, ×1,000 or Max. |
| Shift + click | Trade max | In the shop: Shift-click Buy or Sell to trade the most you can. |

### Camera

| Keys | Name | Description |
|---|---|---|
| G | Switch view | Switch between the chase camera and the cockpit. |

Mouse look stays under Flight. It could move here instead; see question 3.

### How combat works (tips box)

1. Lock a target with Tab and fire: your shots find it without aiming.
2. Locked on, A/D strafe around the target; W/S push in and pull back to hold your distance.
3. An enemy glowing red is charging a heavy shot: change how you're moving to dodge it.
4. Dock at Terra to repair, sell cargo and restock.

---

## Questions before Phase 1

1. **Where the descriptions live:** `DA_ControlsHelp` for everything (recommended, as above), or the engine fields on
   each Input Action (`PlayerMappableKeySettings` display name and category, `ActionDescription`) with
   `DA_ControlsHelp` only for order, merged rows, extra rows and tips?
2. **Short key labels:** OK to keep optional per-key overrides in `DA_ControlsHelp` ("Mouse", "Wheel", "Space", "LMB"),
   or always the engine's names?
3. **Rows:**
   - Keep merged rows (W / S, A / D, 1–4, 5–6)?
   - Mouse look under Flight or under Camera?
4. **Keyless rows** (soft lock, docking, jump gate): show them with an italic "auto" / "fly in" cap, or fold that text
   into other rows and the tips?
5. **Placement:** a full-screen screen over each menu, added in C++ (recommended, no Blueprint step), or a page in the
   main menu's PageSwitcher?
6. **IA_SelectSlot:** delete the unused asset, or leave it?
7. **The text above:** any wording changes to the rows or the tips?

Stopping here as asked.
