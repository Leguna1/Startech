# Task: ship upgrades (Upgrades tab) and a limited boost

> **Status: done (2026-11-22).** Phases 0-3 accepted. Boost: `Source/Startech/*/Boost/`. Upgrades: `Source/Startech/*/Upgrades/`
> and `UI/Hub/HubUpgradesTabWidget`, `UpgradeRowWidget`. Content: `Tools/Ship/create_boost_input.py`,
> `Tools/Upgrades/setup_upgrades.py`. See the Boost and Upgrades sections of `CLAUDE.md`.
> Decisions that differ from the text below: player base hull 100 and shield 50, Hull plating and Shield capacity +100
> flat per level; the shop stops selling Cobaltine and Aurium (still buys them); Terra bounties come from
> `DA_BountyTemplate_Terra`; a purchase while docked also fills hull and shield to the new maximum.

Read Startech's root `CLAUDE.md` first. The hub, HUD, action bar, save and sector tier tasks are done. Reuse `DA_HudStyle`, the hub widgets and helpers, the item catalog, the save participant pattern, and the tier subsystem. Don't change the plugins.

## Decisions already made

- **Upgrades are tracks:** fixed ship systems levelled from 0 to 5. Equipment modules and new ships come later; nothing here may block them.
- **Costs are credits plus minerals.** Higher levels need rarer minerals: Ferrite for levels 1–2, Cobaltine for 3–4, Aurium for 5. That ties progression to the outer sectors.
- **The shop keeps selling minerals as it does today.**
- **Upgrades are permanent:** kept on death and saved.
- **New boost mechanic:** while Left Shift is held, the ship speeds up beyond its normal top speed, faster and faster for a few moments, using a boost energy pool. When the pool runs out, the boost stops, and the pool regenerates after a short delay. The boost has its own upgrade track.

## Phase 0: discover, report, then stop

