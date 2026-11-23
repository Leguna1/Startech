# TASK_Audio_1: Phase 0 report

Discovery only; nothing in the project was changed. Asset facts come from headless editor-Python dumps (durations,
channels, looping, classes, references, cue graphs), not from file names.

**The short version:**

- **Laser sounds already work.** Lasers already play a start and a travel sound through C++ that exists. Laser ammo
  references `laser-fire` and `laser-shooting`, and enemies use the same ammo.
- **Nothing else is set up:** no hit sound, no death sound, no Sound Classes, no mix, no attenuation, no concurrency,
  no music and no volume settings.
- **The laser sounds probably stack.** `laser-fire` is 2.97 s long and each mount fires 4 shots a second, so one ship
  stacks about 24 copies. It probably sounds like noise or a smear rather than silence.
- **The spec has three conflicts with the existing code:**
  - Music in the environment preset needs a ProcSector change.
  - The hub doesn't pause the game.
  - A sector change takes far less time than a 2 s fade.

---

## 1. The imported sound files

### Effects: `/Game/Sound` (10 waves)

None of these has a Sound Class, attenuation or concurrency set.

| Asset | Length | Channels | Rate | Used by | Looks meant for |
|---|---|---|---|---|---|
| `laser-fire` | 2.97 s | **mono** | 44.1 k | `StartSound` of DA_1xAmmo, DA_2x_Ammo, DA_x3_Ammo, DA_x4_Ammo | laser start (long for 4 shots/s, see §2) |
| `laser-shooting` | 0.86 s | stereo | 44.1 k | `TravelSound` of the same four | laser travel, or a shorter laser start |
| `laser_S5c8iCH` | 0.77 s | stereo | 48 k | nothing | an alternative laser shot (enemy shot, or the charged shot's `FireSound`) |
| `direct-hit-sound-anime` | 3.60 s | stereo | 44.1 k | nothing | laser **hit** (long for a hit) |
| `explosion-sound-effect-meme` | 3.20 s | stereo | 44.1 k | nothing | **death explosion** |
| `barrel-exploding-soundbible` | 4.11 s | stereo | 44.1 k | nothing | **death explosion** (the other candidate) |
| `chargeup` | 0.59 s | stereo | 44.1 k | nothing | the charged shot's `ChargeSound` (not in this task) |
| `laser-loop` | 12.04 s | stereo | 44.1 k | nothing | the shield-drain beam's loop (`TravelSound` of DA_1x/2xDrainShield; not in this task) |
| `laser-beam_uJB0vF3` | 14.78 s | stereo | **24 k** | nothing | the same, an alternative |
| `laser-sfx-transition` | 1.65 s | stereo | 44.1 k | nothing | perhaps the warp (not in this task) |

Notes:

- **Stereo files in the world:** they still play spatially, with stereo spread, but mono places them better. Only
  `laser-fire` is mono.
- **Sounds not in the spec:** `chargeup`, `laser-loop` / `laser-beam` and `laser-sfx-transition` look meant for the
  charged shot, the drainer beam and the warp. The C++ hooks for the first two already exist (`FChargedShotEffects`
  `ChargeSound` / `FireSound` / `ImpactSound`, and `UAmmoData::TravelSound` for the beam). Wiring them would be data
  only; see question 6.

### Music: `/Game/Harmony_of_the_World_-_Universe` (140 waves, all stereo, 2–4 min each, **none set to loop**)

**Folders:**

- Black_Hole, Nebula, Neutron_Star and the pack root (`Waves/`): 10 each.
- Solar_System: 100, in ten folders of 10: Earth, Jupiter, Mars, Mercury, Neptune, Saturn, Sun, Uranus, Venus, and
  its own `Waves/`.

**Takes:** every track has one or more `__1_`, `__2_` … files. They are **different takes** of different lengths (for
example `Blue_Marble_Loop` 147.6 s, `Blue_Marble_Loop__1_` 91.3 s), not copies.

**Cues:** the pack ships one cue per track (`<Track>_Cue`, a single wave player, no loop). Each folder also has a
playlist cue:

| Playlist cue | Graph | Loops? |
|---|---|---|
| `Cues/Universe` (pack root), `Black_Hole/Cues/BlackHole`, `Nebula/Cues/Nebula` | Looping → Random over its 10 waves | **yes** (duration reported as 10000 s = loops forever) |
| `Solar_System/{Earth,Mars,Saturn,Sun,Uranus,Venus}/Cues/<Name>` | same | **yes** |
| `Solar_System/{Jupiter,Mercury,Neptune}/Cues/<Name>`, `Neutron_Star/Cues/NeutronStar`, `Solar_System/Cues/SolarSystem` | Random, **no Looping node** | no: plays one random take, then stops |

**Name mismatch:** `Solar_System/Earth/Waves/Gaias_Quiet_Loop(.uasset)` and `__1_` load only by their inner object
name, `Gaia’s_Quiet_Loop` (a curly apostrophe). The Earth cue references them correctly. A script that loads them by
package name fails, as mine first did, but nothing is broken. Renaming them in the editor would tidy it; that's not
needed.

## 2. How lasers fire and are shown

**Start:** `ULaserMountComponent::Fire` (`Private/LaserMountComponent.cpp:86-120`).

- It decides the hit at once with a sphere trace (`TraceShot`, `:93`).
- It records a shot that starts at the mount's location (`:94`).
- It broadcasts `OnShotFired(Mount, ShotId, Start, Ammo)` (`:118`).

**Travel:** a visible bolt.

- The mount moves the shot towards its goal at `BoltSpeed` in `TickComponent` (`:122-161`) and broadcasts
  `OnShotMoved` every frame.
- The goal is the hit point, which follows the hit actor (`GetShotGoal`, `:183`).
- Every laser weapon asset has `BoltSpeed` 40,000 cm/s and `FireRate` 4/s (`DA_BasicWeapon` and the three enemy
  variants). So **a bolt lives 0.075–0.25 s at 30–100 m**, up to 0.25 s at the player's 100 m range.

**Hit:** on arrival, `ResolveShot` (`:164-181`) applies the damage and broadcasts
`OnShotEnded(Mount, ShotId, Location, bImpact)`.

**Who plays the sounds:** `UWeaponEffectsComponent` (subobject `WeaponEffects` on every ship, `StartechShipBase.cpp:40`;
also present in BP_ShipPawn and the three enemy Blueprints). It plays everything from the ammo's data
(`UAmmoData` `StartSound` / `TravelSound` / `HitSound`, `Public/AmmoData.h:57-66`).

- **Start:** `PlaySoundAtLocation` at the mount (`WeaponEffectsComponent.cpp:212`).
- **Travel:** attached to the bolt's Niagara component, or moved by hand when there's no bolt (`:234-239`, `:261-264`).
  It is stopped when the shot ends (`EndBolt`, `:290`).
- **Hit:** `PlaySoundAtLocation` at the impact, only when `bImpact` (`:285`).

**Player and enemies are handled the same way.** Enemies fire `UEnemyData::LaserAmmo`, which is `DA_1xAmmo` for all
three types (`StartechEnemyShip.cpp:194-201`). They get the same sounds today.

**Weapon overrides:** sounds sit on the **ammo**, not the weapon (`ULaserWeaponData` has only Damage, FireRate,
BoltSpeed, Range). "Unless a weapon overrides them" would need new fields on `UWeaponData`. My suggestion: keep the
ammo as the place for laser sounds, which already gives per-ammo sounds, and give the enemies their own ammo asset
later if they should sound different.

**Consequences:**

- **Stacking:** at 4 shots/s per mount and 2 mounts per ship, `laser-fire` (2.97 s) stacks about 24 instances per
  firing ship, and `laser-shooting` is cut off after 0.1–0.25 s on every shot. With no concurrency limit, a fight with
  three enemies runs about a hundred laser voices.
- **Level and placement:** the sounds are non-spatial (no attenuation, so they play at full volume everywhere).
  They also use the engine's default sound class, so no volume control reaches them.

## 3. Existing audio setup

**Assets:** none. There is **no** Sound Class, Sound Mix, Sound Attenuation, Sound Concurrency, Submix or reverb
asset in `/Game`.

**Config:**

- `DefaultEngine.ini:55-75` has only the Windows platform defaults: 48 kHz, `AudioMaxChannels=0` (the engine default,
  32 voices), compression and cooking.
- There is no `[/Script/Engine.AudioSettings]` section, so no default Sound Class, default base mix or default
  concurrency. The game log says `No default SoundConcurrencyObject specified`.

**Device:** audio initialises and plays to the headset (`Using Audio Hardware Device Hörlurar på headset (DH440 …)`).

**Other sound hooks in C++, all empty in data:**

- `FShipDeathEffects::DeathSound` (`Public/Death/ShipDeathTypes.h:36`). It is played by
  `UShipDeathComponent` at the explosion (`ShipDeathComponent.cpp:307-309`).
  - `Death.DefaultEffects` in `DefaultGame.ini:41` sets only FX.
  - BP_ShipPawn's `DeathEffects.DeathSound` is None, so it falls back to the default.
- `FChargedShotEffects` `ChargeSound` / `FireSound` / `ImpactSound` (`Combat/ChargedShotTypes.h:52-73`).
- `UStartechHudStyle::LockTone` (`StartechHudStyle.h:219`, played 2D by `HudReticleWidget.cpp:154`): not set in
  DA_HudStyle.

**Per-machine settings:** none. There is no `UGameUserSettings` subclass, no `GameUserSettingsClassName`, and no
other per-machine store. `UStartechSettings` is a project `UDeveloperSettings`.

## 4. Sectors, warping, main menu

**Presets:** `UEnvironmentPreset` is a **ProcSector** class (`Plugins/ProcSector/.../Public/Data/EnvironmentPreset.h:14`)
with `BaseParams`, the jitter values and `bRandomizeSunAndSky`. **It has no audio field.**

- Each catalog sector (`FSectorDefinition`, `SectorTypes.h:585-600`) points to its preset.
- `Tools/Sectors/setup_tiers.py:228-249` recopies `BaseParams` and the two jitter values from Rust into Glacier and
  Jove on every run. A new **top-level** field on the preset would survive that; one inside `BaseParams` would be
  overwritten with Rust's.

**Warp timeline** (ProcSector):

1. `UWarpTransitionComponent::OnWarpStarted(Destination)` (`WarpTransitionComponent.cpp:147`), then a fade to black
   (`FadeOutSeconds` 0.6).
2. `HandleFadeOutFinished` asks the director for the sector, and `USectorDirectorSubsystem::OnSectorTransitionStarted(From, To)`
   fires (`SectorDirectorSubsystem.cpp:141`).
3. Generation runs, then `OnSectorReady(Id)` (`:389`).
4. The ship is moved and the screen fades in (`FadeInSeconds` 0.8). `OnWarpFinished(Id)` (`WarpTransitionComponent.cpp:232`).

Generation is quick: 16–46 ms of work per sector (`LogProcSector: Sector 'Rust' ready (... 41.87 ms of generation work)`).
**The whole warp is about 1.5 s, shorter than one 2 s fade.**

**Other sector changes go through the same director events:**

- The start of a game: `UStartechStartFlowSubsystem` requests the home sector, then `OnSectorReady`.
- A respawn: `UHomeRespawnSubsystem::TryReturnHome` (`HomeRespawnSubsystem.cpp:65`) requests Terra. If the death was
  in Terra, Terra is regenerated: a Terra → Terra transition with the same track, so the music keeps playing.

**Main menu:** `L_MainMenu` runs `AStartechMenuGameMode::StartPlay` (`Flow/StartechMenuGameMode.cpp:15-35`), which
shows the menu. That is the place for menu music to start.

- Leaving the menu: `UStartechGameFlowSubsystem::OpenGameMap` (`StartechGameFlowSubsystem.cpp:178-194`).
- Quitting to the menu: `QuitToMainMenu` (`:132-150`).
- Both call `OpenLevelBySoftObjectPtr` **at once**. A world's audio components die with the world, so a fade across
  the travel needs music that lives in the game instance. Sounds made with `CreateSound2D(...,
  bPersistAcrossLevelTransition = true)` keep playing through the load.

**Pause and hub:**

- The pause menu pauses the game (`StartechPauseMenuSubsystem.cpp:77`).
- **The hub does not.** Its input is UI-only, but the world runs. Non-UI world sounds pause with the game by
  themselves, so "effects pause with the game" already holds for the pause menu, but **not** for the hub.

## 5. Settings storage

Nothing exists. My plan is `UStartechUserSettings : UGameUserSettings`, registered with
`[/Script/Engine.Engine] GameUserSettingsClassName=/Script/Startech.StartechUserSettings` in `DefaultEngine.ini`.

- **Where it's saved:** `GameUserSettings.ini` in the user's `Saved/Config`, which is per machine and not per slot.
- **When it loads:** at engine start, before any map. It is read through `GEngine->GetGameUserSettings()`.
- **Fields:** `Config` floats for the five volumes. Defaults come from `UStartechSettings::Audio` (Master 1.0,
  Music 0.7, Menu music 0.7, Weapons 0.8, Explosions 0.9).

## 6. Menus: adding Settings next to Controls

It works exactly like the Controls screen, with no Blueprint step.

- **The button:** a Settings button in `UMainMenuHomePanel::BuildContent` (`MainMenuHomePanel.cpp:31-39`) and in
  `UPauseMenuPanel::BuildContent` (`PauseMenuPanel.cpp:58-61`), next to Controls. Each gets an `OnSettings` delegate,
  bound in `UMainMenuWidget` / `UPauseMenuWidget::NativeConstruct`.
- **The page:** `AddToMenu` on each menu's root canvas from `NativeOnInitialized`. It hides the menu's page or panel
  and restores it on close, and both menus send Esc to it first, as for Controls (`MainMenuWidget.cpp`,
  `PauseMenuWidget.cpp`).
- **Small refactor:** the menus now hold one full-screen page (Controls). With two, I'd move the shared part (open,
  close, `OnClosed`, Esc, `AddToMenu`) into a small base `UMenuPageWidget` that both pages derive from. Each menu would
  then track "the open page" instead of special-casing each screen.
