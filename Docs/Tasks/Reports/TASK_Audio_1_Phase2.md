# TASK_Audio_1: Phase 2 report (the Settings page)

**Status:** Phase 2 is done, and so is the laser-hit re-trim you asked for. Both menus have a **Settings** button that
opens a full-screen Settings page:

- one volume slider per sound group (Menu music only in the main menu)
- changes are heard live
- Weapons and Explosions play a sample when you let go of the slider
- Reset to defaults
- the volumes are saved per machine when the page closes, and applied at the next start before the menu music begins

**Checks:** the editor build has no warnings, and every Startech test passes (43/43: the 8 audio tests and the rest).
Three windowed runs checked both menus with real mouse clicks, drags and Esc presses, a restart, and the
menu → new game path on test slot 3.

## The laser hit, re-trimmed

`Tools/Audio/setup_audio.py` now has two trim modes:

- **`start`:** from the first sound; used by the laser start, as before.
- **`peak`:** around the loudest sample. The window starts `PEAK_LEAD_SECONDS` (0.03 s) before it and has a 0.01 s
  fade-in; both modes keep 0.25 s and fade out over 0.08 s.

The script's report for the hit:

> `SW_Laser_Hit (trimmed from direct-hit-sound-anime around its loudest point at 0.393 s: from 0.363 s, 0.250 s kept, 0.01 s fade-in, 0.08 s fade-out)`

The old cut was 0.106–0.356 s, so it ended just before the impact. The new one is 0.363–0.613 s. The original file is
unchanged.

## What was built

### Per-machine volumes: `UStartechUserSettings`

- **What it is:** a `UGameUserSettings` subclass, registered as `GameUserSettingsClassName` in
  `DefaultEngine.ini`. It holds `FStartechVolumes` and `bHasSavedVolumes` in `GameUserSettings.ini` (the user's
  `Saved/Config`), per machine and not per slot.
- **When it loads:** the engine loads it before any map. `UStartechAudioSubsystem::Initialize` starts with
  `LoadSavedVolumes()` (the defaults until something is saved), so the saved volumes are in the mix before the main
  menu's music starts. The run-B log shows `[Audio] volumes in L_MainMenu: … music 0.40 menu 0.30 weapons 0.50`
  before the first take.
- **API:** `UStartechAudioSubsystem::SaveVolumes()` writes them, and `PlayPreview(Volume)` plays a slider's sample as
  a UI sound. `ClearSavedVolumes()` forgets them; the tests use it to put the machine back.

### Menu pages: `UMenuPageWidget` (refactor)

- **What it is:** the full-screen page pattern from the Controls screen, made a base class. It provides
  `AddToMenu<T>`, `Open`/`Close`, the `OnPageOpened`/`OnPageClosing` hooks, `OnClosed`, and Esc to close.
  `UControlsScreenWidget` now derives from it, with no change in behaviour.
- **In the menus:** both menus hold their pages and use `OpenPage`/`HandlePageClosed`/`GetOpenPage`. Esc closes
  whichever page is open; Back or Esc returns home in the main menu and to the still-paused panel in the pause menu.
- **No Blueprint was edited.**
- **Buttons:**
  - Main menu: Continue, New game, Load, **Settings**, Controls, Quit.
  - Pause menu: Resume, **Settings**, Controls, Quit to main menu, Quit game.

### `UHubSliderRowWidget`: a reusable slider row (`UI/Hub/`)

- **Look:** painted in C++ from DA_HudStyle: the label, a track (empty-segment colour, Player-colour fill), a thumb,
  and the value as "70%". The row brightens while hovered, dragged or focused.
- **Input:** drag or click on the track; Left/Right step 5% while it has focus; values snap to 1%. `OnValueChanged`
  fires while it moves, `OnValueCommitted` on release.

### `USettingsScreenWidget` (`UI/Settings/`)

- **Layout:** the title "Settings", an "Audio" heading, five rows (Master, Music, Menu music, Weapons, Explosions),
  then **Reset to defaults** (Secondary) and **Back**.
