# Combat balance 2 (behaviour): Phase 3 report (measure and tune)

All three enemy types were measured live in every tier with the expected gear, using new test-pilot styles. The TTK table now predicts those fights.

**Result against the targets:**
- **On target:** drifters (12.8%), chasers when you face or circle them (18–25%), and ranged with and without dodging (24–31% / 12–14%).
- **Not on target: chasers against a player who backs off.** That costs 6% in tier II, and almost nothing in tiers III–IV once the Engines upgrade makes the player's reverse faster than the chaser. Burn timing, the lever you said to try first, is used up. My proposal is below; I haven't applied it.

All 29 `Startech` automation tests pass, including `Startech.Combat.TTKTargets` (no value outside a target). The build has no new warnings, and no plugin was changed.

## Results

Health lost per kill, from `Startech.Combat.Report`. Each cell is one segment of 90–100 s against one enemy type, with 7–17 kills.

| Fight | Target | Tier I | Tier II | Tier III (Engines 3) | Tier IV (Engines 5) | TTK table |
|---|---|---|---|---|---|---|
| Drifter, pilot holds 40 m | 5–15% | **12.8%** | – | – | – | 14.2% |
| Chaser, pilot holds 30 m (faces it) | 20–25% | – | **23.5%** | **22.7%** | **25.4%** | 23.0 / 22.7 / 23.0% |
| Chaser, pilot circles at 30 m | 20–25% | – | **21.8%** | 17.9% | **22.8%** | (same) |
| Chaser, pilot backs off from max range | 10–15% | – | 6.0% | 0.2% | 0.0% | 5.4% |
| Chaser, pilot backs off, met in aggro range | 10–15% | – | 6.5% | 3.3% / 0.2% | 0.6% | – |
| Ranged, pilot holds still (nothing dodged) | 25–30% | – | **30.8%** | **25.1%** | 23.6% | 29.6 / 30.0 / 25.8% |
| Ranged, pilot dodges every telegraph | 12–18% | – | **13.9%** | **12.4%** | **13.1%** | 12.9 / 12.7 / 12.9% |

- **Ranged charged shots hit:** 10 of 12, 8 of 10 and 7 of 12 with the pilot holding still. **None of 32** hit when it dodged (0/10, 0/11, 0/11).
- **Tier IV no-dodge (23.6%) is slightly under target.** 5 of 12 shots missed a pilot that never touched the throttle, because it still coasts to a stop after closing in. That is ordinary noise in an 8-kill sample.
- **Drifters in tiers II–IV** are the "weak" role and have no target. The table shows 4–5% there.

**Gear per tier:**

| Tier | Lasers | Hull | Shield | Engines | Ammo |
|---|---|---|---|---|---|
| I | 0 | 0 | 0 | 0 | LL1 |
| II | 2 | 1 | 1 | 0 | LL2 |
| III | 3 | 3 | 2 | **3** | LL3 |
| IV | 5 | 5 | 5 | **5** | LL3 |

Engines 3–5 in tiers III–IV is as you asked. Test slot 3 is overwritten.

## How the fights feel (the push-and-pull check)

- **Drifter.** It doesn't move until hit, then turns and fires back while trying to hold its 65 m. Against a player at 40 m it reverses slowly (1,800 cm/s) while the player follows, so nobody gets stuck. It fires about 61% of the fight; the rest is turning. A gentle, readable target.
- **Chaser, faced or circled.**
  - It sits at 30–36 m, strafing and swapping direction every 2–4 s while the player strafes and swaps every 3–5 s. The two ships orbit and keep re-crossing each other's line, which is the circling the spec asks for.
  - Neither side gets stuck. The chaser fires 58–78% of the fight, and never comes inside about two ship lengths: closest 27.2 m in `ChaserNeverBumpsThePlayer`, and 33 m in the Phase 1 session.
  - When the player drifts past 40 m, the next burn (4,320 cm/s) closes it within a second or two.