- **The Menu music slider:** the page takes a flag from the menu that adds it (`AddToMenu(Menu, bShowMenuMusic)`), so
  the main menu shows it and the pause menu doesn't.

## 7. Plan

### Sound groups (by script, `Tools/Audio/setup_audio.py`, idempotent)

- **Sound Classes:** `SC_Master` → `SC_Music`, `SC_MenuMusic`, `SC_SFX` → `SC_Weapons`, `SC_Explosions`.
- **Mix:** `SMix_Volumes`, with one adjuster per class, pushed once at start-up and updated by the settings (Master's
  adjuster applies to children).
- **Defaults:** `[/Script/Engine.AudioSettings] DefaultSoundClassName=SC_Master`, so a sound without a class still
  follows Master.
- **Attenuation:**
  - `ATT_Combat`: full volume to about 30 m, falling to silence at about 300 m (fights are 30–100 m; the 4 km edge is
    far outside).
  - `ATT_Explosion`: a little wider.
- **Concurrency:** `CON_Laser` (max 8 per sound, stop oldest) and `CON_Explosion` (max 4, stop oldest).

**Applied to the effect waves:** class, attenuation and concurrency are set on the effect waves themselves by the
script, so the data keeps referencing plain sounds. Music waves are left untouched; the music player sets the class
on its own audio component (`SoundClassOverride`), so the same track could serve both Music and Menu music.

