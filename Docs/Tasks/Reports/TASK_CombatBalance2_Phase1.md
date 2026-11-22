# Combat balance 2 (behaviour): Phase 1 report

Phase 1 covers the drifter and the chaser:
- **Drifter:** passive, fights back when the player hits it, then gives up.
- **Chaser:** aggressive. It afterburns to close the gap, circles the player in range, and gives up when the fight goes quiet or the player leaves.

All behaviour numbers are in `DA_CombatBalance`. Every life starts calm, and death stops everything at once.

All 22 `Startech` automation tests pass, 6 of them new. The build has no new warnings, and no plugin was changed.

Phase 0 answers applied here: chaser speed 2,400 with a ×1.8 afterburner. The ranged enemy (aim, damage split, retreat) is Phase 2.

## What changed

### Where the numbers live
- `FEnemyBalance` (one per type in `DA_CombatBalance`) has a new `Behaviour` field, `FEnemyBehaviourBalance` (`Public/Combat/CombatBalance.h`). A 0 switches a feature off or falls back to the type's `UEnemyData`.
- `AStartechEnemyShip::ApplyStats` copies it into the life each spawn (`StartechEnemyShip.cpp:165`). The brain reads it through `GetBehaviour()` and `GetHomeLeashRange()`.
- `Tools/Combat/setup_combat_balance.py` has a new `BEHAVIOUR` dict. The `enemies` step also writes the forward speed and leash range into each `DA_Enemy_*`, so the editor shows the same numbers.

| | Drifter | Chaser | Ranged (Phase 2) |
|---|---|---|---|
| Forward speed | 1,500 (unchanged) | **2,400** (was 3,000) | 3,500 |
| Afterburner | – | ×1.8 = 4,320 cm/s, up to 3 s, 6 s cooldown, starts beyond 1.2 × 40 m = 48 m | – |
| Circles in range | – | yes, strafing at its 1,200 cm/s, swapping direction every 2–4 s | (already strafes) |
| Gives up: quiet | 20 s without taking damage | 25 s without dealing or taking damage | – |
| Gives up: far | player beyond 2 × 75 m = 150 m for 8 s | – | – |
| Gives up: home | 300 m (unchanged) | **300 m** (was 150 m) | 300 m |

**Why the chaser's home leash went from 150 m to 300 m.**
- The leash is the distance from the enemy's spawn point to the player. Enemies can't know their ProcSector zone without a plugin API, so this stands in for "the zone plus a clear margin".
- At 150 m, a player who simply flew away at 3,000 cm/s from a chaser 60 m behind crossed the leash in about 3 s. The chaser gave up before its burn could close the gap, so flying away beat chasers through the leash, not the speeds.
- At 300 m, the boost test shows a boosting player still escapes (below).

### The brain (`Private/EnemyBrainComponent.cpp`)
It is still the C++ state machine (Idle → Engage → ReturnHome). The additions:

- **Leash** (`UpdateLeash`, `:232`):
  - The home range is the existing check, now per life.
  - The quiet time is reset by damage from the player (`HandleOwnerDamaged`, `:478`). For the chaser it is also reset by damage it deals to the player, through the native `UHealthComponent::OnAnyDamageApplied` (`HandleAnyDamage`, `:496`).
  - The far time counts only continuous seconds beyond the distance.
  - Giving up holds fire, ends any burn and flies home (`GiveUp`, `:360`). After that the existing rules apply: a drifter wanders again and a chaser re-engages on sight once home. Shield regenerates as before; hull never does.
- **Afterburner** (`UpdateAfterburn`, `:273`):
  - Uses the shared flight model's boost scale (`SetFlightScales(1, 1.8)`), which raises the top speed and acceleration. The player's boost uses the same scale.
  - A burn starts beyond 48 m once the cooldown is over. It ends after 3 s, or earlier, at the point where braking from now would stop it at laser range. Then the 6 s cooldown starts.