- **Chaser, player backing off, tier II (Engines 0).** It alternates:
  1. The burn brings the chaser in to about 33 m.
  2. The player's reverse (2,500) barely outpaces its normal 2,400, so the player gets distance back slowly.
  3. After 4 s it burns again.

  The chaser fires only 15–17% of the fight. A 6 s kill is mostly over before it has arrived and settled.
- **Chaser, player backing off, tiers III–IV (Engines 3–5): one-sided.** The player's reverse is 2,950–3,250 against the chaser's 2,400, so the gap opens at 5–8 m/s between bursts. A 4.5 s burn at 4,320 gains only 5–7 m/s. The chaser fires 0–2% of the fight: **backing off beats it without needing the boost.**
- **Ranged.**
  - It holds 81–99 m, strafing and reversing, and every 6–8 s glows for 1.2 s.
  - A player who keeps the same motion is hit about two times in three. One who changes strafe within about 0.3 s of the glow is never hit.
  - That gives a clear rhythm: watch for the glow about every 7 s, sidestep, and resume. Dodging halves the damage in every tier (24–31% → 12–14%).
  - Closing on it below 36 m makes it turn and flee (Phase 2), so pushing in buys a quiet window but doesn't win the fight.

## What changed in Phase 3

### Test pilot (`Private/Combat/CombatTestPilot.*`, non-shipping)
- **`Startech.Test.PilotStyle <face|circle|backoff|hold> [Dodge] [BackOffThrottle]`:**
  - `face`: holds the distance.
  - `circle`: holds it while strafing, swapping every 3–5 s.
  - `backoff`: reverses at a set throttle.
  - `hold`: closes in once per target, then never touches the throttle.
  - With `Dodge`, it changes its strafe 0.3 s after a telegraph aimed at it: a circler flips direction, any other style strafes for 2.5 s. It strafes through the new `AStartechShipPawn::SetSteerAxis`, the A/D axis.
- **`Startech.Test.AutoFight … [Sector]`:**
  - A death no longer stops the pilot. It waits for the respawn, undocks, warps back to the fight's sector (retrying while a warp is refused in combat) and carries on. This was the Part 1 follow-up.
  - Out of the sector, it jumps back to the fight instead of letting the out-of-bounds timer kill the ship.
  - Its stop line counts kills, dodges and deaths.
- **New commands:**
  - `Startech.Combat.Reset` starts a new report session; the CSV keeps everything.
  - `Startech.Test.Heal` refills hull and shield between measured fights.
- **`Startech.Combat.Report`:** a new **Uptime** column, the enemy's laser shots over the most its two mounts could fire in those engagements.

### TTK table (`Combat/CombatTTKTable.*`)
- **Fire uptime:** each type's lasers fire for its measured `FireUptime` share of the fight. That includes pauses for charging, which is how it's measured.
- **Charged shots** come on the type's schedule (the new "Charged" column).
- **A second loss column** ("Alt case"):
  - every charged shot **dodged** (ranged)
  - the player **backing off**, using `FireUptimeAlt` (chaser)
- **Per-type loss targets** (`LossTargetMin/Max`, `LossAltTargetMin/Max`, `bLossTargetsRegularOnly`) replace the role's 20–25% where set.
- **Data:** all of this lives in `FEnemyBalance`, set by the script's new `TABLE` dict.

Measured uptimes:

| Type | Uptime | Where measured |
|---|---|---|
| Drifter | 0.61 | tier I |
| Chaser | 0.68 | 58–78% across tiers II–IV |
| Chaser, player backing off | 0.16 | tier II |
| Ranged | 0.77 | 67–85% |

The table matches the live fights within 1–3 points everywhere it has a target (see Results).

### Numbers changed (all in `Tools/Combat/setup_combat_balance.py`)

