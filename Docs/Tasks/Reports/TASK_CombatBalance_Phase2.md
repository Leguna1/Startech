# Combat balance, Phase 2: the numbers applied

Task: `Docs/Tasks/TASK_CombatBalance_1_Numbers.md`. Phase 1 (ammo progression) was accepted. This report covers Phase 2 and the LL4 question.

## The LL4 question

LL4 drops only from tier III and IV enemies.
- `DA_WorldLoot` is the only loot table in the project. Its ammo list is empty (Phase 1 removed LL1), and it holds only the three minerals.
- Both the enemies and `BP_ZoneLoot` use that table, so world pickups drop credits and minerals only.
- LL4 comes only from `UCombatBalance::RollTierAmmoDrops`, which `AStartechEnemyShip::DropLoot` calls with the enemy's tier (15% at tier III, 25% at tier IV, 20–60 rounds).

Nothing needed moving. I checked by listing every `ULootTable` asset and what references it.

## Result

`Startech.Combat.TTKTable` on the live data, run headless. Both sides hit every shot, and the enemy fires for the whole fight.
- **TTK:** time to kill, in seconds on target.
- **Lose1:** share of the expected health lost fighting one enemy. **Lose3:** fighting three at once, killed one after another (about 6× one fight).
- **Kill cr:** average credits plus the sell value of the average loot at the minerals shop.
- **Cost%:** ammo cost as a share of the kill's value.
- **Targets:** the allowed range for that row's role. Tough rows are allowed 2× the targets, and weak rows have none.

```
Tier Enemy    Role     Gear      TTK      Lose1    Lose3  Rounds  Ammo cr  Kill cr  Cost%    Targets
1    Drifter   regular  LL1 L0    4.38s    23.3%      140%      35      0.0     36.8  0.0%     TTK 3.00-5.00 s, loss 20-25%
1    Drifter   regular  LL1 L1    3.91s    20.8%      125%      32      0.0     36.8  0.0%     TTK 3.00-5.00 s, loss 20-25%
2    Drifter   weak     LL2 L2    2.82s    8.4%        50%      23     10.4     67.5  15.3%    -
2    Chaser    regular  LL2 L2    6.00s    22.3%      134%      49     22.1    135.0  16.3%    TTK 4.00-6.00 s, loss 20-25%
2    Ranged    tough    LL2 L2    8.06s    33.5%      201%      65     29.3    202.5  14.4%    TTK 0.00-12.00 s, loss 0-50%
3    Drifter   weak     LL3 L3    2.68s    8.2%        49%      22     19.8    126.1  15.7%    -
3    Chaser    regular  LL3 L3    5.70s    21.9%      132%      46     41.4    252.2  16.4%    TTK 5.00-7.00 s, loss 20-25%
3    Ranged    tough    LL3 L3    7.66s    33.0%      198%      62     55.8    378.3  14.7%    TTK 0.00-14.00 s, loss 0-50%
4    Drifter   weak     LL3 L5    3.65s    8.4%        50%      30     27.0    243.0  11.1%    -
4    Drifter   weak     LL4 L5    2.73s    6.3%        38%      22      0.0    243.0  0.0%     -
4    Chaser    regular  LL3 L5    7.75s    22.2%      133%      62     55.8    486.0  11.5%    TTK 7.75-10.25 s, loss 20-25%
4    Chaser    regular  LL4 L5    5.81s    16.7%      100%      47      0.0    486.0  0.0%     TTK 5.75-8.25 s
4    Ranged    tough    LL3 L5    10.42s   33.5%      201%      84     75.6    729.1  10.4%    TTK 0.00-20.25 s, loss 0-50%
4    Ranged    tough    LL4 L5    7.81s    25.1%      151%      63      0.0    729.1  0.0%     TTK 0.00-16.25 s
14 row(s), 0 outside a target ('*').
```