### Music (C++, `UStartechMusicSubsystem`, game instance)

- **The player:** it owns persistent 2D audio components (`bIsUISound`, so it plays while paused; persists across
  level travel) and fades between them.
- **Looping:** it restarts any sound that ends, so a single wave, a looping playlist cue or a non-looping one all loop.
- **Same track:** a sector change to the same `USoundBase` doesn't touch the track.
- **Hooks:**
  - menu music on `AStartechMenuGameMode::StartPlay`
  - fade-out on `OnWarpStarted` / `OnSectorTransitionStarted`
  - fade-in on `OnSectorReady`
  - fade-out of the menu music when the game map opens
- **Timing:** all fade times are in `UStartechSettings::Audio`.
- **Death:** the music continues through death (no hook), and the respawn's transition to Terra switches the track if
  needed.

### Effects

- **Laser:** keep `UWeaponEffectsComponent` as it is. Fill in the ammo assets' sounds with your choices
  (question 3); the script sets their class, attenuation and concurrency.
- **The player's own start sound:** played 2D (non-spatial) at a set volume, so it doesn't sound distant from the
  chase camera. This is a small branch in `HandleShotFired` for the locally controlled ship.
- **Death:** `Death.DefaultEffects.DeathSound` in `DefaultGame.ini` gets the chosen explosion.

