# Startech

## Pooling (ProcSector)

Enemies, stations and loot are pooled by the ProcSector plugin (`Plugins/ProcSector`, read its CLAUDE.md before touching it).

- Never call `Destroy`, `K2_DestroyActor` or `SetLifeSpan` on enemies, stations or loot. Use `Despawn` (`UDespawnSubsystem::Despawn`, `Source/Startech/Public/DespawnSubsystem.h`).
- `BeginPlay` runs once per actor. Per-life state is set in `ResetForNewLife()`, which runs from `BeginPlay` and `OnAcquiredFromPool`.
- A dead enemy is still a valid actor. Never use `IsValid` alone to mean "alive" or "targetable"; use the targetable flag.
- Anything holding references to enemies must react to `OnTargetableChanged` or `OnReleasedToPool`, not only `EndPlay`.
- New enemy and loot classes implement `IPoolable`. Enemies have no AI controller: `UEnemyBrainComponent` flies them, restarted with `ResetBrain` and stopped with `StopBrain`. Do not rely on `AutoPossessAI`.
- Pooled enemies are spawned without a deferred step, so their `EnemyData` is a class default: one Blueprint subclass per enemy type.
- Enemy loot drops come from `UActorPoolSubsystem::Acquire` (through `UDespawnSubsystem::DropLoot`), never from `SpawnActor`.

## Quests (QuestFramework)

QuestFramework (`Plugins/QuestFramework`, read its CLAUDE.md) is connected through `UStartechQuestBridge` (`Source/Startech/Public/StartechQuestBridge.h`), a world subsystem. It provides the inventory, rewards and the turn-in gate (docked only), and it reports the quest events.

- Tags are native, in `StartechGameplayTags.h/.cpp`. Enemies carry `Enemy.*` on `UEnemyData::EnemyTags`. Minerals, ammo and the player's weapons carry `Item.*` on `UTradeableData::ItemTag`.
- `UStartechSettings::ItemCatalog` is the one tag-to-asset lookup (`FindItem`), for quests and saves alike. Every tagged good must be listed there.
- Rewards: `Reward.Credits` grants credits. Any other reward tag is an item tag resolved through `ItemCatalog`: minerals go to cargo, ammo to the weapon system, weapons to the spares.
- A kill counts only when the killer is the player ship (`ReportEnemyDestroyed`, called from `AStartechEnemyShip::HandleDeath`).
- Cargo changes report `Quest.Event.ItemCollected`: a gain sends the amount gained, and a loss (sold, dropped or removed) sends `Amount = 0` so Collect objectives re-read the inventory. Ammo is reported when added, never when fired.
- Quest content lives in `/Game/Quests` and is built by `Tools/Quests/create_example_quests.py`. Asset tags (enemies, minerals, ammo, weapons) are applied by `Tools/Quests/apply_quest_tags.py`.

## Action bar (`Source/Startech/*/ActionBar/`, `UI/HUD/HudActionBarWidget`)

- **Layout:** `DA_ActionBarLayout` (`UActionBarLayout`, chosen in `UStartechSettings::ActionBarLayout`) lists the slots. Each slot has a kind, an item tag, a group label and an input action (`IA_Slot1–6` on keys 1–6). Made by `Tools/HUD/setup_action_bar.py`.
- **Components:**
  - `UActionBarComponent` on the ship binds the slot keys and the mouse wheel. Both do nothing while the hub or pause menu is open.
  - One `UActionSlotProvider` per slot kind fills `FActionSlotState` and `FActionSlotVisual`; `UAmmoSlotProvider` is the only one so far.
  - `UHudActionBarWidget` draws only those two structs, so a new kind (rockets, abilities) is a new provider plus a line in `FActionSlotProviderFactory`. The widget doesn't change.