- **Circling and the approach** (`TickEngage`, `:158`; `GetFlankPoint`, `:335`):
  - **In laser range:** it faces the player, holds 32–40 m and strafes.
  - **Out of range:** it heads for a point beside the player instead of the player itself. That point blends from the player (at 1.5 × laser range, 60 m) to 36 m to the side (at laser range), so it arrives circling instead of crossing the player's path.
  - A fixed 36 m offset from the start turned out to angle the approach so much that a 3 s burn couldn't catch a fleeing player (37° off at 60 m).
- **No overshoot** (`GetPredictedDistance`, `:260`):
  - The brain predicts where the ship stops if it brakes now: the distance minus closing speed² ÷ (2 × acceleration), with the closing speed measured relative to the player's motion. It ends the burn and starts reversing on that predicted distance.
  - **Why it was needed:** the first in-game run had chasers coming out of a burn at 4,320 cm/s and coasting to 10–16 m, nearly bumping the player. Braking at 2,500 cm/s² from that speed takes about 37 m.
  - It applies to every type, but only matters for fast ones.
- **Pooling and death:**
  - `ResetBrain` (`:53`) clears the state, the give-up reason, both leash timers, the burn and the cooldown.
  - `ResetForNewLife` sets the flight scales back to 1 (`StartechEnemyShip.cpp:90`).
  - `StopBrain` (`:46`, called on death) ends a burn at once.

### Other changes
- `AStartechShipBase::GetAcceleration()` and `AStartechEnemyShip::GetBrain()`: read-only accessors.
- The brain gains read-only queries for tests and logs: `IsEngaged`, `IsReturningHome`, `IsAfterburning`, `GetAfterburnCooldown`, `GetStateName` and `GetLastGiveUpReason`.
- `Startech.Test.Enemies` (non-shipping, `Private/BehaviourConsoleCommands.cpp`) logs one `[Brain]` line per enemy near the player or engaged: state, give-up reason, distance, speed, top speed, afterburner and cooldown.
- **Shared test world** (`Private/Tests/StartechTestWorld.h`): the death tests' fixture, moved out so both suites use it.
  - It now has a bare game instance and `AGameModeBase`. Without them the engine drops all damage to pawns (`APawn::ShouldTakeDamage` needs an authority game mode), so "fights back" could not be observed.
  - `SpawnPlayer` possesses the ship with a player controller, so the brain finds it as the player.

## Automation tests (`Private/Tests/StartechBehaviourTests.cpp`)

The tests use real Blueprints (`BP_Enemy_Drifter`, `BP_Enemy_Chaser` from the pool, `BP_ShipPawn` possessed) in a hand-ticked game world.

| Test | What it shows |
|---|---|
| `DrifterNeverAttacksFirst` | Player 30 m in front of a drifter for 8 s: never engages, no damage. A sanity check confirms damage from the drifter *would* be counted in this world. |
| `DrifterFightsBackThenGivesUp` | One hit from the player: it engages at once and damages the player within 4 s. The player then takes no more damage. The drifter is still engaged 1 s before its 20 s quiet leash and returning home (reason "quiet") 0.5 s after it. |
| `ChaserAfterburnsAndCatchesAFleeingPlayer` | Player 60 m ahead flying straight away at 3,000 cm/s: the chaser afterburns at once and is within its 40 m gun after **2.4 s**. The burn then ends, the cooldown runs and the flight scale is back to 1. |
| `BoostingAwayEscapesAChaser` | Player 45 m ahead, boosting away: the chaser burns after it but never gets closer than **45 m**. After 10 s the player is **332 m** away and the chaser has given up (home leash). |
| `ChaserNeverBumpsThePlayer` | Player sitting still 75 m away: the chaser burns in, closest **31.2 m**, and settles at 38 m, inside its gun. |
| `PooledEnemyComesBackCalm` | A chaser released to the pool while engaged and mid-burn comes back idle: not engaged or returning, no burn, no cooldown, flight scale 1, speed 0, and calm a second later. |