| Number | Was | Now | Why |
|---|---|---|---|
| Chaser `AfterburnTriggerFactor` | 1.2 | **1.0** | Burns as soon as the player leaves gun range (your first suggestion). |
| Chaser `AfterburnCooldown` | 6 s | **4 s** | Burn timing. |
| Chaser `AfterburnSeconds` | 3 s | **4.5 s** | Burn timing: closes a backing-off player from max range. |
| Chaser preferred range | 36 m | **33 m** | Its band was 32–40 m against a 40 m gun, so any drift switched it to approach mode, which doesn't fire. Now 29.7–36.3 m. 33 m keeps the no-bump distance (≥ 27 m). |
| Chaser damage per shot | 1.25 | **1.9** | Engaged fights were 14% at 62% uptime; burn timing can't help a player who doesn't flee. This lands them at 22–25%, inside the target, so I applied it under your rule. The TTK table's old worst case (uptime 1) would read 34%; with the measured uptime it reads 23%. |
| Ranged charged-shot damage | 40 | **45** | Nothing-dodged fights were 22–25% because a third of charged shots miss even a still player. At 45 the table's all-hit rows are 26–30% and live 24–31%. Dodged fights are unchanged. |

### Brain fixes found by measuring (`EnemyBrainComponent.cpp`)
1. **A burn ended too early against a fleeing player.**
   - It ended where braking to a full stop would leave it at gun range. That is right against a player sitting still, but a player reversing faster than the chaser's normal speed was never reached: back-off fights showed **0 chaser shots**.
   - A burn now ends on reaching the preferred range. Overshoot is prevented separately: the range band reverses whenever the predicted stopping point is inside it, burning or not.
2. **The automation test changed with it.** `ChaserAfterburnsAndCatchesAFleeingPlayer` now waits for the burn to end (within 3 s) instead of expecting it to end at 40 m. `ChaserNeverBumpsThePlayer` still holds the 27 m floor (closest 27.2 m).

## Proposal (not applied): chasers against a player who backs off

The target is 10–15%; measured, it's 6% in tier II and 0–3% in tiers III–IV.

**Why burn timing can't reach it:** the approach is limited by the agreed ×1.8 burn (4,320). Closing on a player reversing at 2,500 takes 18 m/s, and the 6 s fight is half over before the chaser settles. In tiers III–IV the Engines upgrade makes the reverse faster than the chaser's normal speed, so the gap reopens quickly between burns.

1. **Per-tier chaser speed** (your suggestion for tiers III–IV): normal speed II 2,400 / III 2,800 / IV 3,100, burning to 4,320 / 5,040 / 5,580.
   - It stays slower than the boost at the expected Engines level (6,600 / 7,790 / 8,580), so boosting away always escapes.
   - It roughly matches the player's reverse (2,500 / 2,950 / 3,250), so backing off opens distance only slowly, as in tier II now.
   - It would be a `ForwardSpeedByTier` list (or a tier speed multiplier of 1.0 / 1.17 / 1.29) next to `ForwardSpeed` in the chaser's behaviour numbers.
2. **For tier II to reach 10%** (6% today, also the base for 1): either
   - a 45 m chaser gun with damage per shot trimmed back so engaged fights stay at about 22% (the chaser would fire sooner and longer against a retreating player), or
   - a ×2.0 burn (4,800) instead of ×1.8.

   I'd try the gun first: it changes reach, not the chase itself.

The TTK table already carries the chaser's back-off column (5.4% at uptime 0.16). Its target check is off until you choose, so the TTK test only fails on things that are decided.

## Raw reports (final runs)

From the logs `Saved/Logs/BehaviourI-T1..4.log`, `BehaviourJ1/J3`, `BehaviourK2..4` and `BehaviourL-T2..4.log`; columns as in `Startech.Combat.Report`.

