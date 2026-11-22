# Combat balance, Phase 3: the combat log and the play sessions

Task: `Docs/Tasks/TASK_CombatBalance_1_Numbers.md`. Phases 1 and 2 were accepted, including the four Phase 2 deviations.

## Summary

- **Combat log:** `UCombatTelemetrySubsystem` records every engagement, writes one CSV line per engagement to `Saved/CombatLogs/`, and answers `Startech.Combat.Report`. It isn't created in shipping builds.
- **Play sessions:** one per tier on test slot 3, with the expected gear, flown by a new test pilot (`Startech.Test.AutoFight`). Each tier's report is below.
- **Drifters and ranged enemies fire at the player** in every tier:
  - Drifters fired 250, 50, 62 and 64 shots (tiers I–IV).
  - Ranged enemies fired 254, 352 and 384 shots (tiers II–IV) and hit with 96–98% of them.
- **Time to kill in play matches the TTK table** at every tier (details below).
- **Money:** every kill is profitable. Net credits per minute rise with tier: 130 → 514 → 1,176 → 1,513.
- **Bug found and fixed: the player couldn't hit a ranged enemy from the front.** See the next section.
- **Finding for you: in play, drifters and chasers cost far less health than the table's 20–25%.** See "Health lost".

## Bug fixed: ranged enemies could not be hit nose-on

In the first test, a ranged enemy took 120 player shots without one hit and killed the player.
- Player shots trace a zero-width line to the target's centre.
- The ranged ship's collision (`SM_StarSparrow32`, 13 convex pieces) has a gap under 1 m wide at its centre when seen from the front. A ranged enemy faces the player while it fights, so the line passed between the pieces.
- A probe confirmed it: the line traced against the enemy's mesh alone missed, while a 0.5 m sphere swept along the same line hit.
- Real players would have hit the same wall: a hard-locked ranged enemy facing them couldn't be hurt.

**Fix:** `UWeaponMountComponent::TraceShot` now sweeps a sphere of `DA_CombatBalance::ShotRadius` (50 cm), set by the content script.
- It applies to every laser and drainer shot, the player's and the enemies'.
- Player hit rates are now 96–100% against every type, against 18% (and 0% nose-on) before.
- Enemies already hit 96–100%, so their damage is unchanged. Asteroid edges now block shots slightly more often.

## The play sessions

Each session:
- Starts the test game on slot 3 (overwritten) and sets the tier's expected upgrades and ammo while docked.
- Undocks, warps to the tier's sector, and runs `Startech.Test.AutoFight Any 150 60 <ammo>`: fight the nearest enemy of any type at 60 m for 150 s.
- Warps back after a death and fights another 150 s, then prints `Startech.Combat.Report`.

| Tier | Sector | Gear |
|---|---|---|
| I | Terra | LL1, Lasers 0, Hull 0, Shield 0 |
| II | Rust | LL2, Lasers 2, Hull 1, Shield 1 |
| III | Glacier | LL3, Lasers 3, Hull 3, Shield 2 |
| IV | Jove | LL3, Lasers 5, Hull 5, Shield 5 |

The pilot hard-locks its target, so the pawn's combat stance faces it and aims at it, just as a player's lock does. It holds the trigger while the target is in laser range. When the target is more than 180 m away, it jumps to 90 m from it instead of flying through the asteroid field. That trip isn't what a session measures, and the fight itself is flown normally.

### Reports (`Startech.Combat.Report`, verbatim)

Columns:
- **Eng / Kills / Died:** engagements, kills, and engagements in which the player died.
- **TTK on/total:** average per kill. "On" is the span of the player's volleys; "total" runs from the first shot to the kill.
- **Hit%:** share of the player's shots that hit.
- **Taken/kill and Lost/kill:** health taken per kill, and as a share of max health.
- **Profit/kill:** the drop's credits plus mineral sell value, minus the ammo.
- **cr/min fight:** that profit per minute of fighting.
- **EnemyShots/Hits:** the enemies' shots inside engagements, and how many hit.

**Tier I (Terra)**

```
Session 5.3 min, 18 engagement(s), CSV D:/Unreal Projects/Startech/Saved/CombatLogs/Combat_2026.11.22-04.46.28.csv
Tier Enemy    Eng Kills Died  TTK on/total    Hit%   Taken/kill  Lost/kill  Profit/kill  cr/min fight  EnemyShots/Hits
1    Drifter    18    17    1   4.25/  4.40 s   97.0%       13.5       9.0%         40.5         532.3  250/232
Total gained 688 cr, ammo 0 cr, net 688 cr: 129.6 cr/min over the session.
Enemy shots fired this session: Drifter 250
```

The second half of this session was idle. After the death the player respawned docked at the Terra station, and the pilot doesn't undock.

**Tier II (Rust)**

