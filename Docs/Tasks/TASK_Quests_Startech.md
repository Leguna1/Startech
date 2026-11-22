# Task: connect Startech to QuestFramework, with one quest of each kind

Read Startech's root `CLAUDE.md`, `Plugins/QuestFramework/CLAUDE.md` and `Plugins/ProcSector/CLAUDE.md` first. **The QuestFramework core task must be done.** Don't change QuestFramework or ProcSector; if something is missing there, stop and report it.

Quest UI comes later, together with the station hub. Until then, quests are tested with the plugin's console commands and its `Quest.Debug 1` overlay.

## Phase 0: discover, report, then stop

Report, with file and line references:
- how enemies die, and how the killer is known (player shots versus other causes)
- `EnemyData`
- item data and how minerals are identified
- the cargo add and remove paths
- credits
- the docked state
- where game-wide services live (subsystems, the pawn, the controller)

Propose where the Startech quest bridge should live. Then stop.

## Phase 1: tags

Define the game's tags in `Config/DefaultGameplayTags.ini`, or natively in one Startech file; choose and justify:

- **Enemies:** `Enemy.Drifter`, `Enemy.Chaser`, `Enemy.Ranged`, plus a parent `Enemy` that matches all of them. Add a `FGameplayTagContainer` to `EnemyData` and set it per type. That's an editor step for the data assets, so list it.
- **Items:** one tag per mineral and ammo type (`Item.Mineral.<Name>`, `Item.Ammo.<Name>`), on the item data.
- **Rewards:**
  - `Reward.Credits`
  - item rewards reuse the item tags directly, for example a reward tag `Item.Mineral.IronOre` grants that mineral
  - `Reward.Ammo.<Name>` grants that ammo

## Phase 2: report events

Send events through `UQuestSubsystem::ReportEvent`:

| Event | When | Subject tags | Context |
|---|---|---|---|
| `Quest.Event.TargetDestroyed` | An enemy is destroyed by the player (directly or by the player's projectiles) | The enemy's tags | The current sector id |
| `Quest.Event.ItemCollected` | Items enter the cargo, by pickup or purchase | The item tag | The current sector id |
| `Quest.Event.LocationReached` | `OnSectorReady` (ProcSector), including the first sector at start | None | The sector id |

- Kills by other causes, such as collisions or other enemies, don't count. Report how "killed by the player" is decided.
- Also report `ItemCollected` when items leave the cargo (sold, lost on death or removed), so Collect objectives re-query. Use the same tag with `Amount = 0`, and document that convention in the summary.

## Phase 3: bridge

A Startech class implements `IQuestInventoryProvider` and `IQuestRewardGranter` over the existing cargo and credits. It registers them and the turn-in gate with `UQuestSubsystem` at start, and again after respawn if needed.

- **Turn-in gate:** allowed only while docked; otherwise the reason is "Dock at the station to turn in".
- **Item rewards that don't fit in the cargo:** grant what fits and report the rest in the log. Don't block the turn-in.

**Bounty board refresh:** call `UBountyBoardSubsystem::Refresh()` each time the player docks.

## Phase 4: example content, one of each kind

Create with an editor script in `Tools/Quests/create_example_quests.py`. If instanced objectives can't be created from Python, give exact manual steps instead.

1. **Story quest `DA_Quest_FirstContract`** (Category Story):
   - Title "First contract".
   - Objectives in order:
     1. Visit "Rust"
     2. Collect 20 of the most common mineral, with `bConsumeOnTurnIn` true
   - Rewards: 1500 credits, plus ammo of the basic type (amount to suit the game; report it).
   - Requires turn-in.
2. **Timed quest `DA_Quest_DrifterSweep`** (Category Timed):
   - Title "Drifter sweep".
   - Destroy 8 enemies tagged `Enemy.Drifter`, anywhere, within 240 s.
   - Repeatable, with a 300 s cooldown.
   - Reward: 1200 credits.
   - Requires turn-in.
3. **Bounty template `DA_BountyTemplate_Standard`:**
   - Targets: Drifter (6–12, 80 cr each, weight 3), Chaser (5–10, 140 cr each, weight 2), Ranged (4–8, 200 cr each, weight 1).
   - Locations: Terra (×1.0), Rust (×1.6), anywhere (×1.0, weight 1).
   - No time limit.
   - Title "Destroy {Count} {Target} in {Location}" ("anywhere" reads naturally).

Add the paths to the QuestFramework settings.

## Conventions

The same as the other tasks: no `TMap`, const correctness, logic in C++, doc comments.

## Acceptance

1. Builds cleanly.
2. The summary lists the editor steps: tags on `EnemyData` assets and item data, plus anything the script couldn't do.
3. A test plan in the summary, using the console and `Quest.Debug 1`:
   - `Quest.List`: First contract and Drifter sweep are Available.
   - Accept First contract. Warp to Rust: the Visit objective completes. Collect minerals: progress rises. Sell some: progress drops. Undocked `Quest.TurnIn`: refused with the dock reason. Docked: minerals removed, credits and ammo granted, status Completed.
   - Accept Drifter sweep. The timer counts down in the overlay and pauses when the game pauses. Kill 8 drifters in time: ReadyToTurnIn, then turn in. Repeat with too few kills: Failed at 0, then the cooldown.
   - Dock: `Quest.Bounty.List` shows 3 offers. Accept one and complete it: correct credits. Dock again: the unaccepted offers change and the accepted one stays.
   - Kill an enemy without being the killer (if possible to arrange): no progress.
   - Die during Drifter sweep: the timer keeps running and progress is kept.