### Settings page (Phase 2)

- **Slider:** `UHubSliderRowWidget`: label, a slider painted in C++ from DA_HudStyle (like `UHubButtonWidget`), and
  the value as a percentage. It works with mouse drag and arrow keys.
- **Page:** `USettingsScreenWidget` (a `UMenuPageWidget`), with Reset to defaults, Back and Esc.
- **Live preview:** Music and Menu music slide live. On release, the Weapons and Explosions sliders play one sample,
  2D and as a UI sound so it is heard while paused. The sample goes through its own class, so the new volume applies.
- **Saving:** on close.

## Questions before Phase 1

1. **Where sector music lives.** The spec says the environment preset, but that is a ProcSector class.
   - **A:** add a top-level `TSoftObjectPtr<USoundBase> Music` to `UEnvironmentPreset`. This is a plugin change: one
     field, safe from `setup_tiers.py`'s copying.
   - **B (my recommendation, no plugin change):** a Startech list in `UStartechSettings::Audio`, keyed by sector id
     like `SectorTiers`. The menu track would sit in the same place.

   Which one?
2. **Which tracks go where.** My suggestion, by name, using the pack's playlist cues (random takes, looped by the
   music player):
   - Menu → `Cues/Universe`
   - Terra → `Earth`
   - Rust → `Mars` ("Red Dust")
   - Jove → `Jupiter`
   - Glacier → `Neptune` or `Uranus`? (both are icy; it's unclear)

   Or would you rather have a single track per sector? Playlists give 10 takes each.
3. **Which effect files.**
   - **Laser start:** keep `laser-fire`, which is 2.97 s long and smears at 4 shots/s per mount (concurrency would
     cut the tails)? Or use a short one, `laser_S5c8iCH` (0.77 s) or `laser-shooting` (0.86 s)?
   - **Travel:** a bolt lives at most 0.25 s, so a travel sound is cut almost at once. Keep `laser-shooting` there,
     or leave travel empty? I'd leave it empty and use a short start sound.
   - **Hit:** `direct-hit-sound-anime` (3.6 s, long), or none for now?
   - **Death:** `explosion-sound-effect-meme` (3.2 s) or `barrel-exploding-soundbible` (4.1 s)?
4. **Warp fades.** The whole warp is about 1.5 s, shorter than one 2 s fade.
   - **(a) Sequential:** the old track fades out over 2 s from the warp start, and the new one fades in over 2 s once
     the old has finished, a short quiet gap after arrival.
   - **(b) Crossfade:** the new track starts fading in on arrival while the old one is still fading out.

   I'd pick (a).
5. **The hub isn't paused.** "The hub is silent apart from music" isn't true today, because the world keeps running.
   Should I fade the SFX class to 0 while the hub is open (one hub sound mix), or leave the hub's world sounds
   playing?
6. **The extra sounds** (`chargeup`, `laser-loop` / `laser-beam`, `laser-sfx-transition`): wire the obvious ones now
   as data (`chargeup` → charged-shot `ChargeSound`, `laser-loop` → the drainer beam), or leave them for the later
   audio task as the spec says?

Stopping here as asked.