- **Menu music row:** `SetShowMenuMusic(true)` in the main menu and `false` in the pause menu (the row is collapsed).
- **Live:** every slider move calls `SetVolumes` at once, so Music, Menu music and Master are heard changing. Letting
  go of Weapons or Explosions plays `Audio.WeaponsPreviewSound` (SW_Laser_Start) or `ExplosionsPreviewSound`
  (SW_Explosion) once.
- **Reset** applies `Audio.DefaultVolumes`. **Closing** (Back or Esc) saves.

### Debug (non-shipping)

`Startech.Test.Settings [log|open|back|reset|drag <Volume> <Percent>]`:

- The clicks and drags are real Slate mouse events: down on the thumb, moves, then up.
- The log shows the menu, whether the page is open, whether the game is paused, every row (hidden ones marked), and
  the volumes in use and saved.
- The preview logs `[Audio] preview <sound> (<group>) at <volume>`.

## Tests: `Automation RunTests Startech`, 43/43 pass

New in this phase:

| Test | Checks |
|---|---|
| `Startech.Audio.SavedVolumesAreAppliedAfterARestart` | The engine's user settings are `UStartechUserSettings`. Volumes set through the audio subsystem and saved are read back by a fresh settings object reloading `GameUserSettings.ini` from disk, and are what `LoadSavedVolumes()` (the game's start-up value) returns. Clearing them gives the defaults. The machine's previous state is restored at the end. |
| `Startech.Audio.OnlyTheMainMenuShowsTheMenuMusicSlider` | The real WBP_MainMenu and WBP_PauseMenu, created with a local player so their pages are added. Both have a Settings button and page. Main menu: "Master, Music, Menu music, Weapons, Explosions". Pause menu: "Master, Music, Weapons, Explosions". |
| `Startech.Audio.ResetRestoresTheDefaults` | The pause menu's page with every slider at 13%: Reset puts each back to its default ("100%", "70%", "80%", "90%"), and the audio subsystem holds the defaults after `SetVolumes(Defaults)`. |

**Two test-side fixes on the way:**

- Widgets run `NativeOnInitialized`, where the menus add their pages, only with a valid player context
  (`UserWidget.cpp:173`), so the tests give the test world a player controller with a local player.
- A float tolerance: 0.8 snaps to 0.80000001.

**One build note:** two new test files were at first not compiled, because UBT's cached makefile missed them (the
known issue in my notes). Deleting `Makefile.bin` fixed it.

## Real check (windowed `-game`, 1600×900, the headset)

### Run A: main menu (`Saved/Logs/SettingsA.log`)

1. **Home** shows Settings next to Controls ([main_menu_home.png](Audio1/main_menu_home.png)).
2. **A real click on Settings** opens the page with 100 / 70 / 70 / 80 / 90 %, the Menu music row included.
3. **Real drags:** Music to 40%, Menu music to 30%, Weapons to 50%
   ([main_menu_settings.png](Audio1/main_menu_settings.png)).
   - The menu music is heard changing live: `au.DumpActiveSounds` shows `Infinite_Leitmotif … 0.3` straight after the
     drag.
4. **Real Esc** closes the page and saves: `[Audio] volumes saved: master 1.00 music 0.40 menu 0.30 weapons 0.50`. Home
   is shown again.

### Run B: restart, Reset, new game, pause menu (`Saved/Logs/SettingsB.log`; run with `VisibleSaveSlots=4` so test slot 3 can be used)

1. **Applied after the restart:** before any page opens, `[Audio] volumes in L_MainMenu: master 1.00 music 0.40 menu
   0.30 weapons 0.50 explosions 0.90`. The page then shows 40 / 30 / 50 %, and `saved:` matches.
2. **The Explosions preview:** dragging Explosions to 60% logs `[Audio] preview SW_Explosion (Explosions) at 0.60`, and
   the dump shows `SW_Explosion … 0.6` playing.