- **Weapon groups:** each keeps its own selected ammo. A slot key selects that ammo and makes its group active, and the trigger fires only the active group. `GetSelectedAmmo()` is the active group's selection. Saved as Arsenal version 2.
- **Range:** `UTargetRangeComponent::IsGroupOutOfRange` compares any current target (soft or hard) against `UWeaponSystemComponent::GetGroupRange`.
- **WB_GameHUD layout:** placed by `Tools/HUD/replace_weapon_strip.py apply` through `UHudLayoutEditLibrary` (editor-only C++, because Python can't reach widget trees). The bar is bottom centre, (0, −28), 376 × 96; the speed readout is at alignment (0, 1), position (204, −42).
- **Script paths:** pass `-script=` paths to `pythonscript` with forward slashes. A `\r` in a Windows path, as in `\replace...`, turns into a carriage return.
- **Debug (non-shipping):** `Startech.Test.ActionBar`, `Startech.Test.SetAmmo <Tag> <N>`, `Startech.Test.InfiniteAmmo <0|1>`, and the console variable `Startech.Test.ActionBarFakeTimers 1`. Screenshots with the UI need `Shot showui`, sent through `Startech.Flow.OnStartCommands`.

## Sector tiers (`Source/Startech/*/Tiers/`)

- `UStartechSettings::SectorTiers` (sector id → tier) and `TierData` (one `FSectorTierData` per tier, entry 0 = tier 1) are in `DefaultGame.ini`. Read them only through `UStartechTierSubsystem`. An unknown sector is tier 1, with a warning logged once.
- **Current tier vs spawn tier:** enemies and loot are taken from the pool while the next sector generates, before the director's `GetCurrentSectorId()` switches. Anything spawning reads `GetSpawnTier()`/`GetSpawnTierData()` (set on `OnSectorTransitionStarted`); the HUD reads `GetCurrentTier()`, which switches on `OnSectorReady` and fires `OnTierChanged`.
- Tier colours are an `ETierColorRole` (Text, Credits, Hull, Danger) resolved through DA_HudStyle, never raw colours.
- **Enemies:** `AStartechEnemyShip::ApplyStats` (every life) reads the spawn tier into `LifeTier`: max hull and shield × the tier, and every mount's `SetDamageScale` × `EnemyDamageMultiplier`. The death drop uses `LifeTier`'s `KillCreditsMultiplier` for credits.
- **Loot:** `ULootTable::Roll(FLootRollModifiers)`; build the modifiers with `FLootRollModifiers::FromTier(Tier, ELootRollSource::World|EnemyDrop)`. With tier mineral weights, minerals are one roll at `MineralChance`, picked by weight, with that mineral's own entry's amount. Amounts are scaled, rounded and at least 1. The player's death loot is never rerolled or scaled.
- **Bounty locations** (`DA_BountyTemplate_Standard`) use each sector's `KillCreditsMultiplier` and are set by `Tools/Sectors/setup_tiers.py`. Changing them changes what saved bounty seeds regenerate into, so bump `Startech.Quests` and drop the bounties.
- Saves: `Startech.Sectors` v2 drops v1 consumed spawn ids (only enemies could be in them); `Startech.Quests` v3 drops generated bounties from v1 and v2 (v3: the Terra template), and the start flow refills the board (`NeedsBoardRefresh`).
- **Content** (`Tools/Sectors/setup_tiers.py [loot] [bounty] [presets] [catalog]`, idempotent; it reports with `[SectorTiers]`): four sectors in a chain, Terra ↔ Rust ↔ Glacier ↔ Jove, with gates at x = ±0.925. Glacier and Jove presets are recoloured copies of Rust, and the script recopies them from Rust on every run, so change Rust's spawns rather than the copies. Terra has only drifters. In every preset, enemies respawn after 30 s and are never remembered; loot respawns after 60 s.
- **Display:** the sector banner shows "Rust · Tier II" and gate markers show "Gate · Glacier · Tier III" (`UMapMarkerComponent::LabelSuffix` and `LabelSuffixColor`). In both, the tier is in its `DangerColor`, resolved through `UStartechHudStyle::GetTierColor`, and drawn with `FHudDraw::TwoColourText`.
- **First visit:** `UStartechTierSubsystem::OnFirstVisitToTier` fires when a sector's tier is above `UPlayerProfileSubsystem::GetHighestTierVisited()` (saved in `Startech.Profile` v2; v1 loads as 1). `UHudQuestNoticeWidget` shows the tier's `FirstVisitNotice` in its `NoticeColor`.
- Editor Python: struct arrays (`zones`, `entries`, `types`) come back as copies. Build new lists and set every level back.
- Debug (non-shipping): `Startech.Test.Tier`, `Startech.Test.Warp <SectorId>` (a real gate warp), `Startech.Test.Where` (sector, ship location, nearest gate, each gate marker's label and tier suffix, enemies by type), and `Startech.Test.TierPoolCheck record|check` (record in one sector, warp to another tier, check: logs `[TierPool] PASS|FAIL`). Unit test: `Automation RunTests Startech.Tiers`.

## HUD projection

- `FHudDraw::ProjectToWidget` takes the window's position off (`ScreenToWidgetLocal(..., true)`), because paint geometry is window-relative. Without it every projected element (lock brackets, gate markers, charge warnings) landed off screen in a standalone window away from the desktop corner; a maximized editor hid it. Call it only with paint geometry.

## Boost (`Source/Startech/*/Boost/`)

- `IA_Boost` (Left Shift in `IMC_Ship`, made by `Tools/Ship/create_boost_input.py`) is bound through `UStartechSettings::BoostInputAction`: Started presses and Completed releases. There is no Hold trigger.
- `UShipBoostComponent` (subobject `Boost` on `AStartechShipPawn`) doesn't tick on its own. The pawn calls `UpdateBoost` from its Tick before the flight model, with `IsBoostAllowed()` (false in the hub, the pause menu, a warp or when dead). It is refilled on BeginPlay (so also on load) and on respawn.
- All numbers are in `UStartechSettings::Boost` (`FShipBoostSettings`).
- Flight: `AStartechShipBase::SetFlightScales(Normal, Boost)` is player-only (enemies stay at 1). Normal scales every top speed and the acceleration (Engines upgrade); Boost scales forward speed and acceleration. Anything comparing against "top speed" (nose tilt, cockpit FOV) uses `GetNormalMaxForwardSpeed()`.
- Camera: the chase camera has position lag only while boosting (`bEnableCameraLag` on when the boost alpha > 0). Both cameras widen by `FieldOfViewWidening` × the eased boost alpha.
- HUD: `UHudSpeedReadoutWidget` draws the gauge under the speed number. It uses Player colour, Hull (amber) below the restart threshold, and a Danger flash on `OnBoostDenied`.
- Debug (non-shipping): `Startech.Test.Boost [press|release|throttle <V>]` logs `[Boost]` with energy, lockout, multiplier, speed, FOVs and lag. `Startech.Test.KeyDown LeftShift` presses the real key (windowed runs only).

## Upgrades (`Source/Startech/*/Upgrades/`)

- **Catalog:** `DA_UpgradeCatalog` (`UUpgradeCatalog`, `UStartechSettings::UpgradeCatalog`), made by `Tools/Upgrades/setup_upgrades.py catalog` (rewritten from the script's `TRACKS` on every run). Tracks: Hull, ShieldCapacity, ShieldRecharge, Cargo, Engines, Lasers, Drainer, Booster. Each has `Effects` (a `Stat.*` tag, Percent or Flat, the total bonus at each level) and `LevelCosts` (credits plus minerals; the number of entries is the max level). Track ids are saved, so never rename one.
- **`UShipUpgradeSubsystem`** (game instance): `GetLevel`, `CanUpgrade(TrackId, OutReason)` (Max level → Dock to upgrade → credits → minerals), `Upgrade`, `SetLevel`, `OnUpgradeChanged` (`NAME_None` = all tracks, after a load or a new game). A stat is `(base + GetStatBonus) × GetStatMultiplier`; Percent bonuses add up (+20% ×5 = ×2.0). Credits × `UpgradeCostScale`, rounded to 10.
- **Querying stats:** a system with an upgradable value never reads the levels itself. The player's value is `(base + UShipUpgradeSubsystem::GetStatBonus(Tag)) × GetStatMultiplier(Tag)`, with a `Stat.*` tag from `StartechGameplayTags.h`; use `UShipUpgradeStatsComponent::GetUpgradedValue(Tag, Base)` for that. A system gets the value pushed to it through a setter called from `ApplyStats`, and only the player has that component. A new upgradable stat needs a new `Stat.*` tag, a setter on the system, a line in `ApplyStats`, an effect in the catalog script, and a unit in `FUpgradeStatFormat`. For previews ("next level"), use `GetStatBonusWith`/`GetStatMultiplierWith`. Where each value is read:
  - max hull and shield: `UHealthComponent` (`SetMaximums`)
  - shield regeneration: `HealthComponent.cpp` `TickComponent` (`ShieldRegenRate`)
  - cargo: `UInventoryComponent` (`SetCargoCapacity`, `GetCargoFree`)
  - engines: `StartechShipBase.cpp` `UpdateVelocity` (`NormalSpeedScale`, set by `SetFlightScales`; the pawn's boost update keeps it)
  - lasers: `LaserMountComponent.cpp` shot damage × `GetDamageScale()`
  - drainer: `ShieldDrainMountComponent.cpp` drain × `GetDamageScale()`
  - boost pool and regeneration: `ShipBoostComponent.cpp` `GetMaxEnergy` and `GetRegenPerSecond`
- **`UShipUpgradeStatsComponent`** (subobject `UpgradeStats`, player ship only) remembers BP_ShipPawn's values at BeginPlay and applies the stats: `UHealthComponent::SetMaximums` (a raised maximum raises the current value), `SetShieldRegenRate`, `UInventoryComponent::SetCargoCapacity`, the normal flight scale, each mount's `SetDamageScale` by group, and `UShipBoostComponent::SetEnergyScales`. Reapplied on `OnUpgradeChanged`, at BeginPlay and in `RespawnAt`. A change while docked (a purchase) fills hull and shield to the new maximums. Enemies have no such component, so never set those values on the player from anywhere else.
- **Content** (`setup_upgrades.py [catalog] [ship] [shop] [bounty]`): BP_ShipPawn MaxShield 50; DA_Shop_Minerals sells only Ferrite (still buys Cobaltine and Aurium); `DA_BountyTemplate_Terra` (drifters, Terra only). Terra is not in the Standard template's locations (`setup_tiers.py bounty`).
- **Hub tab:** `UHubUpgradesTabWidget` (one `UUpgradeRowWidget` per track, painted in C++, refreshed on `OnUpgradeChanged`, `OnCreditsChanged` and `OnItemsChanged`). `UStationHubWidget` makes it into `ContentSwitcher` when WBP_StationHub doesn't place one (`UpgradesTab` is `BindWidgetOptional`). A disabled Upgrade button's label is the `CanUpgrade` reason. Stat texts ("Hull 300 → 400") come from `FUpgradeStatFormat`, which computes values the same way the stats component applies them. `UHudQuestNoticeWidget` shows "<track> upgraded to level N" for a raised level, held until the hub closes.
- Debug (non-shipping): `Startech.Test.Hub 1 Upgrades` opens the hub on a tab. `Startech.Test.Upgrades` (levels, next cost, reason, the player's stats and the nearest enemy's), `Startech.Test.SetUpgrade <Track> <Level>` (free, anywhere), `Startech.Test.BuyUpgrade <Track>` (as the hub button).

## Combat balance (`Source/Startech/*/Combat/`)

- **`DA_CombatBalance`** (`UCombatBalance`, `UStartechSettings::CombatBalance`, read through `UCombatBalance::Get()`, which roots it) holds every combat balance value. The tier multipliers stay in `UStartechSettings::TierData`. Never hard-code a balance value.
  - Made by `Tools/Combat/setup_combat_balance.py [balance] [ammo] [enemies] [shop] [loot]`, which is idempotent and reports with `[CombatBalance]`. Change the numbers in the script's constants, not in the asset.
  - The `ammo` and `enemies` steps copy the balance into the ammo, enemy and weapon assets so the editor shows the same numbers. At runtime the balance wins, and the TTK table notes any asset that differs.
- **Runtime readers:**
  - Ammo damage: `UCombatBalance::GetDamageMultiplier(Ammo)`, used by `ULaserMountComponent::Fire` and the shop's detail line.
  - Prices: `UCombatBalance::GetBaseValue(Good)`, used by `UShopData::GetPrice*`. Ammo not listed in `Ammo` uses its asset's values.
  - Enemies: `AStartechEnemyShip::ApplyStats` finds the type's `FEnemyBalance` by `EnemyTags` and applies its hull, shield, laser and preferred range. The mounts' damage scale makes one shot `DamagePerShot` × the tier's `EnemyDamageMultiplier`.
  - The brain reads `GetPreferredRange()`/`GetLaserWeapon()`, never `UEnemyData`'s values.
  - `DropLoot` multiplies credits and loot amounts by `RewardMultiplier` (Drifter 1, Chaser 2, Ranged 3). Loot tables still pick the items.
- **Enemy behaviour** (`UEnemyBrainComponent`, C++ state machine: Idle → Engage → ReturnHome). The type comes from `UEnemyData::Behavior` (Passive, Chaser, Ranged); every behaviour number is in `FEnemyBalance::Behaviour` (`FEnemyBehaviourBalance`, set by the script's `BEHAVIOUR` dict), read through `AStartechEnemyShip::GetBehaviour()`. A 0 switches a feature off.
  - **Drifter (passive):** never starts a fight; damage from the player wakes it, other drifters don't join. Gives up after 20 s without taking damage (`LeashQuietSeconds`) or with the player beyond 2× its laser range for 8 s (`LeashFarRangeFactor`, `LeashFarSeconds`).
  - **Chaser:** engages on sight within `AggroRange` (80 m). Forward speed per tier (`ForwardSpeedByTier`, entry 0 = tier I; `GetForwardSpeed(LifeTier)`): 2,500 / 2,950 / 3,250 in tiers II / III / IV: the player's reverse at the expected Engines level, so backing off holds distance rather than gaining it. Damage per shot 1.9, a 45 m gun (`WEAPON_RANGE` in the script), preferred range 33 m (band 29.7–36.3 m). As soon as the player is beyond its laser range it afterburns (`AfterburnTriggerFactor` 1.0; `SetFlightScales(1, 1.8)`, 4,320 cm/s, up to 4.5 s, then a 4 s cooldown): faster than the player's normal speed, slower than the boost in every tier (4,500 / 5,310 / 5,850 against 6,600 / 7,790 / 8,580 at the expected Engines level). A burn ends on reaching the preferred range. In range it circles (`bCircle`: strafes, swapping every 2–4 s, at its preferred range); out of range it heads for a flank point that blends from the player to 33 m beside it between 1.5× and 1× its laser range (`FlankBlendStart`), so it doesn't cross the player's path. Gives up after 25 s without dealing or taking damage (`bQuietCountsDealtDamage`), or past `HomeLeashRange` (300 m from home; the enemy can't know its ProcSector zone).
  - **Ranged:** engages on sight within 120 m and holds 81–99 m, strafing. With the player at 36–90 m it reverses while firing; within 36 m (`RetreatFraction` 0.4) it turns and flies off at 3,500 without firing (or charging) until it is back at 90 m. Normal shot 0.7 (halved; the charged shot carries the rest).
  - **Charged shot** (`bChargedShot`, `UEnemyChargeComponent`, subobject `ChargedShot` on every enemy, idle unless used): the brain starts one 2–3 s into a fight, then every 6–8 s (start to start), with the player in laser range and in line of sight (a Visibility trace). Normal fire pauses meanwhile.
    - **Telegraph** (1.2 s): the hull glows towards `GlowColor`, pulsing faster (`FHullFlash`, shared with the death flash); `ChargeFX` and `ChargeSound` at the muzzle (between the laser mounts); the HUD warns. It records the target's position and velocity at the start.
    - **Shot:** `AChargedShotProjectile` (pooled, through `UChargedShotSubsystem::Fire`/`Release`) aimed at where that velocity takes the target by the time the shot arrives. Keeping the same motion gets you hit; any change during the telegraph or flight (flip or stop the strafe, push or pull, boost) dodges it. No homing. 12,500 cm/s (0.72 s at 90 m), 150 cm sweep on `ECC_Visibility` (asteroids and the player stop it; wrecks don't), 2 s lifetime, 45 damage × the tier's `EnemyDamageMultiplier` (`GetLifeDamageMultiplier`), credited to the shooter. Every projectile is released on `OnSectorTransitionStarted`.
    - **Stops at once** on death (`HandleDeath`), giving up, retreating, `StopBrain` and the pool hooks; a shot already flying keeps flying.
    - **Effect hooks** in `UStartechSettings::ChargedShot` (`FChargedShotEffects`): glow colour, strength, pulse and parameters; `ChargeFX`/`ChargeSound`, `ProjectileFX`, `FireSound`, `ImpactFX`/`ImpactSound`. Placeholders: `NS_Laser_Hit` (charge, impact), `NS_Laster_Bolt` ×3 (projectile). Preloaded with the death effects.
    - **HUD warning:** `UChargedShotSubsystem::GetActiveCharges` lists the charges in progress. `UHudScreenFeedbackWidget` draws pulsing Danger brackets on a charging enemy on screen (the target included), or a pulsing Danger edge (`M_HUD_EdgeFeedback`) towards one off screen; `UHudReticleWidget` turns the target's brackets Danger. All pulse with `UEnemyChargeComponent::GetPulse`.
    - **Native events** (`UChargedShotSubsystem`): `OnAnyChargeStarted`, `OnAnyChargedShotFired`, `OnAnyChargedShotEnded` (hit actor, damaged). The combat log counts charged shots fired and hit and their damage (report column "Charged hit/fired (dmg)", CSV `ChargedFired,ChargedHit,ChargedDamage`).
  - **No overshoot:** the brain predicts where the ship stops if it brakes now (`GetPredictedDistance`, closing speed² / 2 × acceleration, relative to the player's motion) and reverses once that point is inside its range band, burning or not. A chaser never comes inside about two ship lengths (~27 m).
  - **Every life starts calm:** `ResetBrain` clears aggro, leash timers, afterburn and cooldown; `ResetForNewLife` sets the flight scales back to 1; `StopBrain` (death) ends a burn.
  - Debug (non-shipping): `Startech.Test.Enemies` logs `[Brain]` lines (state, give-up reason, distance, speed, top speed, afterburner, cooldown, bounds); `Startech.Test.ChargeShot [Seconds] [Count]` screenshots (with the HUD) that long into the next telegraphs; `Startech.Test.Turn <Degrees>` yaws the player (to put an enemy behind it). The charge logs `[Charge]` lines (charging, fired, what the shot hit). Unit tests: `Automation RunTests Startech.Behaviour`.
- **Preferred range:** the brain holds anywhere within ±`RangeTolerance` (10%) of it. Keep `PreferredRange` × 1.1 inside the laser's range, or the enemy never fires. The TTK table reports a violation as an error.
- **`Startech.Combat.TTKTable`** (non-shipping, no world needed) prints one row per enemy type, tier and expected-gear case (`GearCases`).
  - Columns: time to kill, health lost fighting one and three, rounds, ammo cost, kill value and cost %.
  - '*' marks a value outside its target. Roles: Regular rows are held to the targets, Tough to `ToughFactor` × them, Weak to none.
  - It assumes both sides hit every shot. The enemy's lasers fire for its `FireUptime` share of the fight (measured: the report's Uptime column), and charged shots come on the type's schedule (counted in "Charged"; their telegraphs don't fire lasers). Kill value is average credits plus the loot's sell value at `LootShop`.
  - A second loss column ("Alt case"): every charged shot dodged (charged-shot types), or the player backing off (afterburner types, with `FireUptimeAlt`). The chaser's back-off column models a chaser met in its aggro range (aim 10–15%) and is information only (`bLossAltTargetInfoOnly`): play sessions vary 3–5 points, and tiers III–IV were accepted under it (5–8%).
  - Per-type loss targets (`LossTargetMin/Max`, `LossAltTargetMin/Max`, `bLossTargetsRegularOnly`) override the role's `OneFightLoss` targets; set in the script's `TABLE` dict.
  - Headless: `UnrealEditor-Cmd <uproject> -ExecCmds="Startech.Combat.TTKTable, QUIT_EDITOR" -unattended -nullrhi -stdout` (the lines start with `[TTK]`).
  - `Automation RunTests Startech.Combat.TTKTargets` fails on table errors and on marked values: time to kill, the one-fight loss (the player facing the enemy; ranged: every charged shot hit), the ranged "dodged" column and the ammo cost. Information-only targets (the chaser's back-off column) are printed, never failing. Chasers circled by the player have no table value (measured 15–23% in sessions, also information).
- **Ammo progression:** LL1 ×1 (free), LL2 ×2 (0.45 cr a round), LL3 ×3 (0.90 cr, locked until tier III), LL4 ×4 (never sold, dropped in tiers III and IV). Prices are `BaseValue` × the shop's 1.5 markup.
- **Free ammo** (`FreeAmmoTag`, LL1):
  - `UWeaponSystemComponent::IsUnlimited`/`HasAmmo` are true for it, and `ConsumeAmmo` never removes it.
  - It is never stored: `AddAmmo` ignores it, `RestoreAmmo` drops saved LL1 counts, and `GetAmmoCount` is 0. Ask `HasAmmo`, not the count.
  - The action bar shows ∞ (`Count = -1`). The shop never offers it, and it is not in `DA_WorldLoot` or the new-game ammo.
- **Running out:** when the selected laser ammo can't fire a round while the trigger is held, the weapon selects LL1 in the same tick and fires `OnAmmoAutoSwitched`. `UHudQuestNoticeWidget` shows "LL2 empty, switched to LL1". Shield drain ammo is never switched.
- **Locks:** `AmmoUnlocks` (ammo, required tier, reason) are compared with `UPlayerProfileSubsystem::GetHighestTierVisited()`, which is already saved and kept on death. `FShopTrading::IsLocked` makes a buy `ETradeResult::Locked`: the Buy button's label is the reason ("Reach Glacier (tier III)") and the detail reads "Locked".
- **Drop-only** (`DropOnlyAmmoTags`, LL4): `FShopTrading::IsBuyOffered` is false whatever a shop asset lists. `TierAmmoDrops` (tier III 15%, tier IV 25%, 20–60 rounds, not scaled by the tier) are rolled by `UCombatBalance::RollTierAmmoDrops` from `AStartechEnemyShip::DropLoot` on the enemy's `LifeTier`, into the same pickup.
- **Shots are spheres:** `UWeaponMountComponent::TraceShot` sweeps a sphere of `ShotRadius` (50 cm), not a line. A line aimed at a ship's centre can pass between its convex collision pieces: `SM_StarSparrow32` (Ranged) seen nose-on was unhittable.
- **Native combat events** (static, every world; for listeners that must not bind to pooled actors):
  - `UHealthComponent::OnAnyDamageApplied`: shield and hull damage, instigator, drain flag, whether fatal.
  - `ULaserMountComponent::OnAnyShotFired`: the mount, the actor its trace hit, the ammo.
  - `AStartechEnemyShip::OnAnyEnemyDied`, at hull 0, after the drop is rolled (`GetLastDrop`). The pickup appears when the wreck explodes.
- **Combat log** (`UCombatTelemetrySubsystem`, game worlds, not created in shipping):
  - An engagement runs from the first player shot at an enemy, or the first damage it deals the player, until it dies, the player dies, or 15 s pass with neither.
  - It records type, tier, main ammo, Lasers level, time on target (the span of the volleys) and total time, shots hit/fired (misses split into blocked and missed), rounds and cost by ammo, shield and hull taken, enemy hits/shots, and the drop's credits, value and ammo. Net profit is the drop's value minus the ammo.
  - Each finished engagement is a line in `Saved/CombatLogs/Combat_<time>.csv`.
  - `Startech.Combat.Report` prints the session per enemy type and tier (averages per kill, hit %, profit per kill, credits per minute of fighting, enemy shots and hits), the session's net credits per minute, and enemy shots fired by type.
- **Test pilot** (`UCombatTestPilot`, non-shipping): `Startech.Test.AutoFight <Type|Any|stop> [Seconds=120] [DistanceM=60] [AmmoTag]`.
  - It hard-locks the nearest living enemy of the type, holds the trigger in laser range and throttles to keep the distance.
  - Beyond 180 m it jumps the ship to just outside that distance. Flying there isn't measured.
  - It logs `[Pilot]` every 5 s with a hit probe, and stops when the time is up or the player dies.
  - `Startech.Test.AutoFight` takes an optional 5th argument, the fight's sector. After a death the pilot no longer stops: it waits for the respawn, undocks, warps back to that sector (retrying while a warp is refused in combat) and carries on; out of the sector it jumps back to the fight. Its stop line counts kills, dodges and deaths.
  - `Startech.Test.PilotStyle <face|circle|backoff|hold> [Dodge=0|1] [BackOffThrottle=1]` sets how later fights are flown: hold the distance, hold it while strafing (swapping every 3–5 s), reverse away at that throttle, or close in once and then never touch the throttle (a target that keeps its course). With Dodge it changes its strafe 0.3 s after a charged-shot telegraph aimed at it (a circler flips, anything else strafes for 2.5 s). It strafes through `AStartechShipPawn::SetSteerAxis`, the A/D axis.
  - Measuring several styles in one run: `Startech.Combat.Reset` starts a new report session (the CSV keeps everything), `Startech.Test.Heal` refills hull and shield between fights.
  - `Tools/Combat/run_behaviour_sessions.ps1 -Tiers 2,3 -Tag A [-Only 'chaser-*']` runs the Phase 3 sessions (one process per tier, segments: chaser face/circle/back-off, ranged hold/dodge; drifter in tier I), and `parse_behaviour_sessions.ps1 -Tag A` prints their reports. A segment varies 3–5 points between runs.
  - A session on test slot 3: from `/Game/StarterMap` with `Startech.Flow.OnStartCommands` = upgrades (`Startech.Test.SetUpgrade`), ammo (`Startech.Test.SetAmmo`), `Startech.Test.Undock`, `Startech.Test.Warp <Sector>`, `wait 10`, `Startech.Test.AutoFight Any 150 60 <Ammo>`, `wait 152`, `Startech.Combat.Report`, `quit`.
- Unit tests: `Automation RunTests Startech.Combat`.

## Death sequence (`Source/Startech/*/Death/`)

- **Every ship dies in two steps.** `AStartechShipBase::HandleHealthDeath` (bound to `UHealthComponent::OnDeath`) runs the virtual `HandleDeath` at hull 0, then `UShipDeathComponent::BeginDying` (subobject `ShipDeath`). When the dying state ends, the component fires `OnExploded(Location)` and the virtual `HandleDyingFinished(Location)` runs.
  - **`HandleDeath` (hull 0)** is everything that counts: the kill is credited (quest `ReportEnemyDestroyed`, `OnAnyEnemyDied`, the combat log), the drop is rolled (`RollLoot` into `GetLastDrop`), the ship is stopped (AI, weapons, intents, boost, input) and made untargetable (which releases locks). Never move any of it to `HandleDyingFinished`.
  - **`HandleDyingFinished`**: the enemy drops `LastDrop` at the explosion's location and despawns. The player hides, drops cargo there, fires `OnPlayerDied` and starts `UHomeRespawnSubsystem::BeginDeathSequence` and the respawn timer.
- **The dying state** (`UStartechSettings::Death`, `FShipDeathSettings`; `DeathDuration` 1 s, 0 explodes at once):
  - Motion: the ship base skips turning and braking while `!IsAlive()`, so the wreck keeps its velocity; the component adds a tumble about a random axis that builds from a tenth of `MaxTumbleRate`. After the explosion the ship doesn't move.
  - Collision: every colliding primitive ignores `ECC_Visibility` (shots) and `ECC_Pawn` (ships) and makes no overlaps (no docking, warping or pickups); the world still blocks it.
  - Flash: dynamic materials on the root mesh blend `FlashColorParameters` (`Color 1–3` of StarSparrow's `M_Colorize`) towards white and raise `FlashEmissionParameter` (`EmissionMultiplier`), flickering, with a pulse at each secondary explosion. Missing parameters are skipped.
  - The player's chase camera holds its heading, and the cockpit view switches to the chase camera until the respawn (`bCockpitBeforeDeath`). Pause stays refused while dead.
  - Driven by the component's tick (`TG_PostPhysics`), so it pauses with the game. No timers.
- **Effect hooks** (`FShipDeathEffects`, all soft and optional): `DeathExplosionFX` + `DeathExplosionScale`, `SecondaryExplosionFX` + `SecondaryExplosionScale`, `DeathSound`. Per type on `UEnemyData::DeathEffects` (enemies) or `AStartechShipBase::DeathEffects` (the player's Blueprint); each empty field falls back to `Death.DefaultEffects` in `DefaultGame.ini`, and an empty default plays nothing. `UShipDeathPreloadSubsystem` loads them asynchronously and holds them, at world begin play and on every `OnSectorReady`: the defaults, the player's and each enemy type's in the world (`[Death] preloading N death effect asset(s) for <sector>`). The component's synchronous load at the death is only a fallback. Placeholders: `NS_Laser_Hit` for the secondaries (`MinSecondaryExplosions`–`MaxSecondaryExplosions`, 2–3, spread over 15–85% of the state) and ×3 as the final explosion. Code can also listen to `OnSecondaryExplosion` / `OnExploded`.
- **Reset:** `ResetDeath` puts back collision, materials and tick. It runs from the component's own pool hooks, `AStartechEnemyShip::ResetForNewLife` and `AStartechShipPawn::RespawnAt`. A ship released during its dying state (a sector switch) never explodes or drops loot.
- Unit tests: `Automation RunTests Startech.Death`. World tests share `Private/Tests/StartechTestWorld.h` (`FTestWorld`: a game world with a bare game instance and `AGameModeBase`, since pawns take damage only with an authority game mode; ticked by hand, one engine frame per step; `SpawnPlayer` possesses the player ship).

## Hub buttons and the shop

- **One button class:** `UHubButtonWidget` (`UI/Hub/`) is the only styled button in the hub, the menus, the pause menu and the shop. Never place an engine `UButton` there.
  - It is painted in C++: the cut-corner `M_HUD_ChamferPanel` through `FHudPanelBrush`, with every colour and size from `DA_HudStyle`.
  - Variants (`SetVariant`): Primary (buy, upgrade, accept), Secondary (sell), Toggle (shop tabs, quantity chips; `SetSelected`) and Quiet (text links).
  - Content: `SetLabel`, `SetAccent(Text, Color)` drawn after " · " ("Buy · 12 cr" with the price in the Credits colour), and `SetSecondLine(Text, Color)`.
  - Layout and input: `SetFixedWidth` lines up a column. `SetButtonEnabled(false)` fades it and makes it unclickable. `GetLastClickModifiers()` gives the modifier keys held at the click (Shift-click).
- **Hub widgets** (`UHubWidgetBase`) build their content in `BuildContent`, called from `NativeOnInitialized`. So a panel made with `CreateWidget` can be `Setup` before it is shown.
- **Shop screen:** `WB_Shop_Screen` (`UShopScreenWidget`) keeps the Blueprint frame (panel, shop name, `TabContainer`, `EntryContainer`). Its tabs and rows are C++ hub widgets, set by `Tools/Shop/setup_shop.py`:
  - `UShopTabWidget`: a Toggle button.
  - `UShopEntryWidget`: colour chip, name, detail, "Held N", then a Primary Buy and a Secondary Sell button.
  - `UMountRowWidget`: the Loadout tab's Fit button.

  The Blueprint's old `ColumnHeaders` are collapsed. `WB_Shop_Tab`, `WB_Shop_Entry` and `WB_MountRow` are unused.
- **Bulk trading** (`FShopTrading`, `ShopTrading.h`) is the only way to trade:
  - `BuyItem` / `SellItem(Shop, Entry, Trader, Quantity)` make one transaction (one credit change, one cargo/ammo/weapon change, one `OnTradeCompleted`). They clamp to the largest amount that works and return an `FTradeOutcome` (Quantity, Requested, Credits, and `Reason` when nothing could be traded: "Need 12 more credits", "Cargo full", "None to sell").
  - `PreviewBuy` / `PreviewSell` do the same without changing anything (the button labels). `GetMaxBuyable` is the lower of what is affordable and, for minerals, the free cargo; `GetMaxSellable` is everything held.
  - Weapons always trade one at a time. Ammo takes no cargo space.
  - Prices: `UTradeableData::BaseValue` is a float, and unit prices (`UShopData::GetPricePlayerPays/Receives`) stay fractional. Only totals are rounded, up when buying (`GetBuyTotal`) and down when selling (`GetSellTotal`). There is no pack size.
  - Purchases add cargo with `AddItem(..., bShowInPickupFeed=false)`, so the HUD pickup feed stays quiet. `OnItemCountChanged` (quests) still fires.
  - `FShopTrading::OnTradeCompleted` (static, native) drives the "Trade" autosave, one request per trade, merged by SaveFramework's debounce.
- **Quantity chips:** `UShopQuantityBarWidget` has five Toggle chips (×1, ×10, ×100, ×1,000, Max; `EShopQuantity`) and the status line ("Bought 100 Ferrite for 1,200 cr", or the reason in DangerText).
  - `UShopScreenWidget` inserts it under `TabContainer` in `NativeOnInitialized`, before Slate builds the frame.
  - It is ×1 each time the hub opens, and hidden on weapon-only tabs and Loadout.
  - Keys 1-5 pick a chip through `UStationHubWidget::NativeOnKeyDown` → `UShopScreenWidget::HandleShortcutKey`. Shift-click trades Max for that click.
  - Rows refresh on credit, cargo, ammo and weapon events and on a quantity change, never per frame.
- Debug (non-shipping): `Startech.Test.Hub 1 Shop`, then:
  - `Startech.Test.ShopTab <Index>` (the station's shops, then Loadout)
  - `Startech.Test.Shop` (the quantity, every row's labels, the status line)
  - `Startech.Test.ClickShop <Row> <Buy|Sell> [Shift]` (a real Slate mouse click)
  - `Startech.Test.KeyDown One..Five` (quantity keys)

  Unit tests: `Automation RunTests Startech.Shop`.

## Saving (SaveFramework)

SaveFramework (`Plugins/SaveFramework`, read its CLAUDE.md) holds the slots. Startech's participants are in `Source/Startech/*/Save/`, created by `FStartechSaveParticipantFactory`.

- Seven participants, all listed in SaveFramework's `ExpectedParticipantIds` (`DefaultGame.ini`):
  - `Startech.Profile`, `Startech.Quests` and `Startech.Upgrades` are registered by `UStartechSaveSubsystem` when the game instance starts.
  - `Startech.Wallet`, `Startech.Cargo` and `Startech.Arsenal` are registered by the ship in `BeginPlay` and unregistered in `EndPlay`.
  - `Startech.Sectors` is registered by `UStartechSectorSaveSubsystem` when the game world starts.
  Adding a participant means adding its id to that list too.
- Goods are saved by `ItemTag`, never by pointer. Mounts are saved by component name (`LaserMountLeft`, `LaserMountRight`, `ShieldDrainMount`); never rename those subobjects.
- Restoring uses the `Restore*` functions (`UInventoryComponent::RestoreCredits/Items/Weapons`, `UWeaponSystemComponent::RestoreAmmo`, `FShipFitting::RestoreWeapon`). They fire the UI events only, never `OnItemAdded`, `OnItemCountChanged` or `OnAmmoAdded`, so a load never counts as collecting.
- New-game values live in `UStartechSettings` (`NewGame*`). SaveFramework has 4 slots: the menu shows `VisibleSaveSlots` (3), and the last slot is the editor's [PIE] test slot.
- Debug: `Startech.Save.Dump` logs everything the participants save; `Startech.Save.Give <ItemTag> <Count>` adds a catalog good.

## Start flow (`Source/Startech/*/Flow/`)

- `L_MainMenu` runs `AStartechMenuGameMode`, which shows `UStartechSettings::MainMenuWidgetClass` (WBP_MainMenu, C++ parent `UMainMenuWidget`, panels in `UI/Menu/`). Every menu action goes through `UStartechGameFlowSubsystem`; it makes the slot active, then opens `UStartechSettings::GameMap`.
- ProcSector's `bAutoGenerateOnBeginPlay` is off. In the game map, `UStartechStartFlowSubsystem` keeps the screen black and input locked until SaveFramework's `OnAllParticipantsLoaded`. Then it requests `HomeSectorId`, and on `OnSectorReady` places the ship at the station (`AStartechShipPawn::PlaceAt`) and fades in (`OnGameStarted`).
- Docks during the start never refresh the bounty board (`UStartechQuestBridge::SetDockRefreshSuppressed`), so saved offers survive. A new game refreshes it once placed.
- A game map started with no active slot (PIE) starts a temporary new game in the test slot, the last SaveFramework slot, named `[PIE] <callsign>` (`UStartechSaveSubsystem::StartTestGame`).
- `OnAllParticipantsLoaded` fires once per activated slot. Never load a slot inside the game map and then travel: the event has already fired and the next game map waits forever. The menu world has no ship, and leaving the game closes the slot.
- **Input:** `FInputModeUIOnly` makes the game viewport ignore input, and the viewport outlives map travel. Every hand-back to the game goes through `FStartechInputState::SetGameOnly`: game-only mode, cursor off, pressed keys flushed, focus on the viewport. The flow calls it before leaving the menu, the start flow on arrival (`ReleasePlayer`), and the hub when it closes. Any new UI-only screen (the pause menu) must hand back the same way.
- **Autosave** (`UStartechAutosaveSubsystem`) is the only saving during play; there is no manual save.
  - Triggers: Docked, Undocked, QuestTurnedIn (a quest reaching Completed, not during a restore), Trade (a shop trade, `FShopTrading::OnTradeCompleted`) and SectorReady (including the arrival).
  - While dying, respawning, warping, generating or starting, one request is held, with its first reason, and made when the sequence ends.
  - Quitting uses `SaveNow("Quit")` through `UStartechGameFlowSubsystem::QuitToMainMenu` / `QuitGameFromPlay`, which then close the slot.
- **Pause:** `IA_Pause` (Escape, in `IMC_Ship`, made by `Tools/Save/create_pause_input.py`) is bound through `UStartechSettings::PauseInputAction`. `UStartechPauseMenuSubsystem` opens `PauseMenuWidgetClass` (WBP_PauseMenu, C++ parent `UPauseMenuWidget`) only in flight, pauses the game, and hands input back on Resume. In the hub, input is UI-only, so Escape still only closes the hub.
- **Save indicator:** `UHudSaveIndicatorWidget` in WB_GameHUD, above the cargo and credits panel. It follows SaveFramework's `OnSaveStarted`/`OnSaveCompleted`.
- Test aids (non-shipping):
  - Console variables:
    - `Startech.Flow.LoadSlotOnStart <N>` (set with `-ini:Engine:[ConsoleVariables]:...`): the game map loads slot N instead of the test game.
    - `Startech.Flow.OnStartCommands "a;b;quit"`: once docked, runs those commands 0.25 s apart, in real time, so it keeps going while paused. `wait <s>` delays the next one. It can be set from `-ExecCmds` on the menu map.
  - Commands:
    - `Startech.Menu.NewGame <Slot> <Callsign>`, `Startech.Menu.Continue` and `Startech.Menu.Load <Slot>`: what the menu buttons call.
    - `Startech.Menu.QuitToMainMenu` and `Startech.Menu.QuitGame`: what the pause menu's buttons call.
    - `Startech.Test.Undock` and `Startech.Test.Dock`.
    - `Startech.Flow.InputState`: possession, pawn input, viewport input, cursor, focus and hub.
    - `Startech.Test.KeyDown|KeyUp <Key>`: a real key event through Slate.
    - `Startech.Test.Hub <1|0>`.
  - Key events only reach the game in a real window (`-game -windowed`), not under `-nullrhi`.
