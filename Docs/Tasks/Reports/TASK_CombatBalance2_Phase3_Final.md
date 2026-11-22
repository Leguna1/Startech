# Combat balance 2 (behaviour): final

**Decision applied:** option 2 from review 2. Tiers III–IV are accepted as they are, and no more sessions were run. `Startech.Combat.TTKTargets` now fails only where you asked, and shows the chaser's back-off value for information. All 29 `Startech` automation tests pass, the build has no new warnings, and **`TASK_CombatBalance_2_Behaviour.md` is marked done**.

## What `Startech.Combat.TTKTargets` checks now

| Value | Fails the test? |
|---|---|
| Time to kill (every row with a target) | Yes |
| One-fight loss: the player facing the enemy (drifter, chaser); ranged with every charged shot hit | Yes |
| Ranged with every charged shot dodged | Yes |
| Ammo cost against the kill's value (the Part 1 profit rule) | Yes, kept (see note) |
| Chaser against a player backing off | **No**: printed as information with its aim (10–15%) |
| Chaser circled by the player | No table value; the session numbers are in the task's follow-ups |
| Table errors (missing data, a preferred range beyond the gun) | Yes |

- **How the information-only value works:** a new `FEnemyBalance::bLossAltTargetInfoOnly` flag, set for the chaser through the script's `TABLE` dict (`alt_info_only=True`). The table still shows the value and its aim, tagged "(info)", but never marks it outside or counts it. The test logs it as an info line:
  ```
  tier 2 Chaser with LL2 L2: back off costs 7.5% of health (information; aim 10-15%)
  tier 3 Chaser with LL3 L3: back off costs 7.3% of health (information; aim 10-15%)
  tier 4 Chaser with LL3 L5: back off costs 7.4% of health (information; aim 10-15%)
  ```
- **Note on ammo cost:** your list didn't mention the ammo-cost check. I kept it, because it guards Part 1's "ammo is profitable" rule and currently passes. Say if you want it to be information too.

## Final numbers

**Live play sessions** (health lost per kill; expected gear; Engines 3 / 5 in tiers III / IV):

| Fight | Target | Tier I | Tier II | Tier III | Tier IV |
|---|---|---|---|---|---|
| Drifter | 5–15% | 12.8% | – | – | – |
| Chaser, faces it | 20–25% | – | 23.2% | 23.0% | 23.8% |
| Chaser, circles it | info | – | 20.8% | 14.7% | 21.3% |
| Chaser, backs off met in aggro range | info (aim 10–15%) | – | 10.5% | 7.4% | 5.0% |
| Chaser, backs off from max range | info (aim ≥ 5%) | – | 10.9% | 4.8% | 4.0% |
| Ranged, nothing dodged | 25–30% | – | 30.8% | 25.1% | 23.6% |
| Ranged, every charged shot dodged | 12–18% | – | 13.9% | 12.4% | 13.1% |

The TTK table has no value outside a target (it's printed in `TASK_CombatBalance2_Phase3_Review2.md`; only the "(info)" tags are new).

**Final enemy numbers** (`Tools/Combat/setup_combat_balance.py`):

| Type | Numbers |
|---|---|
| Drifter | Passive. Leash 20 s without damage, or the player beyond 150 m for 8 s. Fire uptime 0.61. |
| Chaser | Speed 2,500 / 2,950 / 3,250 by tier, ×1.8 burn (up to 4.5 s, 4 s cooldown, starting at gun range). 45 m gun, damage 1.9, preferred range 33 m. Leash 25 s quiet or 300 m from home. Uptime 0.65 engaged, 0.22 backing off. |
| Ranged | Normal shot 0.7. Retreats inside 36 m. Charged shot 45 damage, 1.2 s telegraph, 12,500 cm/s; first at 2–3 s, then every 6–8 s. Uptime 0.77. |

## Also done
- **`CLAUDE.md`:**
  - What the TTK test checks and what is information only.
  - The chaser's final numbers.
  - The session scripts and the 3–5 point run-to-run noise.
- **Session scripts kept:** `Tools/Combat/run_behaviour_sessions.ps1` and `parse_behaviour_sessions.ps1` (moved from my scratch folder; now relative to the project).
- **Follow-ups:**
  - `TASK_CombatBalance_2_Behaviour.md` has a status line and a Follow-ups section: back-off in tiers III–IV, tier III circling, session noise, the boost decision, the HUD projection fix, and no hull repair.
  - `TASK_CombatBalance_1_Numbers.md`: three of its four follow-ups are marked done (chaser out-ranging, fire uptime, pilot undocking). No hull repair in flight stays open.

## Files
- **Changed:**
  - `Public/Combat/CombatBalance.h` (`bLossAltTargetInfoOnly`)
  - `Public|Private/Combat/CombatTTKTable.*` (info-only targets)
  - `Private/Tests/StartechCombatTests.cpp` (which values fail, information lines)
  - `Tools/Combat/setup_combat_balance.py`
  - `CLAUDE.md`
  - `Docs/Tasks/TASK_CombatBalance_2_Behaviour.md`, `Docs/Tasks/TASK_CombatBalance_1_Numbers.md`
- **New:** `Tools/Combat/run_behaviour_sessions.ps1`, `Tools/Combat/parse_behaviour_sessions.ps1`
- **Content** (by the script): `DA_CombatBalance`
