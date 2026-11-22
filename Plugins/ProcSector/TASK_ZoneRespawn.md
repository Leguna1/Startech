# Task: respawning in ProcSector zones (replaces the old spawn volumes)

Read `Plugins/ProcSector/CLAUDE.md` first. All its rules apply. `TASK_PlayArea.md` and `TASK_EnemyPooling.md` are done; build on that code.

## Goal

Zone spawns can come back during a visit, which makes `AEnemySpawnVolume` and `ALootScatterVolume` unnecessary.

- **Respawn after a delay:** a consumed spawn (looted, killed) respawns after a per-entry delay. A delay of 0 keeps today's behaviour: gone until the next visit.
- **Near its old spot:** a respawn appears near where the previous one was consumed, never exactly on it, so a player can't camp a fixed point.
- **Away from the player:** no respawn pops in close to the player (the old volume's `MinDistanceFromPlayer`).
- **Spread out:** spawns keep a minimum spacing from each other, both in the initial layout and on respawn, so loot doesn't bunch up.
- **Live count:** a query for how many spawns of a class are alive (replaces `AEnemySpawnVolume::GetAliveCount`).

## 1. Data (`Data/SectorTypes.h`, `FZoneSpawnEntry`)

Add, each with a doc comment:

| Field | Default | Meaning |
|---|---|---|
| `float RespawnDelay` | 0 | Seconds after consumption before it respawns. 0 means no respawn during the visit. |
| `float RespawnOffsetMin` | 3000 | Smallest distance from the consumed spot to the respawn point (cm). |
| `float RespawnOffsetMax` | 15000 | Largest distance from the consumed spot to the respawn point (cm). |
| `float MinDistanceFromPlayer` | 15000 | A respawn never appears closer than this to the player (cm). |
| `float MinSpacing` | 0 | Minimum distance to any other spawn of this sector, in the initial layout and on respawn (cm). 0 disables it. |

These are local distances around a point, so they stay in centimetres. They are an intended exception to the "no absolute distances" rule from the play-area task. Note the exception in the doc comments.

`bRememberWhenConsumed` and `RespawnDelay > 0` contradict each other.

## 2. A live-sector hook on steps (`Core/SectorGenStep.h`)

Respawning happens while the sector is being played, after generation has finished. Add two generic virtuals to `USectorGenStep`, so the director still knows no step by name:

```cpp
/** Called every frame while the sector is playable (director phase Idle). For steps that maintain the live sector, such as respawns. Default: does nothing. */
virtual void TickLive(UWorld& World, float DeltaSeconds);

/** Number of live actors of ActorClass (or a subclass) this step placed. Default: 0. */
virtual int32 CountLiveActors(const UClass* ActorClass) const;
```

**Director:**
- In `Tick`, while `Phase == Idle` and a current sector exists, call `TickLive` on every step.
- Add:

```cpp
/** Number of live sector-spawned actors of ActorClass (or a subclass), summed over all steps. */
UFUNCTION(BlueprintPure, Category = "ProcSector")
int32 CountLiveSpawns(TSubclassOf<AActor> ActorClass) const;
```

## 3. Spacing in the initial plan (`FZoneSpawnPlanner`)

- When an entry has `MinSpacing > 0`, redraw a planned point that lies closer than `MinSpacing` to any point already in the plan (any entry, any zone). Allow at most 8 redraws, then accept the last one.
- Only redraw the point. Do not touch the count roll or the yaw draw.
- With `MinSpacing == 0`, the draw sequence must be exactly what it is today, so existing layouts are unchanged.
- Spawn ids keep their meaning (plan order). Positions of entries with spacing on will move; mention it in the summary.

## 4. Respawn queue (`USectorGenStep_Population`)

The plan entries need to remember their zone index and their `FZoneSpawnEntry` settings (or a copy of the needed fields), so a respawn knows its rules.

**On consumption** (`TryConsumeActor`): if the entry's `RespawnDelay > 0`, add a pending respawn after releasing the actor:
- spawn id
- class
- zone index
- consumed location
- due time (`World.GetTimeSeconds() + RespawnDelay`)
- an attempt counter

Use a `TArray`, no `TMap`.

**In `TickLive`:** for each due entry, choose a point:
1. Take a random direction (keep vertical spread within the zone's height) and a random distance in [`RespawnOffsetMin`, `RespawnOffsetMax`] from the consumed location.
2. Accept the point only if all of these hold:
   - it lies inside the zone's ring and height (add `FPlayAreaMath::IsInsideRing(...)` next to `RandomPointInRing`, same unit-space maths)
   - it is inside the play-area box
   - it is at least `MinDistanceFromPlayer` from the player's pawn (fall back to `FViewAnchor` when there is no pawn)
   - it is at least `MinSpacing` from every live spawned actor of this step
3. Try up to 8 points. If none is accepted, push the due time back 5 seconds and try again later; the old volume did the same.
4. On success, `Acquire` from the pool with a random yaw, and track it like an initial spawn with the same spawn id, so `ConsumeSpawnedActor` and `Clear` keep working.

**Randomness:** respawns run at runtime and depend on the player, so they are not part of the repeatable layout. They must still use a seeded stream, not `FMath::Rand`, per the conventions. Seed each respawn with `DeriveSeed(Context.Seed, HashCombine(SpawnId, AttemptCounter))` and a new step-specific salt.

**Clearing and counting:**
- `Clear` drops all pending respawns, so nothing from the old sector respawns in the new one.
- `CountLiveActors` counts tracked live actors whose class is a child of the given class.

## 5. Validation (`UEnvironmentPreset::IsDataValid`)

- Error: `RespawnDelay > 0` together with `bRememberWhenConsumed`. Message: choose one; respawning entries are not remembered.
- Error: `RespawnOffsetMax < RespawnOffsetMin`, or any of the new values negative.

## 6. Debug

The overlay gains a line: `Respawns pending: N`, summed through a new virtual on `USectorGenStep` (default 0), the same way as `CountLiveActors`.

## 7. Built-in catalog (`FDefaultCatalogFactory`)

Match the old volumes:
- Enemy entries: `RespawnDelay` 30, `MinDistanceFromPlayer` 15000, `bRememberWhenConsumed` false.
- Loot entries: `RespawnDelay` 60, offsets 3000 to 15000, `MinSpacing` 8000, `bRememberWhenConsumed` false.
- Station: unchanged (no respawn, remembered).

## 8. Startech side

**Loot contents.** `ALootScatterVolume` rerolled empty loot up to 20 times. Zones place the pickup directly, so the pickup must roll its own contents:
- Do the roll in the pickup's `ResetForNewLife()`, so it runs from `OnAcquiredFromPool` on every spawn and respawn.
- Reroll up to 20 times when the result is empty.
- If you report that pickups can still end up empty, they should despawn through `Despawn` as before.

**Old volumes.** Leave the `AEnemySpawnVolume` and `ALootScatterVolume` classes in the code; nothing places them anymore. In the summary, list every asset that still references them (for example `BP_WorldLoot`), so they can be deleted by hand.

**Quest and score code.** If anything used `GetAliveCount()` on a volume, switch it to `USectorDirectorSubsystem::CountLiveSpawns`.

## Out of scope

- Keeping initial spawns away from the arrival gate.
- Waves or trigger-based spawning.
- Box-shaped or off-centre zones.

## Acceptance

1. Builds cleanly (command in `Plugins/ProcSector/CLAUDE.md`).
2. With every `MinSpacing` and `RespawnDelay` at 0, generated layouts are identical to before. Check by comparing a few asteroid, enemy and loot positions before and after, in the same sector with the same seed.
3. The documentation is updated: section 8 (authoring: the new entry fields, with the rule "respawn or remember, not both"), section 9 (integration: `CountLiveSpawns`, the pickup rolling its own contents), the API reference entries, and the old volumes marked as replaced.
4. The summary ends with manual editor steps, at least:
   - In `DA_Preset_Terra` and `DA_Preset_Rust`, loot entries: untick Remember When Consumed, set Respawn Delay 60, offsets 3000 to 15000, Min Spacing 8000. Enemy entries: Respawn Delay 30, Min Distance From Player 15000.
   - Delete `BP_WorldLoot` and any other asset still based on the old volumes, after confirming nothing references them.
5. Manual test plan for the summary, with `ProcSector.Debug 1`:
   - Pick up loot. `Respawns pending` rises by one. After about 60 s a pickup appears 30 to 150 m from where it was, never on the exact spot, and the counter drops.
   - Stand still where the respawn point would be. It waits, then appears once you're 150 m or more away.
   - Kill an enemy. A replacement appears after about 30 s, away from you.
   - Warp out with respawns pending and come back. Nothing respawns in the wrong sector; the sector starts fresh.
   - Loot looks spread out, with no two pickups closer than 80 m.
   - A pickup never spawns empty.