3. **Reset to defaults** (a real click): 100 / 70 / 70 / 80 / 90 % in use at once. **Back** (a real click) saves them.
4. **Menu → new game** (`Startech.Menu.NewGame 3 AudioCheck`, test slot 3):
   - The menu music fades out across the load: `FadingOut playlist=DA_Music_Universe fade=0.48 pending=DA_Music_Earth`.
   - Then `Playing playlist=DA_Music_Earth … class=SC_Music`.
5. **Pause menu:** undock, then a real Esc.
   - A real click on Settings: `menu=pause menu page=OPEN paused=yes`, with `Menu music 70% (hidden)`.
   - Drags: Master to 80% (heard live), Weapons to 60% (`[Audio] preview SW_Laser_Start (Weapons) at 0.60`)
     ([pause_menu_settings.png](Audio1/pause_menu_settings.png), four rows).
6. **Real Esc** saves (`volumes saved: master 0.80 … weapons 0.60`) and returns to the pause menu: `page=closed
   paused=yes`, `[Controls] menu=pause menu` ([pause_menu_after_escape.png](Audio1/pause_menu_after_escape.png)).
7. **Settings again:** Reset, then Back, gives `paused=yes`. A final Esc resumes: no menu, the viewport has input, the
   cursor is off, and the music plays on.

### Run C: a preview heard while paused (`Saved/Logs/SettingsC.log`)

In the paused pause menu, dragging Explosions to 70% logs the preview, and the dump straight after shows `SW_Explosion
(3.2) … 0.7` playing.

In run B, the Weapons sample (0.25 s) had already ended by the time the dump ran, so this run used the longer
explosion to prove it.

**Leftover state:** every run ended with Reset, so this machine is left on the default volumes, saved
(`bHasSavedVolumes=True`). Test slot 3 now holds a fresh "AudioCheck" new game.

## Notes

- **Which volume is heard for the preview:** the volume in the dumps is the group's volume (Master × group). For
  example, Explosions at 0.6 is heard at 0.6 because Master is 1.
- **Slider labels** are C++ LOCTEXT, like the other menus' buttons.
- **Adding a slider** is described in CLAUDE.md: a value in `FStartechVolumes`, an `EStartechVolume` entry, an
  adjuster line, a row in the page's `Order`/`Label`, and a default. Adding another page next to Settings and Controls
  is described there too.

## Files

New:

- `Source/Startech/Public/Audio/StartechUserSettings.h`, `Private/Audio/StartechUserSettings.cpp`
- `Source/Startech/Public/UI/Menu/MenuPageWidget.h`, `Private/UI/Menu/MenuPageWidget.cpp`
- `Source/Startech/Public/UI/Hub/HubSliderRowWidget.h`, `Private/UI/Hub/HubSliderRowWidget.cpp`
- `Source/Startech/Public/UI/Settings/SettingsScreenWidget.h`, `Private/UI/Settings/SettingsScreenWidget.cpp`
- `Source/Startech/Private/UI/Settings/SettingsConsoleCommands.cpp`
- `Docs/Tasks/Reports/Audio1/*.png` (4 screenshots)

Changed:

- `Audio/StartechAudioTypes.h`: `EStartechVolume`, `FStartechVolumes::Get`/`Set`/`==`, the preview sounds.
- `Audio/StartechAudioSubsystem.h/.cpp`: load, save, preview.
- `UI/Controls/ControlsScreenWidget.h/.cpp`: now a `UMenuPageWidget`.
- `UI/Menu/MainMenuWidget`, `MainMenuHomePanel`, `UI/Pause/PauseMenuWidget`, `PauseMenuPanel` (`.h/.cpp`): the
  Settings buttons and generic pages.
- `Private/Tests/StartechAudioTests.cpp`: 3 new tests.
- `Tools/Audio/setup_audio.py`: peak trim. `Content/Audio/SFX/SW_Laser_Hit.uasset` was re-made.
- `Config/DefaultGame.ini` (preview sounds), `Config/DefaultEngine.ini` (`GameUserSettingsClassName`).
- `CLAUDE.md`: Audio covers the saved volumes, the Settings page, menu pages, slider rows, adding a slider, and
  `Startech.Test.Settings`. Controls now points to the shared page pattern.

TASK_Audio_1 is complete. Stopping here.
