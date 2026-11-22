# Task: sector tiers, and two new sectors (Glacier and Jove)

Read Startech's root `CLAUDE.md`, and the `CLAUDE.md` of ProcSector and QuestFramework. **Don't change the plugins.** Tiers are a Startech concept, so they live in Startech; if a plugin is missing something, stop and report it.

## Decisions already made

- **Four sectors, four tiers, in a chain:** Terra (I, home) ↔ Rust (II) ↔ Glacier (III) ↔ Jove (IV). Each step away from home is harder and pays more.
- **Tougher enemies come from stat multipliers per tier.** The enemy types stay the same; no elites or new types.
- **Terra has weak enemies only:** drifters, no chasers or ranged, so new pilots can earn safely near home.

## Phase 0: discover, report, then stop

Report, with file and line references:
1. **Enemy stats:** where hull, shield and damage come from (`EnemyData`, components), and where a pooled enemy resets them (`ResetForNewLife` from the pooling task).
2. **Kill rewards:** how kills pay credits, and how enemy drops choose minerals, credits and ammo.
3. **World loot:** how pickups roll their contents (the pickup's own roll from the respawn task).
4. **The current catalog and presets:** sectors, gates, star system bodies, and what each preset's zones spawn.
5. **Bounties:** the bounty template's location options.
6. **The sector banner and gate markers:** how they get their text.

Then stop.

## Phase 1: tier data

**`UStartechSettings` additions:**
- `SectorTiers`: a list of `{SectorId, Tier}`. Terra 1, Rust 2, Glacier 3, Jove 4. An unknown sector counts as tier 1, with a warning logged once.
- `TierData`: one entry per tier. It's a `TArray`, looked up by tier number.

| Field | I | II | III | IV |
|---|---|---|---|---|
| `DisplayName` | I | II | III | IV |
| `EnemyHullMultiplier` | 1.0 | 1.6 | 2.5 | 4.0 |
| `EnemyShieldMultiplier` | 1.0 | 1.6 | 2.5 | 4.0 |
| `EnemyDamageMultiplier` | 1.0 | 1.3 | 1.7 | 2.2 |
| `KillCreditsMultiplier` | 1.0 | 1.8 | 3.0 | 5.0 |
| `LootAmountMultiplier` | 1.0 | 1.5 | 2.2 | 3.2 |
| Mineral weights (Ferrite / Cobaltine / Aurium) | 100 / 0 / 0 | 60 / 40 / 0 | 30 / 50 / 20 | 0 / 40 / 60 |
| `DangerColor` (from `DA_HudStyle`) | Text | Credits | Hull | Danger |

**`UStartechTierSubsystem`** (a world subsystem) is the one place that answers "what tier is the current sector, and what are its values":
- `GetCurrentTier()` and `GetCurrentTierData()`
- `GetTierForSector(FName)`
- `OnTierChanged(int32 NewTier)`, fired on `OnSectorReady`

Nothing else reads the settings table directly.

## Phase 2: applying tiers

- **Enemies:**
  - In `ResetForNewLife`, which runs on every spawn and respawn, set max hull and max shield from `EnemyData` × the current tier's multipliers, and fill both.
  - Damage dealt by enemies × the damage multiplier.
  - Pooled enemies move between sectors, so this must happen on every acquire, never once in `BeginPlay`.
- **Kill rewards:** credits from kills × `KillCreditsMultiplier`.
- **Enemy drops and world loot:**
  - Minerals are chosen with the tier's weights.
  - Amounts × `LootAmountMultiplier`, rounded and at least 1.
  - Ammo and credit amounts in loot scale the same way.
  - The empty-loot reroll still applies.
- **Bounties:** location options for all four sectors, with the reward multiplier equal to `KillCreditsMultiplier` (1.0 / 1.8 / 3.0 / 5.0), plus "anywhere" at 1.0. Update `DA_BountyTemplate_Standard` with the script.
- **Shop prices don't change.** The value of a mineral comes from its type, so Aurium from Jove is simply worth more.

## Phase 3: sectors and content

Use an editor script (`Tools/Sectors/setup_tiers.py`) where possible; otherwise give exact manual steps.

**Catalog (`DA_SectorCatalog`):**
- Sectors Terra, Rust, Glacier, Jove, each with a play area of 200000 × 200000 × 50000.
- **Gates as a chain:**
  - Terra: one gate at (0.925, 0, 0) leading to Rust.
  - Rust: (−0.925, 0, 0) leading to Terra and (0.925, 0, 0) leading to Glacier.
  - Glacier: (−0.925, 0, 0) leading to Rust and (0.925, 0, 0) leading to Jove.
  - Jove: (−0.925, 0, 0) leading to Glacier.
- **Star system bodies:** keep Terra (orbit 1) and Rust (1.5). Add Glacier (orbit 3, angle 150, height −0.2, radius 0.9, icy blue colours) and Jove (orbit 5.2, angle 250, height 0.3, radius 6, banded orange colours, two moons).

**Presets:**
- `DA_Preset_Terra`: remove the chaser and ranged entries from every zone. Keep drifters and loot, and keep the station in the Core zone.
- `DA_Preset_Glacier` and `DA_Preset_Jove`: copy `DA_Preset_Rust`, with no station, then recolour:
  - Glacier: ice white and pale blue surface, cyan atmosphere, blue-teal nebula, pale grey-blue asteroids
  - Jove: orange and brown banded surface, gold atmosphere, purple-orange nebula, dark asteroids
  - Enemy counts as in Rust; the tier multipliers make them harder.

## Phase 4: showing the tier

- **Sector banner:** "Rust · Tier II", with the tier in its `DangerColor`. The arrival banner after a warp shows it too.
- **Gate markers:** "Gate · Glacier · Tier III", with the tier part in its `DangerColor`.
- **First visit to a higher tier:** the first time the player enters a tier above the highest visited so far, show a notice in the quest notice area: "Tier III sector: enemies are much stronger", in Hull or Danger colour by tier.
  - The highest tier visited is saved: add it to the `Startech.Profile` participant, bumping its version, with old saves migrating to 1.
- **Station hub top bar:** unchanged. The hub only exists at Terra.

## Conventions

The same as before: no `TMap`, const correctness, logic in C++, doc comments, Blueprint only for layout.

## Acceptance

1. Builds cleanly. Old saves load; highest tier visited migrates to 1.
2. Summary: the tier table as implemented, every changed asset, and any manual steps.
3. Test plan (add `Startech.Test.Tier` to log the current tier and its values, and an automated check that a pooled enemy's max hull changes when the same actor is reused in a different tier's sector):
   - Terra: only drifters, the banner shows "Terra · Tier I", and drops are Ferrite only.
   - Warp to Rust: the "Tier II" first-visit notice appears. Enemies are tougher (check with `Startech.Test.Tier` and target hull), kills pay more, and Cobaltine appears.
   - Glacier and Jove: gates chain correctly both ways, arrival lands at the right return gate, and each has its own sky and neighbours. Aurium appears in Glacier and is the most common mineral in Jove.
   - Bounties for Jove pay about 5× a Terra bounty with the same target and count.
   - Die in Jove: respawn at Terra, as before.
   - Save in Jove's tier, quit, continue: the highest tier is kept, so the notice doesn't repeat.