The 16 earlier tests, including the death tests on the shared world, still pass.

## In-game check: tier II chasers in Rust

This ran on test slot 3 with tier II gear (Lasers 2, Hull 1, Shield 1, LL2). The test pilot fought chasers for 85 s at its default 60 m: hard lock, holding 60 m ±10 m by throttle and reverse. `Startech.Test.Enemies` was logged every second. The log is `Saved/Logs/ChaserCheck2.log`.

**Distances** (75 samples of an engaged chaser against the pilot):

| 30–39 m | 40–49 m | 50–59 m | 60–69 m | 70–99 m |
|---|---|---|---|---|
| 22 | 21 | 19 | 2 | 11 |

- The closest was **33 m**, and it never came inside 27 m. Before the braking fix, the same session had chasers at 10, 11 and 16 m.

**How a fight plays (feel):**
1. Pilot at 60 m. The chaser, beyond 48 m, burns (4,320 cm/s) and arrives at 30–40 m.
2. It circles there, strafing and firing, while the pilot reverses to get back to 60 m. The pilot's reverse (2,500) is just faster than the chaser's normal speed (2,400), so the gap opens slowly, to 45–60 m during the 6 s cooldown.
3. At about 50 m the next burn closes it again.

So the rhythm alternates; neither side gets stuck. A player who backs off gets distance back between bursts and loses it to each burst, as the spec asks.

**Combat report:**
```
[CombatReport] Session 2.0 min, 13 engagement(s)
[CombatReport] Tier Enemy    Eng Kills Died  TTK on/total    Hit%   Taken/kill  Lost/kill  Profit/kill  cr/min fight  EnemyShots/Hits
[CombatReport] 2    Chaser     13    13    0   5.99/  6.15 s   98.0%        5.8       1.6%        138.6        1354.9  46/46
[CombatReport] Enemy shots fired this session: Chaser 194
```

**For Phase 3:** chasers are now hard to out-run, but still cheap against a player who backs off: **1.6% of health per kill** against the 15–25% target. They spend about half of each fight at 40–60 m, outside their 40 m gun, and a kill takes only 6 s.

Possible levers, to measure in Phase 3 with the back-off option the task asks for:
- a shorter cooldown (6 → 4 s)
- burns that start at the gun's range rather than 1.2× it
- a 45–50 m gun
- higher damage per shot together with the fire-uptime factor

I haven't changed any of these yet, since Phase 3 is where damage is tuned.

## Notes

1. **The cost of a far drifter test.** The drifter's far leash (150 m for 8 s) is in place but has no automation test. In a test world a player that keeps 150 m away from a drifter also leaves its 300 m home leash within those 8 s. The quiet leash, which is the one a beginner meets, is tested.
2. **Re-engaging after giving up:** a chaser that gave up re-engages on sight once it is home and the player is within 80 m. A drifter only re-engages if hit again.
3. **Engines upgrade:** the afterburner (4,320) still beats the player's normal speed at Engines 5 (3,900). The Phase 3 check at Engines 3–5 in tiers III–IV is still to do, as you asked.

## Files

- **New:**
  - `Private/BehaviourConsoleCommands.cpp`
  - `Private/Tests/StartechBehaviourTests.cpp`
  - `Private/Tests/StartechTestWorld.h`
- **Changed:**
  - `Public/Combat/CombatBalance.h`
  - `Public|Private/EnemyBrainComponent.*`
  - `StartechEnemyShip.h/.cpp`
  - `StartechShipBase.h`
  - `Private/Tests/StartechDeathTests.cpp` (uses the shared world)
  - `Tools/Combat/setup_combat_balance.py`
  - `CLAUDE.md`
- **Content** (by the script):
  - `DA_CombatBalance`: behaviour rows
  - `DA_Enemy_Chaser`: forward speed 2,400, leash range 300 m
