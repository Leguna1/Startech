# Task: combat balance, part 2 (enemy behaviour)

**Status: done (2026-11-22).** Reports: `Reports/TASK_CombatBalance2_Phase1.md`, `_Phase2.md`, `_Phase3.md`, `_Phase3_Review.md`, `_Phase3_Review2.md`, `_Phase3_Final.md`. Accepted with chasers against a player who backs off under the 10–15% aim in tiers III–IV (see Follow-ups).

Read Startech's root `CLAUDE.md` first. `TASK_CombatBalance_1_Numbers.md` and `TASK_DeathSequence.md` are done; read the Follow-ups section of the first one. Reuse `DA_CombatBalance`, the TTK table, the combat log, the test pilot and the death sequence. Don't change the plugins, except where enemy pooling needs a reset hook (say so first).

## The goal

Today the three enemy types differ mostly in numbers. Part 1 showed the result: chasers can be out-ranged (a player backing off at 60 m takes 2–7% damage, because the chaser's gun reaches 40 m), and every shot from both sides hits, so fights are pure stat checks. This task gives each type its own temperament and one question it asks the player:

| Type | Temperament | The question it asks |
|---|---|---|
| Drifter | Passive until attacked | None. The safe target for beginners and farming. |
| Chaser | Aggressive, closes distance | "Can you handle being rushed?" You can't just back off. |
| Ranged | Aggressive, keeps distance, charged shot | "Are you watching?" Dodge the charged shot with the boost. |

Inspiration: DarkOrbit's passive and aggressive aliens, and the telegraph rule: an enemy shows what it's about to do before the player has to react.

## Protect the core combat style (most important)

The game's combat is **push and pull**: the player locks a target, strafes around it and constantly adjusts the distance, pushing in or pulling back. This task must strengthen that style, never replace it. Every behaviour below is judged by whether it makes that positioning matter more.

Rules:
- **Don't change the player's movement, combat stance, strafing, lock or boost.** If you believe one of them needs a change, propose it in the report instead.
- **Enemies push and pull; they never pin the player down.** The chaser's afterburner comes in bursts with a cooldown, so a player who backs off still gets distance back between bursts. A chaser that stays glued to the player is a failure, even if the numbers are met.
- **Enemies don't body-block or bump the player** while circling or strafing. Keep a minimum distance from the player's ship (about 2–3 ship lengths), and avoid crossing the player's flight path at close range.
- **Distance is always a choice with a trade-off,** never a solved problem: backing off from a chaser costs boost or eats a burst; closing on a ranged enemy makes it retreat; standing still gets hit by charged shots.
- **In Phase 3, check the feel, not only the numbers.** Describe how a fight against each type plays when the test pilot strafes and adjusts its distance: does the rhythm alternate, or does one side get stuck?

## Decisions already made

**No hull repair in flight.** Hull only recovers by docking. This stays; a repair bot may be added later on top of it. Nothing in this task should regenerate hull.

**Drifter:**
- Passive: never starts a fight. It wanders as today.
- When damaged by the player, it fights back until it dies or gives up.
- Gives up (leash) when it hasn't been damaged for 20 s, or when the player is more than 2× its weapon range away for 8 s. Then it goes back to wandering, and its shield (not hull) regenerates normally.
- Other drifters don't join in.

**Chaser:**
- Aggressive: attacks the player on sight within its aggro radius (keep the current radius unless Phase 0 shows a problem).
- Closes the gap: when the player is beyond 1.2× its weapon range, it **afterburns**: about 1.8× its normal speed for up to 3 s, then a 6 s cooldown. Tune so that:
  - afterburning is faster than the player's normal top speed, so backing off doesn't work
  - it's slower than the player's boost, so escaping is possible, but costs boost energy
- Inside weapon range it doesn't sit still: it circles or strafes around the player at its preferred range.
- Leash: gives up after 25 s without dealing or taking damage, or when the player leaves the zone it belongs to by a clear margin.

**Ranged:**
- Aggressive, like the chaser, but holds its preferred range. If the player comes within 40% of that range, it backs away.
- **Charged shot:** every 6–8 s (random within that), while it has line of sight:
  - **Telegraph, 1.2 s:** the ship glows (a material parameter, like the death flash), a charge-up effect hook plays at its weapon, and a sound hook. The player's HUD warns them: the enemy's marker or reticle pulses red, and if the enemy is off-screen, the edge of the screen in its direction pulses red.
  - **The shot:** a slow, visible projectile (not a trace), aimed at where the player is when it fires, with no lead and no homing. Speed is tuned so that at the ranged enemy's preferred range, it takes about 0.6–0.8 s to arrive. A player who boosts sideways during the telegraph or flight dodges it; one who flies straight or sits still gets hit.
  - **Damage:** about 3× a normal ranged shot. Lower the normal shot damage so that the ranged enemy's total damage over a fight stays about the same as today **if every charged shot hits**. Dodging is the reward.
  - The projectile is pooled, has a lifetime, and is destroyed on hitting the player, an asteroid or anything else that blocks shots.
- Its normal shots still hit as today. Only the charged shot can be dodged.

**General:**
- Behaviour numbers live in one place per type (the enemy data assets or `DA_CombatBalance`, whichever fits the existing pattern), not in code.
- Behaviour must survive pooling: a reused enemy starts calm, with no leftover aggro, afterburner, cooldowns or a half-charged shot.
- A dying enemy (the death sequence) stops everything at once, including a charge in progress. A charged shot already in flight keeps flying.
- Effects are hooks, not art, like the death sequence: soft references with defaults in the project settings, empty is fine. `NS_Laser_Hit` can stand in where something must be visible for tests.

## Phase 0: discover, report, then stop

Report, with file and line references:
1. **How enemy AI works today:** Behavior Tree, State Tree or C++, the states, how a target is chosen, aggro radius, preferred range and how movement holds it, and how "passive until hit" is done for drifters today (the Part 1 report says drifters are already passive until hit).
2. **Movement:** enemy speed and acceleration per type, against the player's top speed and boost speed.
3. **Enemy weapons:** how they fire (traces through `UWeaponMountComponent`), and whether anything in the project supports projectiles already.
4. **HUD:** how enemy markers and the reticle are drawn, and whether `M_HUD_EdgeFeedback` (or something else) can show a directional warning.
5. **Leash and reset:** whether enemies ever give up a chase today, and what the pool resets on reuse.
6. **How the player fights today:** the combat stance, strafing and distance control, as the code does it, so the plan can be checked against the push-and-pull style.
7. **Your plan** for each type, including anything in the decisions above that doesn't fit the existing code, and the numbers you'd start with.

Then stop.

## Phase 1: drifter and chaser

- Drifter temperament and leash.
- Chaser aggro, afterburner, circling, and leash.
- Tests: a drifter never attacks first; it fights back when hit and gives up after the leash time; a chaser afterburns when the player is out of range and catches a player flying away at normal top speed; a player boosting away escapes it; a pooled enemy comes back calm.

## Phase 2: ranged enemy and the charged shot

- Ranged temperament (holding range, backing away), the charged shot with telegraph, the projectile and the HUD warning.
- Lower the normal ranged shot damage as described.
- Tests: the telegraph always comes 1.2 s before the shot; a still player is hit; a player moving sideways at boost speed during the telegraph is missed; the projectile is destroyed by an asteroid; a ranged enemy that dies mid-charge never fires; a pooled ranged enemy comes back with no charge.

## Phase 3: measure and tune

- **Test pilot:** add an option to `Startech.Test.AutoFight` that dodges charged shots: when a charge telegraph starts against the player, it boosts sideways. Add an option to back off at a set speed, to test the chaser.
- **TTK table:** add the per-type fire-uptime factor from Part 1's follow-ups, measured from the play sessions, so the table predicts real fights again. The ranged row should show two health-lost values: all charged shots hit, and all dodged.
- **Play sessions,** one per tier with the expected gear, as in Part 1, with and without dodging. Include each `Startech.Combat.Report`.
- **Tune damage** so that live single fights land at:
  - drifter: 5–15% of health (it's the gentle enemy)
  - chaser: 15–25%, including against a player who backs off
  - ranged: about 25–30% when nothing is dodged, about 12–18% when every charged shot is dodged
- Propose any number changes in the report before applying ones that move a Part 1 target; apply ones that are inside the targets.

## Done when

- each type behaves as described, survives pooling and stops cleanly on death
- backing off no longer beats chasers; boosting away still can
- charged shots are readable and dodgeable, and dodging measurably reduces damage
- the TTK table predicts live fights, and live fights meet the targets above
- `CLAUDE.md` describes the behaviours, where their numbers live, and the test pilot's new options

## Follow-ups

- **Chasers against a player who backs off, tiers III–IV** (accepted, information only): 5–8% met in aggro range, 4–5% from max range (tier II: 10.5% and 10.9%). The chaser needs about 2.5 s to arrive, a third to a half of a 5.5–7.5 s kill. Levers if it matters later: measure "backs off after contact" (the chaser has reached the player first), or longer tier III–IV fights (chaser hull and shield).
- **Circling a chaser in tier III** measured 14.7% (two runs: 12.0% and 17.4%) against 20–25%; tier IV about 17–21% (accepted). Information only.
- **Session noise:** the same 90 s segment varies 3–5 points between runs. For a firmer number, run a segment two or three times (`Tools/Combat/run_behaviour_sessions.ps1 -Only`).
- **Boost stays forward-only;** the dodge is changing strafe or throttle. A sideways boost was proposed and declined.
- **HUD projection fix** (Phase 2): every world-projected HUD element was off screen in standalone windows. Fixed in `FHudDraw::ProjectToWidget`; worth a look in a packaged fullscreen build.
- **No hull repair in flight** stays as decided; the test pilot dies after 4–12 kills in a row and now carries on after the respawn.