```
Session 5.6 min, 24 engagement(s), CSV D:/Unreal Projects/Startech/Saved/CombatLogs/Combat_2026.11.22-04.52.08.csv
Tier Enemy    Eng Kills Died  TTK on/total    Hit%   Taken/kill  Lost/kill  Profit/kill  cr/min fight  EnemyShots/Hits
2    Chaser     13    12    1   5.99/  6.11 s   98.1%       17.1       4.9%        153.9        1471.8  138/128
2    Drifter     6     6    0   2.75/  3.09 s   95.8%       10.8       3.1%         68.9        1335.5  50/50
2    Ranged      5     4    1   7.99/  8.16 s   98.6%      106.5      30.4%        152.0        1040.3  254/246
Total gained 3337 cr, ammo 469 cr, net 2868 cr: 514.1 cr/min over the session.
Enemy shots fired this session: Chaser 138, Ranged 254, Drifter 50
```

**Tier III (Glacier)**

```
Session 5.6 min, 35 engagement(s), CSV D:/Unreal Projects/Startech/Saved/CombatLogs/Combat_2026.11.22-04.57.53.csv
Tier Enemy    Eng Kills Died  TTK on/total    Hit%   Taken/kill  Lost/kill  Profit/kill  cr/min fight  EnemyShots/Hits
3    Chaser      9     9    0   5.49/  5.58 s  100.0%       13.2       2.0%        309.4        3326.1  40/38
3    Drifter    17    17    0   2.50/  2.61 s  100.0%        8.2       1.3%        102.6        2359.5  62/56
3    Ranged      9     7    2   7.49/  7.65 s   99.2%      140.0      21.5%        288.3        1932.9  352/342
Total gained 7714 cr, ammo 1168 cr, net 6546 cr: 1175.9 cr/min over the session.
Enemy shots fired this session: Drifter 62, Chaser 40, Ranged 352
```

**Tier IV (Jove)**

```
Session 5.6 min, 21 engagement(s), CSV D:/Unreal Projects/Startech/Saved/CombatLogs/Combat_2026.11.22-05.03.44.csv
Tier Enemy    Eng Kills Died  TTK on/total    Hit%   Taken/kill  Lost/kill  Profit/kill  cr/min fight  EnemyShots/Hits
4    Chaser      6     4    2   7.50/  7.57 s  100.0%       76.3       6.6%        494.8        3456.6  88/84
4    Drifter     9     9    0   3.50/  3.62 s  100.0%       22.0       1.9%        244.4        4052.2  64/60
4    Ranged      6     6    0  10.24/ 10.45 s  100.0%      291.1      25.3%        709.1        4069.7  384/378
Total gained 9384 cr, ammo 950 cr, net 8434 cr: 1512.5 cr/min over the session.
Enemy shots fired this session: Drifter 64, Chaser 88, Ranged 384
```

**Extra session: tier II chasers only, fought at 30 m** (`Startech.Test.AutoFight Chaser 90 30 Item.Ammo.LL2`)

```
Session 1.8 min, 8 engagement(s), CSV D:/Unreal Projects/Startech/Saved/CombatLogs/Combat_2026.11.22-05.10.00.csv
Tier Enemy    Eng Kills Died  TTK on/total    Hit%   Taken/kill  Lost/kill  Profit/kill  cr/min fight  EnemyShots/Hits
2    Chaser      8     7    1   6.00/  6.06 s   97.7%       42.2      12.1%        201.0        1769.9  216/216
Total gained 1584 cr, ammo 177 cr, net 1407 cr: 796.9 cr/min over the session.
Enemy shots fired this session: Chaser 216
```

## Against the targets

**Time to kill: matches the table.** The measured "on" time spans N volleys, which is (N − 1) shot intervals, so it reads about 0.1–0.25 s under the table.

| Tier | Regular type | Table | Measured (total) |
|---|---|---|---|
| I | Drifter (LL1 L0) | 4.38 s | 4.40 s |
| II | Chaser | 6.00 s | 6.06–6.11 s |
| III | Chaser | 5.70 s | 5.58 s |
| IV | Chaser (LL3) | 7.75 s | 7.57 s |

Ranged, the tough type, matched too: 8.06 / 7.66 / 10.42 s in the table against 8.16 / 7.65 / 10.45 s measured.

**Health lost: ranged enemies match the table, but drifters and chasers cost much less.**

| Tier | Type | Table (Lose1) | Measured, kiting at 60 m | Measured, at 30 m |
|---|---|---|---|---|
| I | Drifter | 23.3% | 9.0% | |
| II | Chaser | 22.3% | 4.9% | 12.1% |
| III | Chaser | 21.9% | 2.0% | |
| IV | Chaser | 22.2% | 6.6% | |
| II / III / IV | Ranged (tough) | 33.5 / 33.0 / 33.5% | 30.4 / 21.5 / 25.3% | |

