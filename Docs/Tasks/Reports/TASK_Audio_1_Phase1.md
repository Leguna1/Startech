# TASK_Audio_1: Phase 1 report (music and the sound setup)

Phase 1 is done. The game now has:

- sound groups and volume mixes
- per-sector and main-menu music, played by our own playlist player with sequential fades
- trimmed laser start and hit sounds, one start sound per volley
- the death explosion
- the hub's SFX fade
- the charge-up, the drainer's beam loop and the warp sound

Everything is set from data, made by one idempotent script. The editor build has no warnings. The 5 new tests pass,
and the whole `Startech` suite passes (40/40). A windowed play check confirmed the music, the volumes and the
concurrency on the real audio device.

**Your Phase 0 answers, as applied:**

| Answer | Done as |
|---|---|
| A Startech list keyed by sector id; no plugin change | `UStartechSettings::Audio.SectorMusic` |
| Playlists | Menu → Universe, Terra → Earth, Rust → Mars, Glacier → **Neptune**, Jove → Jupiter. A random take, then another when it ends, by our own player. |
| Trimmed copies of the laser start and hit | 0.25 s each, 0.08 s fade-out, leading silence cut; the originals are untouched |
| One start sound per volley; travel empty | as asked |
| Death explosion | `explosion-sound-effect-meme` |
| Warp fades | full fade-out, then fade-in |
| Hub | the SFX group fades to silent while it's open |
| Charge-up, beam loop, warp | wired, as data |

## What was built

### Assets: `Tools/Audio/setup_audio.py`

The script has steps `classes`, `mixes`, `attenuation`, `concurrency`, `sfx`, `music` and `ammo`; it is idempotent
(rerun verified) and reports with `[Audio]`.

| Asset | What |
|---|---|
| `/Game/Audio/Classes/SC_*` | `SC_Master` > `SC_Music`, `SC_MenuMusic`, `SC_SFX` > `SC_Weapons`, `SC_Explosions` |
| `/Game/Audio/Mixes/SMix_Volumes` | one adjuster per group. Master's applies to its children (the engine multiplies adjusters down the tree, `AudioDevice.cpp:3021-3060`). |
| `/Game/Audio/Mixes/SMix_Hub` | SFX and its children to 0, 0.5 s fade in and out |
| `/Game/Audio/Attenuation/ATT_Combat`, `ATT_Explosion` | linear sphere: full to 30 m, silent at 300 m; 50 m / 500 m |
| `/Game/Audio/Concurrency/CON_*` | the oldest stops first: LaserShot 8, LaserHit 8, Explosion 4, Beam 4, Charge 4, Warp 1 |
| `/Game/Audio/SFX/SW_Laser_Start` | trimmed from `laser-fire`: 0.005 s of silence skipped, 0.25 s kept. Weapons, ATT_Combat, CON_LaserShot. |
| `/Game/Audio/SFX/SW_Laser_Hit` | trimmed from `direct-hit-sound-anime`: 0.106 s skipped, 0.25 s kept. Weapons, ATT_Combat, CON_LaserHit. |
| `/Game/Audio/SFX/SW_Explosion` | a copy of `explosion-sound-effect-meme` (3.2 s). Explosions, ATT_Explosion, CON_Explosion. |
| `/Game/Audio/SFX/SW_Beam_Loop` | a copy of `laser-loop` (12 s), **looping**. Weapons, ATT_Combat, CON_Beam. |
| `/Game/Audio/SFX/SW_ChargeUp` | a copy of `chargeup` (0.59 s). Weapons, ATT_Combat, CON_Charge. |
| `/Game/Audio/SFX/SW_Warp` | a copy of `laser-sfx-transition` (1.65 s). SFX, non-spatial, CON_Warp. |
| `/Game/Audio/Music/DA_Music_{Universe,Earth,Mars,Neptune,Jupiter}` | `UMusicPlaylist`: the 10 takes in that pack folder's `Waves`. The Gaia waves are loaded by their object path. |
| Ammo | LL1–LL4: start `SW_Laser_Start`, no travel, hit `SW_Laser_Hit`. SD1–SD2: travel (the beam) `SW_Beam_Loop`. |

**How the trim works:** the script exports the source wave to WAV, cuts it with Python's `wave` module, and imports
the result as a new asset. The originals in `/Game/Sound` are never changed (git shows no change there).

### Config

- **`DefaultGame.ini`:**
  - `Audio=(...)`: playlists, groups, mixes, fades 2 s / 2 s, default volumes 100 / 70 / 70 / 80 / 90 %, the player's
    start volume 0.7, a volley window of 0.05 s, the warp sound.
  - `Death.DefaultEffects.DeathSound` = SW_Explosion.
  - `ChargedShot.ChargeSound` = SW_ChargeUp.
