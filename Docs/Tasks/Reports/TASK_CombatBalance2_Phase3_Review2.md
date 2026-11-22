# Combat balance 2 (behaviour): Phase 3 review 2

## Decisions applied

| Decision | Status |
|---|---|
| Chaser speed = the player's reverse at the expected Engines level: 2,500 / 2,950 / 3,250 in tiers II / III / IV | Applied. Afterburning: 4,500 / 5,310 / 5,850, still slower than the boost (6,600 / 7,790 / 8,580). |
| 45 m gun, ×1.8 burn | Kept. |
| Backing off from a chaser met in aggro range: 10–15% | Checked by `Startech.Combat.TTKTargets`, through the table's back-off column (uptime 0.22, measured). |
| Backing off from max range: at least 5% | Checked in play sessions. |
| Tier IV circling at about 17% | Accepted. |

**The task is not marked done.** Tier II is inside every target; tiers III and IV are not, for backing off and for tier III circling.

## Results (health lost per chaser kill)
Same pilot styles and gear as Phase 3. Where a segment was run twice, the average is given with both samples; each segment has 7–14 kills.

| Fight | Target | Tier II | Tier III (Engines 3) | Tier IV (Engines 5) |
|---|---|---|---|---|
| Faces it | 20–25% | **23.2%** | **23.0%** | **23.8%** |
| Circles it | 20–25% (IV ~17% OK) | **20.8%** | 14.7% (12.0, 17.4) | **21.3%** |
| Backs off, met in aggro range | 10–15% | **10.5%** (8.7, 12.3) | 7.4% (6.7, 8.1) | 5.0% |
| Backs off from max range | ≥ 5% | **10.9%** | 4.8% | 4.0% |
| TTK table (engaged / back off) | | 22.0% / 7.5% | 21.7% / 7.3% | 22.0% / 7.4% |

- **Noise:** the same segment can differ by 3–5 points between runs. For example, tier II "met in aggro range" gave 14.8% in review 1 with a slower chaser, then 8.7% and 12.3% here.
- **Logs:** `Saved/Logs/BehaviourP-T2..4.log` and `BehaviourQ2/Q3/Q4-*.log`.

## Why tiers III–IV stay low
Matching the speeds removed the effect I expected to be the cause: the chaser no longer loses ground between burns. What remains is **how long the chaser takes to arrive compared with how long the fight lasts.**
- The player opens fire at 80–90 m and reverses from the first shot.
- At the agreed ×1.8 burn, the chaser closes at about 24 m/s and needs about 2.5 s to reach its 33 m.
- A tier III kill takes 5.5 s, so the chaser is in range for under half the fight.

That is why the measured back-off uptime is 21–32% in tier II but 10–24% in tiers III–IV. Raising the speed by tier keeps the closing speed the same, so the arrival time doesn't shrink.

## Options (not applied)
1. **Accept tiers III–IV as they are.** Backing off pays off a little more as the player upgrades: 5–8% met in aggro range, 4–5% from max range. Then set the targets per tier, with the back-off target only in tier II. I'd need a per-tier target in the table; today one target covers all tiers.
2. **Measure "backs off after contact".** The chaser has reached the player and is fighting before the player starts reversing. My two cases both start the reversal from the first shot, at 80–90 m. I think contact-first is what players do when "rushed", and it would likely land at 10–15%. That would replace the "met in aggro range" case.
3. **Make tier III–IV fights longer.** For example, raise chaser hull and shield in those tiers by about 15%, so the arrival is a smaller share of the fight. That moves their time-to-kill (currently 5.7 s and 7.8 s against 5–7 s and 7.75–10.25 s), so it's a Part 1 number.

**My recommendation:** option 2 first. It tests whether the remaining gap is in the game or in how I measure backing off.

## Tests
- 28 of 29 `Startech` tests pass.
- `Startech.Combat.TTKTargets` fails only on the chaser back-off rows: 7.3–7.5% against 10–15%. It should stay red until you choose an option.
- The build has no new warnings.

## Files
- `Tools/Combat/setup_combat_balance.py`: speeds 2,500 / 2,950 / 3,250, back-off uptime 0.22.
- `CLAUDE.md`: chaser speeds, and which back-off case the table models.
- **Content** (by the script): `DA_CombatBalance`.