- The table assumes the enemy fires for the whole fight. Ranged enemies do, because their 100 m gun covers the fight.
- Drifters and chasers spend much of each fight turning and closing in.
  - At 60 m the pilot stays outside a chaser's 40 m gun, so chasers barely fire (138 shots in 13 fights at tier II).
  - Even at 30 m, a chaser fires only about 31 of a possible 48 volleys (two shots each), so it does about 55% of the table's damage.
  - Drifters (passive until hit, 75 m gun) fired about 14 shots per fight at tier I, against 35 possible.
- So the one-fight target of 20–25% is met only as the table's worst case. In actual play, regular fights cost about 5–12%.

**Deaths:** the player died in most sessions, after 7–19 kills in a row. Hull doesn't regenerate in flight and the pilot never retreats, so damage adds up across fights. Being fought three at once was also lethal, as intended.

**Money (30% rule):** ammo cost 0–15% of what kills paid at every tier. Net credits per minute were 130 / 514 / 1,176 / 1,513 for tiers I–IV, so the outer sectors pay better.

## Decision for you: chaser and drifter damage

The table's 20–25% loss is a worst case. Live chasers cost about half of it even when fought up close, and drifters about 40%. Options:
- **(a) Keep the numbers.** The table stays a worst-case check, and live fights are easier than it says.
- **(b) Raise chaser damage about 1.8× and drifter damage about 2.5×** (1.25 → 2.25 and 1.0 → 2.5 per shot) so live fights land near 20–25%. Then add a "fire uptime" factor per type to the table (`FEnemyBalance`, about 0.55 chaser, 0.4 drifter, 1.0 ranged) so the table keeps predicting play.
- **(c) Something in between.** It's a single constant per type in `setup_combat_balance.py`.

The kiting result (2–7%) also shows a 40 m chaser gun can be out-ranged by a player who backs off. That's a behaviour question for the later enemy-behaviour task.

## What changed in code

- **Combat log:** `Combat/CombatTelemetrySubsystem.h/.cpp` (`UTickableWorldSubsystem`, game worlds, not created in shipping).
  - It listens to three new native static events, so nothing binds to pooled enemies:
    - `UHealthComponent::OnAnyDamageApplied`: shield and hull damage, instigator, drain flag, whether fatal. Raised from `ApplySplitDamage`, which now also takes the drain flag.
    - `ULaserMountComponent::OnAnyShotFired`: the mount, the actor its trace hit, the ammo.
    - `AStartechEnemyShip::OnAnyEnemyDied`, after the drop. The drop is kept in `GetLastDrop()` and cleared every life; `DropLoot` is no longer const.
  - **Engagements:** start on the first player shot at an enemy (its trace hit, else the current target) or the first damage the enemy deals. They end on its death, the player's death, or 15 s idle. Open engagements are written as disengaged when the world ends.
  - Ammo cost and loot value use `UCombatBalance::GetRoundCost`, `GetSellValue` and `GetBundleValue`, the same helpers the TTK table now uses.
  - **CSV columns:** Start, Enemy, Tier, End, Ammo, LasersLevel, TimeOnTarget, TotalTime, ShotsFired, ShotsHit, ShotsBlocked, ShotsMissed, Rounds, RoundsByAmmo, AmmoCost, ShieldTaken, HullTaken, HealthLostPct, EnemyShots, EnemyHits, Credits, LootValue, LootAmmoRounds, NetProfit, PlayerDied.
  - **Cost:** work happens only on shots, damage and deaths, plus a per-frame loop over the few open engagements. The disk is written once per finished engagement.
- **`Startech.Combat.Report`** is in `CombatConsoleCommands.cpp`.
- **Test pilot (non-shipping):** `Combat/CombatTestPilot.h/.cpp` (`UCombatTestPilot`) and `Startech.Test.AutoFight <Type|Any|stop> [Seconds] [DistanceM] [AmmoTag]`.
  - It logs `[Pilot]` every 5 s with a hit probe: the scene trace, the mesh alone, and spheres of 0.5, 1 and 2 m.
  - It needed two small public hooks: `UTargetingComponent::LockTarget` and `AStartechShipPawn::SetFireHeld` / `GetTargeting`.
- **Shot radius:** `UWeaponMountComponent::TraceShot` sweeps a sphere of `UCombatBalance::ShotRadius`. The content script sets it to 50 cm.
- **`CLAUDE.md`** covers the shot radius, the native combat events, the combat log, the report and the test pilot.

## Tests

All 13 automation tests pass: Startech.Combat (5, including TTKTargets), Shop (5), Save (1), Tiers (1) and Flow (1). The TTK table still has no row outside a target.

## Not done

- The pilot doesn't undock after respawning, so the tier I session's second half was idle. The other tiers re-warp, which undocks.
- The task asked for one session per tier. A longer or repeated session would give more ranged kills per tier (4–7 here).