```
Tier Enemy    Eng Kills Died  TTK on/total    Hit%   Taken/kill  Lost/kill  Profit/kill  cr/min fight  EnemyShots/Hits  Uptime  Charged hit/fired (dmg)
-- drifter, face 40 m
1    Drifter    18    17    1   4.02/  4.14 s   97.1%       19.2      12.8%         49.4         680.2  360/348     61%  -
-- chaser, face 30 m
2    Chaser     15    12    3   5.99/  6.05 s   98.2%       82.3      23.5%         78.0         711.9  428/426     68%  -
3    Chaser     15    12    3   5.48/  5.55 s  100.0%      147.2      22.7%        148.3        1403.0  426/411     70%  -
4    Chaser     10     8    2   7.49/  7.55 s   99.7%      291.6      25.4%        405.4        2656.0  458/458     78%  -
-- chaser, circle 30 m
2    Chaser     13    11    2   5.97/  6.07 s   97.6%       76.3      21.8%         96.8         859.9  384/376     65%  -
3    Chaser     14    12    2   5.49/  5.56 s  100.0%      116.4      17.9%        238.8        2285.2  352/343     58%  -
4    Chaser     11     9    2   7.47/  7.56 s  100.0%      261.9      22.8%        530.8        3750.0  416/414     68%  -
-- chaser, backoff from max range (pilot jumps in at 90 m, opens fire)
2    Chaser     12    12    0   5.99/  7.30 s   98.0%       21.0       6.0%         83.7         687.9  108/102     15%  -
3    Chaser      8     8    0   5.49/  8.64 s  100.0%        1.2       0.2%        246.2        1710.9  2/2      0%  -
4    Chaser      1     1    0   7.73/ 60.74 s   98.4%        0.0       0.0%        967.4         955.6  0/0      0%  -
-- chaser, backoff met in aggro range (pilot jumps in at 80 m)
2    Chaser     12    12    0   5.98/  6.94 s   98.0%       22.6       6.5%        144.7        1249.9  114/110     17%  -
3    Chaser     12    12    0   5.49/  7.17 s  100.0%        1.6       0.2%        300.8        2517.7  6/4      1%  -
4    Chaser      9     9    0   6.74/  9.56 s  100.0%        7.0       0.6%        558.3        3503.2  10/10      1%  -
-- ranged, hold (nothing dodged), charged shot 45
2    Ranged     12     8    2   7.99/  8.20 s   98.6%      107.6      30.8%        191.8        1265.4  484/480     83%  10/12 (585)
3    Ranged     11     9    2   7.49/  8.41 s   99.7%      163.3      25.1%        270.5        1837.1  427/416     67%  8/10 (900)
4    Ranged      9     7    2  10.23/ 10.46 s   99.4%      271.2      23.6%        482.6        2367.1  528/523     77%  7/12 (1040)
-- ranged, face 90 m dodging, charged shot 45
2    Ranged     10     9    1   7.99/  8.26 s   98.6%       48.7      13.9%        240.0        1658.9  504/497     81%  0/10 (0)
3    Ranged     11    10    1   7.48/  7.76 s   99.7%       80.5      12.4%        348.5        2588.3  482/467     75%  0/11 (0)
4    Ranged      8     7    1  10.23/ 10.58 s   99.7%      150.5      13.1%        583.3        3042.8  506/498     79%  0/11 (0)
```

Notes on the raw reports:
- **The tier IV back-off from max range** (1 kill in 61 s) shows the problem in its purest form: the chaser never reached the player.
- **"Died"** counts engagements open when the player died. Hull doesn't repair in flight, and the pilot fights back to back, so it died after every 4–12 kills, respawned and carried on.

## TTK table (final)