- **Time to kill:** every regular enemy is inside its target. Tier IV chasers are 0.25 s under (see deviation 2).
- **Health lost:** one regular fight costs 20.8–23.3% at every tier, the low end you asked for. Three at once costs 125–140%, so it is lethal without retreating or draining.
- **Money:** ammo costs 10–16% of a kill's value at every tier, against the 30% rule. Kill value grows 37 → 135 → 252 → 486 credits from tier I to IV for regular enemies, so the outer sectors pay more per kill.
- **Tests:** `Startech.Combat.TTKTargets` passes. It fails on any marked value, on any table error, and on a preferred range the gun can't reach. The other 12 tests also pass: Combat, Shop, Save, Tiers and Flow.

## Numbers applied (all in `DA_CombatBalance`, from `Tools/Combat/setup_combat_balance.py`)

| | Drifter | Chaser | Ranged |
|---|---|---|---|
| Hull / shield | 150 / 200 | **372 / 372** | 500 / 500 |
| Weapon | `DA_BasicWeapon_Drifter` (75 m) | `DA_BasicWeapon_Chaser` (40 m) | `DA_BasicWeapon_Ranger` (100 m) |
| Damage per shot (before tier) | **1.0** | **1.25** | **1.4** |
| Preferred range | **6,500** | **3,600** | **9,000** |
| Reward multiplier | ×1 | ×2 | ×3 |
| Role by tier | I regular, II–IV weak | II–IV regular | II–IV tough |

- **Ammo:** LL1 ×1 (free), LL2 ×2 at 0.30 base (0.45 cr a round), LL3 ×3 at 0.60 (0.90 cr), LL4 ×4 (never sold).
- **Expected gear:**

| Tier | Gear | Hull / Shield levels | Expected health |
|---|---|---|---|
| I | LL1, Lasers L0 and L1 | 0 / 0 | 150 |
| II | LL2, L2 | 1 / 1 | 350 |
| III | LL3, L3 | 3 / 2 | 650 (middle of 550–750) |
| IV | LL3, L5, and LL4, L5 (time-to-kill target only) | 5 / 5 | 1,150 |

- **Tier damage multipliers** (`DefaultGame.ini` TierData): **tier III ×1.7 → ×2.5, tier IV ×2.2 → ×3.3.** Hull and shield multipliers are unchanged.

## Deviations from the approved numbers. Please review

1. **Tier III and IV enemy damage multipliers raised** (×1.7 → ×2.5, ×2.2 → ×3.3). You preferred per-enemy changes, but no single damage per shot meets 20–25% at every tier.
   - Expected health grows ×1.86 from tier II to III (350 → 650), but enemy damage grows only ×1.31 (1.3 → 1.7).
   - Tuned to tier II, chasers cost 15% at tier III and 15% (LL3) / 11% (LL4) at tier IV. Tuned to tier III, they cost 31% at tier II.
   - The only per-enemy fix is a damage value for every enemy in every tier, which is the same change split three ways.
   - If you'd rather keep the tier multipliers, I'll add per-tier damage to `FEnemyTierRole` instead.
2. **Chaser 372 / 372, not 375 / 375.** At 375, tier II takes 6.05 s, outside 4–6 s, and the test would fail.
   - At 372, tier II is 6.00 s, but tier IV is 0.25 s under its targets (7.75 s with LL3, 5.81 s with LL4), not the 0.2 s you accepted.
   - No chaser value meets both: tier II needs ≤ 372, and tier IV within 0.2 s needs ≥ 374.4.
   - The tier IV gear cases carry a 0.25 s tolerance (`ToleranceSeconds`).
3. **Preferred ranges are 6,500 / 3,600 / 9,000, not 7,000 / 4,000 / 10,000.**
   - The brain holds still anywhere within ±10% of the preferred range (`UEnemyBrainComponent::RangeTolerance`).
   - At 10,000 a ranged enemy settles as far out as 110 m, beyond its 100 m gun. The chaser (4,000 → 44 m against 40 m) and a 7,000 drifter (77 m against 75 m) would sit just outside their guns too, so none of them would fire.
   - With these values the far edge is inside the gun: 71.5, 39.6 and 99 m. The TTK table reports any future mismatch as an error.
