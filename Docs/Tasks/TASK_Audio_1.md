# Task: audio, part 1 (music, existing sound effects, volume settings)

Read Startech's root `CLAUDE.md` first. Reuse `DA_HudStyle`, `UHubButtonWidget`, the main menu and pause menu code, the Controls screen's pattern (a full-screen page over each menu), the environment presets and the death sequence's `DeathSound` hook. Don't change the plugins; if a sector preset field needs adding in ProcSector, say so in Phase 0 first.

## Goal

The game is silent. This task adds:
- background music: one looping track per sector, and one for the main menu
- the sound effects that exist so far: laser shot start, travel and hit, and the death explosion
- a **Settings** page with volume sliders, in both the pause menu and the main menu

More sounds come later (boost, UI clicks, pickups, warnings, the charged shot). Build the structure so adding them is only data.

## Decisions already made

**Music:**
- Each sector has its own looping music track, set in its environment preset (`DA_EnvironmentPreset_*`) as a soft reference. Empty means silence.
- The main menu has its own looping track, set in the project settings (or wherever the main menu's config lives).
- **Warping:** the current sector's music fades out during the warp and the new sector's fades in on arrival (about 2 s each, in the settings). If both sectors use the same track, it keeps playing without restarting.
- **Leaving the main menu** fades the menu music out; quitting to the main menu fades the sector music out and the menu music in.
- Music keeps playing while the game is paused and while the hub is open.
- Music continues through the player's death and respawn (Terra's music after the respawn).

**Sound effects:**
- **Laser shot:** start (at the firing mount), travel (follows the visible shot, if there is one; if shots are instant traces, play it along the shot's path or skip it and tell me), hit (at the impact point). For the player and for enemies, using the same sounds unless a weapon overrides them.
- **Death explosion:** fill the existing `DeathSound` default with the explosion sound.
- **3D sound:** effects play in the world, with an attenuation that suits the fight distances (fights happen at about 30–100 m; sounds should fade out well before the 4 km sector edge). The player's own laser start can be slightly louder or non-spatial, so it doesn't sound distant from a chase camera.
- **Concurrency limits** so a big fight can't stack dozens of laser sounds: a small maximum per sound (about 6–8 laser shots, 4 explosions), oldest or quietest stops first.
- Effects pause with the game; the pause menu and hub are silent apart from music.
- Sound assets are referenced from data (the weapon data assets, the settings), never hard-coded paths in C++.

**Sound groups (Sound Classes) and volumes:**
- `Master`, with children: `Music` (sector music), `MenuMusic`, `SFX`, and under `SFX`: `Weapons` (lasers) and `Explosions`. New sound groups later go under `SFX` or `Master`.
- Volumes are applied through a Sound Mix with class adjusters, set from the settings.

**Settings page:**
- One `Settings` page, shown full screen over the pause menu and the main menu, like the Controls screen. Back and Esc return to where it was opened from.
- A **Settings** button in the pause menu and in the main menu.
- **Sliders in the pause menu:** Master, Music, Weapons, Explosions.
- **Sliders in the main menu:** the same four, plus **Menu music**.
- Each slider goes from 0 to 100% with the value shown as a number, styled from `DA_HudStyle` (a new reusable slider row widget: label, slider, value).
- **Live preview:** changing Music or Menu music is heard immediately in the playing track; letting go of the Weapons or Explosions slider plays one sample of that sound at the new volume.
- **Saving:** volumes are per machine, not per save slot. Store them in the game's user settings (a `UGameUserSettings` subclass or the existing settings pattern), saved when the page closes and applied at startup, before the main menu music starts.
- A **Reset to defaults** button. Defaults: Master 100%, Music 70%, Menu music 70%, Weapons 80%, Explosions 90% (in the settings, so they're easy to change).

## Phase 0: discover, report, then stop

Report, with file and line references:
1. **The sound files the user has imported:** their names and paths, length, mono or stereo, and whether looping music is set to loop. List anything that looks like it's meant for this task but isn't covered above.
2. **How lasers fire and are shown:** where a shot starts, whether the "travel" is a visible bolt or effect with a lifetime or an instant trace, and where hits are detected, for both the player and enemies.
3. **Existing audio setup:** any Sound Classes, Sound Mixes, attenuation or concurrency assets, and any audio settings in the project.
4. **Sectors and warping:** where the environment preset is applied on warp, and the right moment to fade music out and in. How the main menu level starts and ends.
5. **Settings storage:** whether the project already has a `UGameUserSettings` subclass or another place for per-machine settings.
6. **Menus:** how a Settings button and page would be added next to the Controls button in both menus.
7. **Your plan,** including anything in the decisions above that doesn't fit the existing code.

Then stop.

## Phase 1: music and the sound setup

- Sound Classes, the Sound Mix, attenuation and concurrency assets (by script).
- Per-sector music in the presets and the main menu track, with the fades on warp and between menu and game. Assign the user's tracks to the sectors and the menu as they're named; if it's unclear which track goes where, list them in the report and leave them unassigned.
- Laser start, travel and hit sounds, and the explosion sound in `DeathSound`.
- Tests: the right track plays in the main menu and after warping to each sector; the same track isn't restarted between sectors that share it; laser and explosion sounds use the right Sound Class and concurrency.

## Phase 2: the Settings page

- The slider row widget, the Settings page, the buttons in both menus, the live preview, saving and Reset to defaults.
- Tests: a changed volume is saved and applied after a restart; the main menu shows the Menu music slider and the pause menu doesn't; Reset restores the defaults.
- A real check: open the page from both menus, move each slider, take a screenshot of each page, and confirm Back and Esc return correctly and the game stays paused.

## Done when

- every sector and the main menu play their own looping track, with smooth fades between them
- lasers and explosions are heard in fights, without stacking into noise
- both menus have a Settings page with working, saved volume sliders
- `CLAUDE.md` explains the sound groups, where sounds are set, and how to add a new sound and a new slider