- **`DefaultEngine.ini`:** `[/Script/Engine.AudioSettings] DefaultSoundClassName=SC_Master`, so sounds with no class
  (for example a future `LockTone`) follow the Master volume.

### C++ (`Source/Startech/*/Audio/`)

**`UMusicPlaylist`:** the takes of one playlist.

**`FStartechAudioSettings` / `FStartechVolumes` / `FSectorMusic`:** the `UStartechSettings::Audio` section.

**`UStartechMusicSubsystem`** (game instance): the player.

- **Playlists:** a random take, loaded asynchronously, then another random take when it ends, never the same one twice
  in a row. A single-take playlist loops it.
- **How it plays:** a persistent, non-spatial UI sound, so it survives map travel and plays while paused, in the
  playlist's class (`SoundClassOverride`).
- **Fades:** driven by the subsystem on the core ticker, so they are deterministic and testable. A change is a full
  fade-out, then the fade-in. A sector sharing the playing playlist never restarts it, and a Terra → Terra respawn
  keeps playing.
- **Hooks:**
  - the menu game mode's `StartPlay`
  - the pawn's warp start (the fade begins with the screen fade)
  - the director's `OnSectorTransitionStarted` / `OnSectorReady`, bound in every game world through
    `FWorldDelegates::OnWorldInitializedActors`
  - `UStartechGameFlowSubsystem::OpenGameMap` / `QuitToMainMenu` (fade out across the load)

**`UStartechAudioSubsystem`** (game instance):

- the volumes, through `SMix_Volumes`. The mix is popped and pushed again in every game world, so it never stacks.
- the hub mix, pushed and popped from `AStartechShipPawn::OpenHub`/`CloseHub`
- the warp sound, from the pawn's warp start
- `GetAdjusterVolume`, which maps a group to its slider; SFX and future sliderless groups get 1

Phase 2's Settings page only has to call `SetVolumes`.

**`UWeaponEffectsComponent`:**

- `PlayStartSound` plays one start sound per volley: the same sound within `VolleyWindowSeconds` is skipped.
- The locally controlled player's start sound is non-spatial (`PlaySound2D` at `PlayerLaserStartVolume`), so it
  doesn't sound distant from the chase camera. Enemies' play at the mount.
- The beam's own start sound (`HandleBeamStarted`) is unchanged.

**Debug (non-shipping):**

- `Startech.Test.Audio`: the music, the volumes, the hub mix, and the player's start-sound count.
- `Startech.Test.MusicNext`: ends the playing take.

## Tests: `Automation RunTests Startech.Audio`, 5/5

| Test | Checks |
|---|---|
| `MenuAndEverySectorPlayItsMusic` | The menu plays Universe and fades in. A new game fades the menu music out completely before Terra's (Earth) starts. Warping Terra → Rust → Glacier → Jove → Glacier → Rust → Terra plays Mars, Neptune, Jupiter, …, each after a full fade-out, with no take started during the fade and exactly one new take per sector. Quitting to the menu brings Universe back. |
| `SharedPlaylistIsNotRestarted` | Two sectors sharing a playlist: no fade-out between them, no new take, the same take plays on. A same-sector respawn doesn't restart either. A different playlist fades out and in; an unlisted sector is silent. |
| `PlaylistPlaysAnotherTakeWhenOneEnds` | 20 take endings in a row never repeat a take and stay at full volume. A one-take playlist restarts its take. A take ending during a fade-out starts nothing. |
| `SoundsUseTheirGroupsAndLimits` | Real assets: the class tree, `SMix_Volumes` (every group, children only for Master) and `SMix_Hub` (SFX 0, children), the default volumes. Every laser ammo, the player's and the three enemy types': start and hit in **SC_Weapons**, **max 8, oldest stops**, spatial, at most 0.3 s long, no travel sound. The drainer's beam loops (Weapons, max 4). The death sound: **SC_Explosions, max 4**. The charge-up: Weapons, 4. The warp: SFX, 1, non-spatial. Every sector of the chain and the menu have a playlist with takes. |
| `OneStartSoundPerVolley` | The real player ship: both mounts firing in the same frame play **one** start sound; the next volley (after the 4/s cooldown) plays its own. |

The full suite passes, `Automation RunTests Startech`: 40/40. All tests run with `-culture=en-US`, so the engine's
Swedish-locale smoke-test lines don't appear (see CLAUDE.md, "Engine log noise").

## Real check (windowed `-game`, the headset as the audio device)

**Main sequence** (`/Game/StarterMap` on the test slot; log `Saved/Logs/AudioCheck.log`):