4. **Damage per shot** is 1.0 / 1.25 / 1.4, down from the 1.2 / 1.8 / 2.0 starting values, to land at the low end of 20–25%.

## What changed in code

- **`UCombatBalance`** (`Combat/CombatBalance.h/.cpp`) gained:
  - `Ammo` (`FAmmoBalance`) and `Enemies` (`FEnemyBalance` with per-tier roles).
  - `GearCases` (`FCombatGearCase`).
  - The targets (`OneFightLossMin/Max`, `ToughFactor`, `MaxAmmoCostShare`), the upgrade track ids, the player ship class, `AmmoShop` and `LootShop`.
  - The static readers `GetDamageMultiplier` and `GetBaseValue`.
- **The runtime reads the balance:**
  - `ULaserMountComponent::Fire` (ammo damage), `UShopData` prices and the shop's detail line.
  - `AStartechEnemyShip::ApplyStats`: hull, shield, laser, damage per shot, preferred range and reward. It also adds `GetLaserWeapon`, `GetPreferredRange` and `GetRewardMultiplier`.
  - `DropLoot`: the reward multiplier on credits and loot amounts.
  - `UEnemyBrainComponent`: the ship's preferred range and laser.
  - An enemy type missing from the balance falls back to its `UEnemyData`, with a warning logged once per type.
- **TTK table:**
  - `FCombatTTKTable` (`Combat/CombatTTKTable.h/.cpp`) builds the table from the balance, the tier data, the new-game lasers, the upgrade catalog, `BP_ShipPawn`'s health component and the enemies' loot tables.
  - The `Startech.Combat.TTKTable` command is in `CombatConsoleCommands.cpp`.
  - `UStartechTierSubsystem::FindTierData` (static) lets the table read tiers without a world.
  - `UEnemyBrainComponent::GetRangeTolerance` lets the table check ranges.
- **Content script:** `balance` writes everything above. New `ammo` and `enemies` steps copy the balance into the ammo assets, `DA_Enemy_*` and the enemy weapons, so the editor shows what the game uses.
- **Smaller changes:**
  - Phase 1's `RollTierAmmoDrops` local was renamed (it shadowed the new `Ammo` member).
  - `ShopScreenWidget.cpp` and `StartechEnemyShip.cpp` gained includes the unity build had been supplying.
- **Docs:** `CLAUDE.md` describes the balance asset, its runtime readers, the preferred-range rule, the TTK table and how to run it headless.

## How to check a number change

```
UnrealEditor-Cmd.exe Startech.uproject -run=pythonscript -script="<Project>/Tools/Combat/setup_combat_balance.py" -unattended -nopause -nosplash -stdout
UnrealEditor-Cmd.exe Startech.uproject -ExecCmds="Startech.Combat.TTKTable, QUIT_EDITOR" -unattended -nullrhi -stdout
```

The second command takes about 20 seconds. In the editor, `-ExecCmds` splits commands on commas, and `QUIT_EDITOR` is what exits.

## Checked in a real game

On the test slot, after warping to Rust:
- Drifters had hull 240 and shield 320 (150 / 200 × 1.6), and chasers 595.2 / 595.2 (372 × 1.6).
- The nearest chaser's laser scale was ×1.30. The weapon now holds 1.25, so a shot does 1.625 = 1.25 × the tier II multiplier, as the table assumes.

Not checked yet:
- Hit rates and real fight times. They need Phase 3's combat log and the per-tier play sessions.
- A live check that drifters and ranged enemies now fire, which needs a windowed play session with the player in their range. The table's range check confirms the numbers allow it.

## Still to do (Phase 3)

`UCombatTelemetrySubsystem`, `Startech.Combat.Report`, the CSV in `Saved/CombatLogs/`, and one play session per tier with its report.
