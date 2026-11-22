# Combat balance 2 (behaviour): Phase 3 review

This follows up your Phase 3 decisions:
- Accept chaser damage 1.9, charged shot 45, and chaser speeds 2,400 / 2,800 / 3,100 per tier.
- For tier II, try a 45 m gun, or a ×2.0 burn if backing off stays under 10%.
- Re-run the back-off sessions, turn on the back-off target check, and mark the task done if everything is inside the targets.

**Everything is not inside the targets, so the task is not marked done.**
- Engaged chasers, drifters and ranged enemies stay on target.
- Backing off from a chaser now costs **4–15%** depending on tier and how the fight starts. That is a clear improvement (it was about 0% in tiers III–IV), but most cases are still under 10%.
- With the check on, `Startech.Combat.TTKTargets` fails on exactly those rows. The other 28 `Startech` tests pass. The build has no new warnings.

## What was applied
- **Per-tier chaser speed:**
  - `FEnemyBehaviourBalance::ForwardSpeedByTier` (entry 0 = tier I) is used through `GetForwardSpeed(LifeTier)` in `AStartechEnemyShip::ApplyStats`. A pooled chaser reads its spawn tier's speed every life.
  - Chaser values: 0 / 2,400 / 2,800 / 3,100. Afterburning: 4,320 / 5,040 / 5,580, still slower than the boost at the expected Engines level (6,600 / 7,790 / 8,580).
- **Chaser gun 40 → 45 m:** `WEAPON_RANGE` in `Tools/Combat/setup_combat_balance.py` writes `DA_BasicWeapon_Chaser.Range`.
  - It applies in every tier, since the chasers share one weapon.
  - Damage stays 1.9: with the longer gun, engaged tier II fights averaged 22.0% with no trim (23.2% facing, 20.8% circling).
- **The ×2.0 burn was tried and dropped** (results below). The burn stays ×1.8.
- **TTK table:**
  - Chaser uptimes re-measured: 0.65 engaged and 0.19 backing off (the average over all six back-off segments).
  - The back-off target (10–15%) is on.
  - `Startech.Combat.TTKTargets` now also checks the second loss column. It didn't before, which was my oversight in Phase 3; the ranged "dodged" rows pass.
- **`BoostingAwayEscapesAChaser`** now starts the player at 50 m, just outside the new 45 m gun. Before, the 45 m start counted as already caught. It still escapes: the player's closest point was 48 m, and after 10 s it was 317 m away.

## Tier II: 45 m gun against ×2.0 burn
Health lost per chaser kill, same segments and gear (Lasers 2, Hull 1, Shield 1, LL2, Engines 0):

| Chaser setup | Faces it | Circles it | Backs off from max range | Backs off, met in aggro range |
|---|---|---|---|---|
| 40 m gun, ×1.8 (Phase 3) | 23.5% | 21.8% | 6.0% | 6.5% |
| **45 m gun, ×1.8 (kept)** | **23.2%** | **20.8%** | 6.6% | **14.8%** |
| 40 m gun, ×2.0 burn | 23.7% | 20.9% | 5.9% | 8.4% |

The 45 m gun is better on both back-off cases. The ×2.0 burn didn't beat it, so per your instructions I kept the gun and the ×1.8 burn.

## All tiers with the final settings
| Fight | Target | Tier II | Tier III (Engines 3) | Tier IV (Engines 5) |
|---|---|---|---|---|
| Chaser, faces it | 20–25% | **23.2%** | **22.9%** | **23.8%** |
| Chaser, circles it | 20–25% | **20.8%** | 19.6% | 16.7% |
| Chaser, backs off from max range | 10–15% | 6.6% | 4.1% | 4.7% |
| Chaser, backs off, met in aggro range | 10–15% | **14.8%** | 7.0% | 7.5% |
| TTK table (engaged / back off) | | 22.0% / 6.4% | 21.7% / 6.3% | 22.0% / 6.4% |

- **Tiers III–IV improved:** backing off was 0–3% before the per-tier speed and is now 4–7.5%.
- **The cause of what's left:** the player's reverse at the expected Engines level (2,950 / 3,250) is still a little faster than the chaser (2,800 / 3,100), so the gap reopens between burns.
- **Circling in tier IV (16.7%) is under target.** The chaser fires 52% of a fight against a circling player there, against 63–72% elsewhere.
- **Unchanged from Phase 3, on target:** drifters 12.8%; ranged 23.6–30.8% with nothing dodged and 12.4–13.9% dodged.

Raw reports are in `Saved/Logs/BehaviourM-T2.log` (45 m gun), `BehaviourN-T2.log` (×2.0 burn) and `BehaviourO-T3/T4.log`.

## Options to close it (not applied)
1. **Match the chaser's speed to the player's reverse at the expected Engines level:** 2,500 / 2,950 / 3,250 in tiers II / III / IV (afterburning 4,500 / 5,310 / 5,850, still slower than the boost).
   - A player backing off would then hold distance rather than gain it, so the cooldown no longer gives distance back.
   - **I recommend this.** It's the smallest change that addresses why the gap opens.
2. **Combine the 45 m gun with the ×2.0 burn.** This is untested. It shortens the approach, and the gun keeps the chaser firing.
3. **Lower the back-off target to about 5–10%.** Backing off would then "pay off" more than the spec's wording, but it isn't free: 4–15% today.

**The max-range case stays low under any of these:** the player opens fire at 90 m, before the chaser has noticed, and keeps reversing. If you consider that a legitimate reward for spotting a chaser first, the target could apply to the "met in aggro range" case only. That case is 14.8% / 7.0% / 7.5%.

## Files
- **Changed:**
  - `Public/Combat/CombatBalance.h` (`ForwardSpeedByTier`, `GetForwardSpeed`)
  - `Private/StartechEnemyShip.cpp`
  - `Private/Tests/StartechCombatTests.cpp` (second-column check)
  - `Private/Tests/StartechBehaviourTests.cpp` (50 m start)
  - `Tools/Combat/setup_combat_balance.py` (`forward_speed_by_tier`, `WEAPON_RANGE`, uptimes, back-off target)
  - `CLAUDE.md`
- **Content** (by the script):
  - `DA_CombatBalance`
  - `DA_BasicWeapon_Chaser`: range 45 m