| Moment | `Startech.Test.Audio` / `au.DumpActiveSounds` |
|---|---|
| Docked in Terra | `music Playing playlist=DA_Music_Earth take=Circles_of_Earth__1_ heard=yes class=SC_Music`. Volumes 1.00 / 0.70 / 0.70 / 0.80 / 0.90 applied in StarterMap. |
| Hub open | `hub mix ON`; the music still `Playing … heard=yes`. On close: `hub mix popped`, `off`. |
| Warp to Rust, 1 s in | `FadingOut playlist=DA_Music_Earth fade=0.37 … pending=DA_Music_Mars` |
| 5 s later | `Playing playlist=DA_Music_Mars take=Red_Dust_Horizon__1_` |
| Glacier, Jove | `DA_Music_Neptune` (Neptune_Drift), then `DA_Music_Jupiter` (Jupiter_Rise__1_) |
| Fight in Rust (auto-pilot, 25 s) | `Active Sound Count: 8`: the music at **0.7** (Master × Music), 3 × SW_Laser_Start at **0.56** (the player's: 0.7 × Weapons 0.8) and 0.723, and 4 × SW_Laser_Hit at 0.74–0.8. That is well under the limits. The player's start sounds counted 85 by the end. |
| `Startech.Test.MusicNext` | a new Mars take (`Red_Dust_Lullaby__1_`) |
| `Startech.Menu.QuitToMainMenu` | `FadingOut … DA_Music_Mars fade=0.43 pending=DA_Music_Universe`, then in L_MainMenu `Playing playlist=DA_Music_Universe class=SC_MenuMusic`. The volumes were pushed again in L_MainMenu. |

**During the fight:** the pilot died once. The respawn's sector change fired, and the pilot warped back to Rust
within the 2 s fade, so Mars finished fading out and started a new take rather than Earth playing. That is the
expected result of "fade out fully, then fade in".

**Pause** (`Saved/Logs/AudioPause.log`): with the pause menu up (`[Controls] menu=pause menu`), the music stays
`Playing … heard=yes` for 3 s and more, and it is the only active sound (`Global_Motif … 0.7`).

**First play of a take:** in the editor, the first time a take plays it is compressed (`BINKA … compressed to
12.8%`, about 2 s). It's a one-time editor cost; cooked builds ship compressed. That's why a take starts a moment
after the fade-in begins the first time.

## Notes and limits

- **I can't listen.** The checks above prove what plays, in which group and at what volume, but not how it sounds.
  Please listen to:
  - **`SW_Laser_Hit`:** the first 0.25 s of a 3.6 s "anime hit". It may be the wind-up rather than the impact. If so,
    the script's `trim_wav` can take an offset.
  - **`SW_Laser_Start`:** the first 0.25 s of `laser-fire`.
  - **The volumes:** the 30 m / 300 m attenuation, and the player's start at 0.7.
- **Menu → game:** this was not run as a play check, because `Startech.Menu.NewGame` / `Load` would write to one of
  your real save slots. The unit test covers the logic: the menu music fades out on `OpenGameMap`, Earth follows on
  Terra ready. The code path is the same one the quit-to-menu run exercised in reverse.
- **The warp fade:** it begins at the pawn's warp start, so it is a full 2 s fade-out then a 2 s fade-in. The new
  sector's music is therefore audible about 3–4 s after the warp starts, after a short quiet moment on arrival, as you
  chose.
- **The beam loop:** it plays at the mount and loops while the drainer beam is on (the existing `HandleBeamStarted`
  path). Its own start sound stays empty.
- **No travel sound:** `laser-shooting` is no longer referenced by the ammo; the file is kept.

## Files

New:

- `Source/Startech/Public/Audio/{MusicPlaylist,StartechAudioTypes,StartechMusicSubsystem,StartechAudioSubsystem}.h`
- `Source/Startech/Private/Audio/{StartechMusicSubsystem,StartechAudioSubsystem,AudioConsoleCommands}.cpp`
  (`LogStartechAudio` is in `StartechAudioSubsystem.cpp`)
- `Source/Startech/Private/Tests/StartechAudioTests.cpp`
- `Tools/Audio/setup_audio.py`
- `Content/Audio/**` (6 classes, 2 mixes, 2 attenuation, 6 concurrency, 6 sounds, 5 playlists)

Changed:

- `StartechSettings.h` (`Audio`)
- `WeaponEffectsComponent.h/.cpp` (volley, the player's non-spatial start)
- `StartechShipPawn.cpp` (hub mix, warp sound and fade)
- `Flow/StartechMenuGameMode.cpp` (menu music)
- `Flow/StartechGameFlowSubsystem.cpp` (fade out on travel)
- `Config/DefaultGame.ini`, `Config/DefaultEngine.ini`
- the six ammo assets
- `CLAUDE.md` (new "Audio" section: the groups, where sounds are set, how to add a sound and a group)

Phase 2 (the Settings page) builds on `UStartechAudioSubsystem::SetVolumes` / `GetVolumes`. Stopping here until you
say go.
