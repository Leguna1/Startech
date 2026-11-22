# Combat balance 2 (behaviour): Phase 2 report

Phase 2 covers the ranged enemy and its charged shot:
- **Holding range:** it backs off while firing when the player is at 36–90 m, and turns and flees without firing below 36 m.
- **Telegraph:** every 6–8 s it charges a shot for 1.2 s. The hull glows, a charge effect plays at the muzzle and the HUD warns the player.
- **The shot:** a slow, visible projectile aimed where the player's motion at the start of the telegraph would take them.

The normal ranged shot is halved to 0.7, and the charged shot carries the rest of the damage. All 29 `Startech` automation tests pass, 7 of them new in this phase. The build has no new warnings, and no plugin was changed.

**HUD bug found and fixed.** While checking the warning on screen I found an existing bug in shared HUD code. In a standalone game window, nothing projected from the world onto the HUD was drawn: not lock brackets, not gate markers, not the new warning. The details are under [The HUD projection fix](#the-hud-projection-fix) below.

Phase 0 answers applied:
- The shot aims where the player's velocity at the telegraph start would take them.
- The boost stays forward-only.
- Normal shot about 0.7, charged shot about 2.5 s of the old normal fire.
- Ranged retreat split at 36 m and 90 m.

## The ranged enemy

### Holding range and retreating (`EnemyBrainComponent.cpp`, `UpdateRetreat`, `:342`)
- **36–90 m** (40–100% of its preferred range): as before. It faces the player and reverses at 1,800 cm/s while firing and strafing.
- **Within 36 m** (`RetreatFraction` 0.4): it cancels any charge, turns its nose away and flies off at full forward speed (3,500 cm/s, faster than the player's normal 3,000) without firing. It turns back once it is at 90 m again.

Pushing in therefore buys a window with no fire coming back. Catching it before it gets away takes the boost.

### The charged shot

**When** (`UpdateCharge`, `:381`):
- The first charge comes 2–3 s into the fight, then every 6–8 s, measured from one charge's start to the next.
- Each charge needs the player in laser range (100 m) and in line of sight (`HasLineOfSight`, `:409`, a trace on the shot channel). Behind an asteroid it waits.
- Normal fire pauses while it charges.

**Telegraph** (`UEnemyChargeComponent`, `Combat/EnemyChargeComponent.cpp`; subobject `ChargedShot` on every enemy, used only by types with `bChargedShot`):
- `BeginCharge` (`:32`) records the target's position and velocity.
- **Hull glow:** for 1.2 s the hull's colours blend towards red with a strong emission boost, pulsing faster as the shot nears (3 → 9 Hz).
- **Muzzle effects:** the charge effect and sound play at the muzzle, between the laser mounts.
- **Shared flash code:** the death flash's material code is now a shared `FHullFlash` (`Death/HullFlash.h`), used by both the death flash and this glow.

**Aim** (`GetAimPoint`, `:113`): the target's position at the telegraph start, moved on by its velocity at that moment, for the telegraph plus the shot's flight time (refined three times).
- A player who keeps doing the same thing is hit.
- A player who changes anything during the 1.2 s telegraph plus the 0.4–0.7 s flight is missed: flip or stop the strafe, push in, pull back or boost.
- No homing, and no lead taken when it fires.

**Projectile** (`AChargedShotProjectile`, taken from and returned to the pool through `UChargedShotSubsystem`):
- 12,500 cm/s, so 0.72 s at the preferred 90 m.
- Each tick it sweeps a 150 cm sphere on the shot channel, so the player, an asteroid or anything else that blocks shots stops it. Wrecks ignore shots, so it passes through them.
- It damages anything not on the shooter's team, credited to the shooter, then ends. With no hit it ends after 2 s.
- Every projectile still flying is released when a sector transition starts.

**Damage:**
- 40 before the tier multiplier (52 in tier II). That is about 2.5 s of the ranged enemy's old normal fire: 1.4 × 2 mounts × 4/s × 2.5 s ≈ 28 shots ≈ 39 damage.
- The normal shot is halved from 1.4 to 0.7.

**Stopping and pooling:**
- `CancelCharge` runs on hull 0 (`HandleDeath`), when it gives up, when it retreats, on `StopBrain` and from the component's pool hooks. It ends the glow, the effect and the HUD warning without firing.
- A shot already in flight keeps flying, credited to its now-dead shooter.

### The HUD warning (`UHudScreenFeedbackWidget::DrawChargeWarnings`, `:324`; `UHudReticleWidget`)
`UChargedShotSubsystem` lists the charges in progress, and the HUD reads that list. Everything pulses with the charge's own rhythm (`GetPulse`), in time with the hull glow.
- **On screen** (the current target included): pulsing red corner brackets around the enemy, drawn outside the lock brackets.
- **The current target:** the reticle's own lock brackets also turn red and pulse.
- **Off screen:** the screen edge facing the enemy pulses red, through the same `M_HUD_EdgeFeedback` material as the shield-hit ripple.

### Data and effect hooks
- **Numbers per type:** `FEnemyBehaviourBalance` in `DA_CombatBalance`, set by the script's `BEHAVIOUR` dict.
  - `RetreatFraction`, `bChargedShot`
  - `ChargeFirstDelayMin/Max`, `ChargeIntervalMin/Max`, `ChargeTelegraphSeconds`
  - `ChargedShotDamage`, `ChargedShotSpeed`, `ChargedShotRadius`, `ChargedShotLifetime`
- **Look and sound:** `UStartechSettings::ChargedShot` (`FChargedShotEffects`): glow colour, strength, pulse and parameter names; `ChargeFX`, `ChargeSound`, `ProjectileFX`, `FireSound`, `ImpactFX`, `ImpactSound`. Every effect may be empty.
- **Placeholders in `DefaultGame.ini`:** `NS_Laser_Hit` for the charge (×2) and the impact (×2), and the laser bolt `NS_Laster_Bolt` (×3) for the projectile. They are preloaded with the death effects.

### Combat log
- Three native events on `UChargedShotSubsystem`: charge started, shot fired, shot ended (with what it hit).
- The combat log counts each engagement's charged shots fired and hit and their damage. That damage is also still counted as enemy hits.
- `Startech.Combat.Report` gains a "Charged hit/fired (dmg)" column, and the CSV gains `ChargedFired,ChargedHit,ChargedDamage`.

## The HUD projection fix
- **The symptom:** in standalone game windows, the HUD drew nothing that is placed from the world. No lock brackets on the target, no gate markers, and no new charge warning. I first saw it as missing red brackets (`CombatBalance2/before_projection_fix.png`: hard lock on, no brackets, no gate markers).
- **The diagnosis:** a temporary probe showed an enemy at viewport pixel (746, 385) coming out at (2,082, 1,083) in the HUD's space, when it should have been (1,120, 578) at the window's 1.5 DPI scale. The error was exactly the game window's position on the desktop × 1.5.
- **The cause:** `FHudDraw::ProjectToWidget` converted with `ScreenToWidgetLocal(…)`, which returns desktop coordinates, but paint geometry is relative to the window.
- **Why it went unnoticed:** a maximized editor sits at the desktop's corner, where the error is zero.
- **The fix:** pass `bIncludeWindowPosition = true`, the engine's option for windowed mode (`HudDraw.cpp:238`). All six callers use paint geometry (reticle, nav markers, the new warning).
- **Verified:** the same pixel now maps to (787, 581) for (524, 387), exactly ×1.5. Gate markers and lock brackets now show in every later screenshot.

## Automation tests (`Private/Tests/StartechBehaviourTests.cpp`)
Phase 2's tests use real `BP_Enemy_Ranged` and `BP_ShipPawn` in the hand-ticked game world.

| Test | Result |
|---|---|
| `TelegraphComesBeforeTheShotAndAStillPlayerIsHit` | Two charges: telegraph **1.200 s** before each shot (within 2 frames), a still player hit both times, charges ≥ 6 s apart. |
| `ChargedShotHitsAPlayerKeepingItsCourse` | A player crossing the line of fire at a steady 1,500 cm/s is hit: the aim leads it. |
| `ChargedShotMissesAPlayerWhoBoostsSideways` | A still player facing across the line of fire, who boosts once the telegraph starts (**4,460 cm/s** at the shot), is missed. |
| `ChargedShotIsStoppedByAnAsteroid` | A 30 m rock placed halfway after the telegraph started stops the shot; nothing is left flying. |
| `RangedDyingMidChargeNeverFires` | Killed halfway through the telegraph: the charge stops at hull 0, no shot in the next 3 s, no warning left listed. |
| `PooledRangedComesBackWithNoCharge` | Released mid-charge while glowing: reused with no charge and no tick, the hull's own materials back, calm, no warning listed, and the old charge never fires. |
| `RangedRetreatsFromAPlayerTooClose` | Player at 30 m: it retreats at once, never fires meanwhile, and stops retreating at 90 m after **3.6 s**, still engaged. |

The 15 Phase 1 and death tests still pass. That includes the death flash, now built on `FHullFlash`.

## In-game checks (Rust, tier II)
- **Rhythm:** 7 charges in 80 s against the test pilot holding 60 m. Each telegraph took 1.2 s, and shots arrived after 0.4–0.6 s at 66–100 m.
  - **4 hit and 3 missed.** The misses were shots that expired after 2 s, because the pilot's throttle corrections changed its motion during the telegraph. The pilot doesn't dodge on purpose yet; that is the Phase 3 option.
- **Combat report** (Lasers 2, Hull 1, Shield 1, LL2):
```
[CombatReport] Tier Enemy    Eng Kills Died  TTK on/total    Hit%   Taken/kill  Lost/kill  ...  EnemyShots/Hits  Charged hit/fired (dmg)
[CombatReport] 2    Ranged      7     5    1   7.89/  8.57 s   98.6%       73.8      21.1%  ...  246/244  4/7 (208)
```
  - Health lost per kill was 21.1% with about half the charged shots landing. In Part 1, with every shot hitting, it was 21–30%.
  - The pilot died on the 6th fight; hull doesn't repair in flight.
- **TTK table:** it doesn't count charged shots yet. The tier II ranged row now shows 16.8% health lost from normal fire alone (was about 33%). Phase 3 adds the two-value ranged row: everything hits, and every charged shot dodged.
- **Screenshots** (`Docs/Tasks/Reports/CombatBalance2/`):
  - `charge_warning_target.png`: 1.0 s into a telegraph against the locked target. Red pulsing brackets on it, and the gate marker visible.
  - `charge_warning_offscreen.png`: the player turned away (`Startech.Test.Turn 140`); the left and bottom edges glow red towards the charging enemy behind.
  - `charge_glow_closeup.png`: enlarged, the ranged hull tinted red with the charge sparks at its muzzle.

## Notes and proposals
1. **Dodging means changing motion, not a "sideways boost".** The boost stays forward-only. In the combat stance, forward is straight at the enemy, which moves along the line of fire and doesn't dodge. What dodges is any change of strafe or throttle during the 1.9 s. If you want the boost itself to be the dodge, it would need a sideways component. That would change the player's movement, so I'm only proposing it.
2. **The first charge at 2–3 s** (not 6–8 s) makes most fights see about two charges, so the charged shot matters in short fights. It is a number in the balance asset.
3. **Changes outside the task's own files:**
   - The death flash now uses the shared `FHullFlash`, with the same behaviour.
   - The death-effect preloader also preloads the charged-shot effects.
   - The HUD projection fix above.
4. **New test commands** (non-shipping):
   - `Startech.Test.ChargeShot [Seconds] [Count]`: screenshots that far into the next telegraphs.
   - `Startech.Test.Turn <Degrees>`: yaws the player ship.
   - `Startech.Test.Enemies` now also logs each enemy's bounds.
   - The charge logs `[Charge]` lines: charging, fired, what the shot hit.

## Files
- **New:**
  - `Public|Private/Combat/ChargedShotTypes.h`, `ChargedShotProjectile.*`, `ChargedShotSubsystem.*`, `EnemyChargeComponent.*`
  - `Public|Private/Death/HullFlash.*`
- **Changed:**
  - `Combat/CombatBalance.h`, `Combat/CombatTelemetrySubsystem.*`
  - `Death/ShipDeathComponent.*`, `Death/ShipDeathPreloadSubsystem.*`
  - `EnemyBrainComponent.*`, `StartechEnemyShip.*`, `StartechSettings.h`
  - `UI/HUD/HudDraw.cpp`, `UI/HUD/HudReticleWidget.*`, `UI/HUD/HudScreenFeedbackWidget.*`
  - `BehaviourConsoleCommands.cpp`, `Tests/StartechBehaviourTests.cpp`
  - `Tools/Combat/setup_combat_balance.py`
  - `Config/DefaultGame.ini`
  - `CLAUDE.md` (ranged and charged-shot section, the new commands, a HUD projection section)
- **Content** (by the script):
  - `DA_CombatBalance`: ranged behaviour and damage
  - `DA_BasicWeapon_Ranger`: damage 0.7