Report, with file and line references:
1. **Boost today:** the Left Shift Blueprint event in `BP_ShipPawn` and any C++ boost (what it does, and whether it's limited).
2. **Movement:** top speed, acceleration, and how they're applied (movement component, custom code).
3. **Where each stat lives and how it's read:**
   - max hull and max shield
   - shield regeneration
   - cargo capacity
   - laser damage and drainer power, per weapon group
4. **The economy today:**
   - typical credits per kill and per loot pickup, per tier
   - mineral sell prices, and mineral buy prices in the shop
   - ammo prices
5. **Camera:** the field of view and camera lag settings (for boost feel).

Then stop.

## Phase 1: boost

**Input:** `IA_Boost` (a hold action) on Left Shift in `IMC_Ship`, created by script. Remove the old Blueprint key event from `BP_ShipPawn`; that's a manual step, so give exact instructions. Ignore the input while the hub or pause menu is open, while warping, and while dead.

**`UShipBoostComponent`** on the player ship:
- **Pool:** boost energy from 0 to `MaxBoostEnergy`, default 100. It starts full and is refilled on respawn and on load.
- **While held with energy left:**
  - energy drains at 25 per second, so a full pool lasts 4 s at level 0
  - a speed multiplier ramps from 1.0 to 2.2 over 1.5 s, which is "faster and faster"
  - top speed and acceleration are multiplied by it
- **On release, or when energy runs out:** the multiplier falls back to 1.0 over 0.8 s.
- **When empty:** boosting can't start again until energy is back above 25%, so there's no stuttering on a near-empty pool.
- **Regeneration:** 1.5 s after boosting stops, energy regenerates at 20 per second.
- **Feel:** while boosting, the camera field of view widens by up to 8° with the multiplier, and the camera lag increases slightly. Both ease back.
- **Settings:** all numbers live in `UStartechSettings`.
- **Events:** `OnBoostEnergyChanged(Current, Max)`, fired at most 10 times a second, and `OnBoostStateChanged(bBoosting)`.

**HUD:** the speed readout widget gains a small boost gauge under the speed number:
- a 10-segment bar in the Player colour
- the segments turn amber while the pool is below the restart threshold
- one short red flash when Shift is pressed with too little energy

Event-driven, like the rest of the HUD.

## Phase 2: upgrade tracks

**Data:** `DA_UpgradeCatalog` (`UUpgradeCatalog`), a list of tracks. Each track has:
- an id (`FName`), display name and short description
- the stat it changes, as a gameplay tag such as `Stat.Hull.Max`
- the bonus per level, five values for levels 1–5
- the cost per level: credits, plus a list of `{mineral tag, count}`

**Tracks, with starting bonus values to tune later:**

| Track | Stat | Per level | At level 5 |
|---|---|---|---|
| Hull plating | max hull | +20% | ×2.0 |
| Shield capacity | max shield | +20% | ×2.0 |
| Shield recharge | shield regeneration | +15% | ×1.75 |
| Cargo hold | cargo capacity | +20 units | +100 |
| Engines | top speed and acceleration | +6% | ×1.3 |
| Lasers | laser damage | +12% | ×1.6 |
| Drainer | drainer power | +12% | ×1.6 |
| Booster | max boost energy and regeneration | +20% and +15% | ×2.0 and ×1.75 |

**Cost scheme**, the same for every track to start with. `UpgradeCostScale` is a settings multiplier for the credits:

| Level | Credits | Minerals |
|---|---|---|
| 1 | 2,000 | 20 Ferrite |
| 2 | 5,000 | 50 Ferrite |
| 3 | 12,000 | 25 Cobaltine |
| 4 | 25,000 | 60 Cobaltine |
| 5 | 50,000 | 30 Aurium |

**Calibration:** using the economy numbers from Phase 0, check the credit costs against these targets, and propose a value for `UpgradeCostScale` that meets them:
- level 1 of a track is affordable after about 10 minutes in Terra
- level 3 after a few Rust trips
- level 5 only after several trips to Glacier and Jove

Report the reasoning.

**Mineral buying:** report each mineral's buy and sell price. If buying minerals is cheaper than earning them (for example Aurium cheap enough that level 5 can be bought with Terra credits), say so. Don't change shop prices without my approval.

**Runtime:**
- **`UShipUpgradeSubsystem`** (game instance) holds a level per track. It provides:
  - `GetLevel`
  - `CanUpgrade(TrackId, OutReason)`: max level reached, not docked, not enough credits, or not enough of a mineral (naming it and the amount missing)
  - `Upgrade(TrackId)`: takes credits and minerals from the wallet and cargo, raises the level, fires `OnUpgradeChanged`
  - `GetStatMultiplier(StatTag)` and `GetStatBonus(StatTag)`
- **The ship's systems read their stats through it,** and refresh on `OnUpgradeChanged`, on load and on respawn:
  - health and shields
  - shield regeneration
  - cargo capacity
  - movement
  - weapon mounts per group
  - boost
- **Only the player is affected,** never enemies, even though they share the ship base class.
- **Raising max hull or max shield** raises the current value by the same amount, so an upgrade never feels like losing health.
- **Save:** a new participant, `Startech.Upgrades` (track ids and levels). Add it to `ExpectedParticipantIds`. Old saves start at level 0.

## Phase 3: Upgrades tab

Enable the hub's **Upgrades** tab; it's no longer greyed out. Same style as the Quests tab.

- **One row per track:**
  - name and short description
  - 5 level pips (filled in the Player colour)
  - the change at the next level, for example "Hull 2,400 → 2,900" or "Cargo 100 → 120"
  - the cost: credits in the Credits colour, and minerals as "25 Cobaltine (12)" with the amount you have in brackets. Each part is green when you have enough and red when you don't.
  - an **Upgrade** button, which shows the `CanUpgrade` reason when disabled. At level 5, the row shows "Max level" instead.
- **Upgrading:**
  - the top bar's credits and cargo update
  - the row's pips and values update
  - a notice in the HUD's notice area after leaving: "Hull plating upgraded to level 3"
- **Updates:** event-driven, as everywhere else.

## Conventions

The same as before: no `TMap`, const correctness, logic in C++, doc comments, Blueprint only for layout.

## Acceptance

1. Builds cleanly. Old saves load with every track at 0.
2. Summary:
   - the economy numbers from Phase 0
   - the chosen `UpgradeCostScale`, with the reasoning
   - the mineral price check
   - every changed file
   - the manual steps (the Left Shift event, any widget layout)
3. Test plan (add `Startech.Test.Upgrades` to log every track's level and the resulting stats, and `Startech.Test.SetUpgrade <Track> <Level>`):
   - **Boost:**
     - hold Shift: speed climbs to about 2.2× over 1.5 s, the field of view widens, and the gauge drains over about 4 s
     - release: it eases back
     - empty: boosting can't restart until the pool is above 25% (amber gauge, red flash on pressing)
     - regeneration starts 1.5 s after stopping
     - Shift does nothing in the hub, the pause menu or a warp
   - **Upgrade buying:**
     - buy Hull plating level 1 with enough credits and Ferrite: credits and Ferrite are taken and max hull rises 20%, the current hull too
     - without enough Ferrite: the button shows "Need 12 more Ferrite"
     - undocked (via the test command): "Dock to upgrade"
   - **Each track changes its stat,** checked with `Startech.Test.Upgrades`. Cargo capacity shows in the top bar; engine and booster levels change the boost and normal speeds.
   - **Enemies are unaffected:** a Rust chaser's hull is the same before and after maxing Hull plating.
   - **Save and continue:** levels are kept, and stats are applied on load and after respawning.