```
Tier Enemy    Role     Gear      TTK      Lose1    Alt case          Lose3  Charged  Rounds  Ammo cr  Kill cr  Cost%    Targets
1    Drifter   regular  LL1 L0    4.38s    14.2%    -                    85%        0      35      0.0     36.8  0.0%     TTK 3.00-5.00 s, loss 5-15%
1    Drifter   regular  LL1 L1    3.91s    12.7%    -                    76%        0      32      0.0     36.8  0.0%     TTK 3.00-5.00 s, loss 5-15%
2    Drifter   weak     LL2 L2    2.82s    5.1%     -                    31%        0      23     10.4     67.5  15.3%    -
2    Chaser    regular  LL2 L2    6.00s    23.0%    5.4% back off       138%        0      49     22.1    135.0  16.3%    TTK 4.00-6.00 s, loss 20-25%
2    Ranged    tough    LL2 L2    8.06s    29.6%    12.9% dodged        178%        1      65     29.3    202.5  14.4%    TTK 0.00-12.00 s, loss 25-30%, dodged 12-18%
3    Drifter   weak     LL3 L3    2.68s    5.0%     -                    30%        0      22     19.8    126.1  15.7%    -
3    Chaser    regular  LL3 L3    5.70s    22.7%    5.3% back off       136%        0      46     41.4    252.2  16.4%    TTK 5.00-7.00 s, loss 20-25%
3    Ranged    tough    LL3 L3    7.66s    30.0%    12.7% dodged        180%        1      62     55.8    378.3  14.7%    TTK 0.00-14.00 s, loss 25-30%, dodged 12-18%
4    Drifter   weak     LL3 L5    3.65s    5.1%     -                    31%        0      30     27.0    243.0  11.1%    -
4    Drifter   weak     LL4 L5    2.73s    3.8%     -                    23%        0      22      0.0    243.0  0.0%     -
4    Chaser    regular  LL3 L5    7.75s    23.0%    5.4% back off       138%        0      62     55.8    486.0  11.5%    TTK 7.75-10.25 s, loss 20-25%
4    Chaser    regular  LL4 L5    5.81s    17.2%    4.1% back off       103%        0      47      0.0    486.0  0.0%     TTK 5.75-8.25 s
4    Ranged    tough    LL3 L5    10.42s   25.8%    12.9% dodged        155%        1      84     75.6    729.1  10.4%    TTK 0.00-20.25 s, loss 25-30%, dodged 12-18%
4    Ranged    tough    LL4 L5    7.81s    22.6%    9.7% dodged         135%        1      63      0.0    729.1  0.0%     TTK 0.00-16.25 s
14 row(s), 0 outside a target ('*').
```

## Done-when status
- **Each type behaves as described, survives pooling and stops cleanly on death:** yes (Phases 1–2; 13 behaviour tests).
- **"Backing off no longer beats chasers; boosting away still can":**
  - Boosting always escapes.
  - Backing off still beats chasers: partly in tier II, completely in tiers III–IV. **Open:** the proposal above.
- **Charged shots are readable and dodgeable, and dodging measurably reduces damage:** yes, 0 of 32 dodged shots hit, and damage is about halved.
- **The TTK table predicts live fights, and live fights meet the targets:** yes, except chasers against a player who backs off.
- **`CLAUDE.md`:** describes the behaviours, where their numbers live, the pilot's styles and options, the uptime and the table's second column.

## Files
- **Changed:**
  - `Private/Combat/CombatTestPilot.*`
  - `Public|Private/Combat/CombatTTKTable.*`
  - `Public/Combat/CombatBalance.h` (`FireUptime`, `FireUptimeAlt`, loss targets)
  - `Combat/CombatTelemetrySubsystem.*` (`ResetSession`, the Uptime column)
  - `Private/Combat/CombatConsoleCommands.cpp` (`Startech.Combat.Reset`)
  - `EnemyBrainComponent.*` (burn end)
  - `StartechShipPawn.h` (`SetSteerAxis`)
  - `BehaviourConsoleCommands.cpp` (`Startech.Test.Heal`)
  - `Tests/StartechBehaviourTests.cpp`
  - `Tools/Combat/setup_combat_balance.py`
  - `CLAUDE.md`
- **Content** (by the script):
  - `DA_CombatBalance`: chaser and ranged numbers, uptimes, targets
  - `DA_Enemy_Chaser`: preferred range 33 m
  - `DA_BasicWeapon_Chaser`: damage 1.9
