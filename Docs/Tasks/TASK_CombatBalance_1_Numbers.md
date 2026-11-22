# Task: combat balance, part 1 (the numbers)

**Status: done (2026-11-22).** Reports: `Reports/TASK_CombatBalance_Phase2.md` and `Reports/TASK_CombatBalance_Phase3.md`. Open items are under [Follow-ups](#follow-ups).

Read Startech's root `CLAUDE.md` first. Do `TASK_Shop_Polish.md` before this one: the new ammo is bought in bulk. The upgrade tracks, sector tiers, action bar, shop and save tasks are done. Don't change the plugins.

This task is only about numbers: ammo progression, prices, time to kill, enemy damage, and a combat log to measure them. Enemy behaviour (passive and aggressive enemies, telegraphed shots) and kill feedback (hit flash, death spin, screen shake) are later tasks. Don't start them here.

## The goal

Today the player is stuck on LL1 ammo, so outer-sector enemies take far too long to kill (about 50 s for a Jove ranged enemy at 80 DPS), and the ammo costs more than the kills pay. The fix follows DarkOrbit: **each tier has a matching ammo tier**, so time to kill stays roughly flat as the player progresses, and the Upgrades tab makes it a bit faster on top.

## Decisions already made

**Ammo progression:**

| Ammo | Damage | Where it comes from |
|---|---|---|
| LL1 | ×1 | Free and unlimited. Never runs out, isn't sold, shown as ∞ on the action bar. |
| LL2 | ×2 | Sold at the station from the start |
| LL3 | ×3 | Sold once the player has reached tier III (Glacier) |
| LL4 | ×4 | Never sold. Dropped by tier III and IV enemies only |

- LL1 being free is a safety net: the death penalty is harsh, so a broke player must always be able to fight.
- The LL3 unlock uses the highest tier the player has ever reached. If that isn't tracked yet, add it to the sector tier save data (bump its save version) and keep it on death.
- The shop's row for LL3 shows as locked before the unlock, with the reason "Reach Glacier (tier III)".
- The drainer (SD1, SD2) is left as it is in this task.

**Time-to-kill targets.** "Expected gear" is what a player should have by the time they fight there. Time to kill is for one regular enemy, both laser mounts firing, counting only time on target.

| Tier | Expected gear | Target time to kill |
|---|---|---|
| I Terra | LL1, Lasers L0–1 | 3–5 s |
| II Rust | LL2, Lasers L2 | 4–6 s |
| III Glacier | LL3, Lasers L3 | 5–7 s |
| IV Jove | LL3, Lasers L5 (8–10 s with LL3; 6–8 s with LL4) | 6–10 s |

Tougher enemy types may take up to twice the target; none should take longer.

Tier IV chasers may die up to 0.2 s under the tier IV targets (7.8 s with LL3, 5.9 s with LL4): no single chaser toughness fits both tier II and tier IV, and the tier IV multiplier stays at ×4.0.

**Enemy damage targets.** Expected player health (hull + shield) per tier, with the base now 100 hull and 50 shield and +100 per upgrade level:

| Tier | Expected Hull / Shield levels | Expected health |
|---|---|---|
| I | 0 / 0 | 150 |
| II | 1 / 1 | 350 |
| III | 2–3 / 2–3 | 550–750 |
| IV | 5 / 5 | 1,150 |

- Fighting **one** regular enemy at the expected gear should cost the player about **20–25%** of their health (aim for the low end).
- Fighting **three at once** is meant to be **lethal** unless the player retreats or uses the drainer. Killed one after another, three cost about 6× one fight, so there is no upper target for it; retreating or draining must make it survivable.

**The money rule:** the ammo spent on a kill at the intended tier must cost no more than **30%** of what the kill pays (credits plus the sell value of its typical loot). The rest is profit, so the outer sectors pay better per minute than the inner ones, not worse.

**All balance numbers in one place:** a `DA_CombatBalance` data asset (or `UStartechSettings` if that's the existing pattern) holding the targets above, ammo multipliers and prices, and the LL4 drop chances. Nothing balance-related is hard-coded.

## Approved after Phase 0

| Change | Value |
|---|---|
| LL1 | Free; removed from the shop, from `DA_WorldLoot` and from the new-game stock |
| LL2 price | 0.45 cr a round (base value 0.30) |
| LL3 price | 0.90 cr a round (base value 0.60), locked until tier III |
| LL4 drops | Tier III 15%, tier IV 25%, 20–60 rounds |
| New game | LL1 only plus a small LL2 stock; no LL3 or LL4 |
| Chaser hull / shield | 375 / 375 (from 300 / 300) |
| Enemy damage per shot | Own weapon asset per type; starting at Drifter 1.2, Chaser 1.8, Ranged 2.0, tuned down in Phase 2 to the 20–25% target |
| Enemy ranges | Drifter preferred range 7,000 with `DA_BasicWeapon_Drifter` (75 m); Ranged keeps 10,000 with `DA_BasicWeapon_Ranger` (100 m) |
| Kill rewards by type | In `DA_CombatBalance`: Drifter ×1.0, Chaser ×2.0, Ranged ×3.0 on credits and loot amounts. Loot tables still decide which items drop. |
| Tier multipliers | Unchanged (tier IV hull and shield stays ×4.0) |

## Phase 0: discover, report, then stop

Report, with file and line references:
1. **Ammo:** the definitions of LL1–LL4 (ids, damage multipliers, prices, where they're stored), how one shot consumes ammo (one round per mount per shot, or per volley?), and what happens today when the selected ammo runs out.
2. **Player damage:** base damage per shot, fire rate, mounts, range, and how the Lasers and Drainer upgrade multipliers are applied (confirm Lasers is +12% per level, ×1.6 at level 5).
3. **Every enemy type:** tier, hull, shield, shield regeneration, damage per shot, fire rate, range, and the resulting damage per second, after the tier multipliers.
4. **Kill rewards:** credits per kill per enemy type and tier, and the typical loot with its sell value.
5. **Whether "highest tier reached" is tracked and saved.**
6. **A computed table**, from those numbers, for each enemy type at its tier with the expected gear:
   - time to kill
   - player health lost fighting one, and fighting three
   - rounds used per kill and their cost
   - ammo cost as a percentage of the kill's value
   Mark every value outside the targets.
7. **Your proposed changes** to bring them inside the targets: LL2 and LL3 prices (per round; fractional is fine, the shop rounds totals), and any enemy hull, shield or damage changes. Prefer changing ammo prices and per-enemy values over changing the tier multipliers.

Then stop. I'll review the proposed numbers before they go in.

## Phase 1: ammo progression

- LL1 free and unlimited: no consumption, ∞ on the action bar, removed from the shop's sell list. Existing saved LL1 counts are ignored; don't refund them.
- LL2 and LL3 in the shop at the approved prices, LL3 locked until tier III is reached.
- LL4 drops: added to tier III and IV enemy loot with the approved chances and amounts, as a normal pickup.
- When the selected ammo runs out mid-fight, the weapon switches to LL1 automatically and the action bar shows it, with a short notice "LL2 empty, switched to LL1". No firing gap.
- Save: the unlock and any new data saved, with version bumps where needed.

## Phase 2: apply the approved numbers

- Move every balance number into `DA_CombatBalance` (or the settings), and apply the approved changes to ammo prices and enemy values.
- **`Startech.Combat.TTKTable`** console command: prints the Phase 0 table (6) from the live data, so any balance change can be checked in seconds without playing. Values outside the targets are marked.
- Make it runnable headless, so you can run it after any number change and report the table.

## Phase 3: the combat log

`UCombatTelemetrySubsystem` records each **engagement**: from the first shot fired at an enemy, or the first damage taken from it, until it dies or 15 s pass with neither.

For each engagement, record:
- enemy type and tier, the ammo and Laser level used
- time to kill (time on target, and total time)
- shots fired and hit, rounds used by ammo type and their cost
- damage taken, split into shield and hull
- credits and loot value gained, and net profit (gained minus ammo cost)
- whether the player died

Outputs:
- `Startech.Combat.Report` prints a summary of the current session per enemy type: average time to kill, average damage taken, average profit per kill, credits per minute.
- Each session also writes a CSV to `Saved/CombatLogs/`.
- Cheap enough to leave on in development builds; off in shipping builds.

## Tests

- automation tests for LL1 never consuming, the LL3 lock and unlock, auto-switch to LL1 on empty, and LL4 never appearing in the shop
- an automation test that runs the TTK table and fails if any regular enemy is outside its time-to-kill target with the expected gear
- one real play session per tier with the test commands (give yourself the expected gear and upgrades on test slot 3; overwriting it is fine), then `Startech.Combat.Report`. Include the reports.

## Done when

- each tier's regular enemies meet the time-to-kill and damage targets with the expected gear, shown by the TTK table and the play sessions
- ammo is profitable at every tier under the 30% rule
- the combat log works, and `CLAUDE.md` describes `DA_CombatBalance`, the TTK table command and the combat log

## Follow-ups

Accepted after Phase 3: the numbers stay as they are (option (a)). The TTK table's health-loss column is a worst case, which ranged enemies match in play (21–30% per kill) and drifters and chasers don't (2–12%), because they don't fire for the whole fight.

- **Done (combat balance 2):** chasers afterburn to close the gap and circle at 33 m with a 45 m gun; engaged fights cost 21–24%.
- ~~**Chasers can be out-ranged**~~ (enemy-behaviour task). A chaser's laser reaches 40 m, so a player fighting from 60 m takes almost no fire. Fought at 30 m, a chaser kill cost 12.1% of health on average, against the 20–25% target. Fix it in the enemy behaviour (closing in, keeping the player inside its range), not with damage numbers.
- **Done (combat balance 2):** `FEnemyBalance::FireUptime` / `FireUptimeAlt`, measured in play (drifter 0.61, chaser 0.65, ranged 0.77).
- ~~**Fire-uptime factor in the TTK table**~~ (after the enemy-behaviour task). Re-measure each type's share of the fight spent firing with the combat log and the test pilot, then add it per type to `FEnemyBalance` (the Phase 3 estimates were about 0.55 chaser and 0.4 drifter) so the health-loss column predicts play. Only then reconsider damage per shot.
- **Still open: no hull repair in flight.** (Confirmed as intended for now in combat balance 2; a repair bot may come later.) Hull doesn't regenerate, so the test pilot died after 7–19 kills in a row in most sessions. Decide whether that is the intended limit on a sortie (return to dock to repair) or whether hull needs some repair in flight.
- **Done (combat balance 2):** the pilot waits for the respawn, undocks and warps back to the fight's sector.
- ~~**The test pilot doesn't undock after a respawn.**~~ After a death the player respawns docked and `Startech.Test.AutoFight` stays idle, which left the second half of the tier I session empty. Make the pilot undock (and warp back to the session's sector) on respawn, so a session keeps fighting for its full length.
