# Task: make Startech's enemies, stations and loot work with ProcSector pooling

Read `Plugins/ProcSector/CLAUDE.md` first. Its rules on pooling, determinism and conventions apply. The play-area task (`TASK_PlayArea.md`) is already done; build on that code.

This task changes **Startech's own source** (`Source/Startech/...`). Change the plugin only where a step below says so.

## Why this is needed

ProcSector never destroys the actors it spawns. Each actor is spawned once by `UActorPoolSubsystem`. When it is done, it is hidden and stored, and reused later, possibly in another sector. Three consequences:

1. **`BeginPlay` runs once per actor, not once per life.** State set there (health, dead flags, AI, effects) is not reset when the actor is reused.
2. **`Destroy()` breaks the pool.** The actor is gone, so the pool spawns a new one next time and the sector can't remember it was consumed.
3. **Anything that cleans up in `EndPlay`, `OnDestroyed` or by checking `IsValid` no longer fires on death.** A dead enemy is still a valid actor, just hidden. Targeting locks, radar lists, enemy registries, quest counters and anything else holding a reference will keep it unless they also react to release.

## Phase 0: discover, report, then stop

Before editing anything, read the Startech source and report:

- **Enemy classes:** the shared ship pawn base, the enemy classes (drifters, chasers, ranged and any others), and which class is the best home for pooling logic. The player must never be pooled, so prefer an enemy-only class over the shared base.
- **Per-life state:** health and shields, dead flags, weapon cooldowns, AI controller and behaviour tree or state tree, blackboard keys, movement, timers, attached VFX and audio, damage-flash materials, and anything else an enemy sets up in its constructor or `BeginPlay`.
- **Every despawn path:** every place an enemy, station or loot actor can leave play, including `Destroy`, `K2_DestroyActor`, `SetLifeSpan`, kill or bounds volumes (check `BP_SectorBounds` and its C++ parent, if any), and death handlers.
- **Everything that keeps references to these actors:** the targeting component, the targetable component, radar, any enemy registry or subsystem, quest or score counters, and HUD markers. For each, say how it currently learns that an actor is gone.
- **Loot:** how enemy drops are spawned, how pickup works, whether loot has a despawn timer, and whether sector-scattered loot and dropped loot share a class.
- **AI setup:** `AIControllerClass` and `AutoPossessAI` on each enemy class.

Write this as a short inventory with file and line references. Then **stop and wait for confirmation** before Phase 1. If anything below does not fit what you found, say so in the report instead of forcing it.

## Phase 1: one despawn function

Create one function every despawn path uses, so no path can be forgotten. For example, a static helper in a small Startech utility, or a function on a Startech world subsystem:

```cpp
/** Removes a gameplay actor from play. Sector-spawned actors go back to the plugin (which may remember the removal), other pooled actors go back to the pool, and everything else is destroyed. */
void Despawn(AActor* Actor);
```

Order of checks:

1. `USectorDirectorSubsystem::ConsumeSpawnedActor(Actor)` returns true: done.
2. The actor came from the pool but not from the sector, such as an enemy loot drop: `UActorPoolSubsystem::Release(Actor)`.
   - Tracking "came from the pool" is up to you; an interface, a tag or a component all work. Choose and justify it.
   - Do not call `Release` on actors the pool never handed out. It would store hand-placed actors in the pool.
3. Otherwise: `Destroy()`. Hand-placed actors keep working.

Then replace every path from the Phase 0 inventory with `Despawn`, including `SetLifeSpan` timers and bounds or kill volumes.

**Death sequence.** Run everything that must happen at the moment of death before `Despawn`, because the actor is hidden instantly:
- explosion VFX and sound, spawned at the location and not attached
- loot drop, rewards, notifications

If an enemy currently waits before destroying itself (for a death animation, say), keep the wait with a timer and call `Despawn` when it ends. Release clears its timers.

## Phase 2: reset on reuse (`IPoolable`)

Implement `IPoolable` (`Population/Poolable.h`) on the enemy class chosen in Phase 0.

**`OnAcquiredFromPool_Implementation`** restores the enemy to "fresh":
- full health and shields
- not dead
- weapon cooldowns cleared
- velocity zeroed with `GetMovementComponent()->StopMovementImmediately()`
- effects and materials back to normal
- AI restarted: `BrainComponent->RestartLogic()`, or re-run the behaviour or state tree, and clear stale blackboard keys such as the current target

**`OnReleasedToPool_Implementation`** stops everything that could keep running while hidden:
- `GetWorldTimerManager().ClearAllTimersForObject(this)`
- `BrainComponent->StopLogic(TEXT("Pooled"))`
- looping VFX and audio deactivated

It must also be safe on an enemy that is already dead.

Rules:
- Move per-life setup out of `BeginPlay` into one private `ResetForNewLife()`, and call it from both `BeginPlay` (for hand-placed enemies) and `OnAcquiredFromPool`. On first spawn both run, so `ResetForNewLife` must be idempotent.
- Set `AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned` in the enemy constructor. The pool spawns actors, so the default (placed in world only) leaves pooled enemies without AI.
- Apply the same pattern to stations if they can be destroyed, and to loot pickups (amount, despawn timer, glow).

## Phase 3: targeting, radar and every other reference

The targetable component implements `IPoolable` too:

- Add a targetable flag with a getter and an event (for example `OnTargetableChanged`).
- `OnReleasedToPool`: not targetable, broadcast.
- `OnAcquiredFromPool`: targetable, broadcast.
- The death sequence also sets it not targetable, so a dying enemy can't be locked during a death delay.

Then:

- **Targeting component:** drops a soft or hard lock as soon as its target stops being targetable. It also skips untargetable candidates when choosing targets.
- **Radar:** skips or removes untargetable or hidden actors.
- **Every other reference from the Phase 0 inventory:** give it a removal path that does not depend on `EndPlay`, `OnDestroyed` or `IsValid`.

## Phase 4: enemy loot drops

- **Dropping:** take the pickup from `UActorPoolSubsystem::Acquire` at the death location, not from `SpawnActor`.
- **Pickup and despawn timeout:** both go through `Despawn`. Sector-scattered loot is then remembered by the plugin, and dropped loot simply returns to the pool.
- **Warping:** dropped loot is not owned by the sector, so it would follow the player into the next sector. On `USectorDirectorSubsystem::OnSectorTransitionStarted`, release every dropped pickup still in the world.
  - Track live dropped pickups in a `TArray` of weak pointers. Per the conventions, no `TMap`.

## Phase 5: Startech root CLAUDE.md

Append this section to Startech's root `CLAUDE.md` (create the file if it doesn't exist):

```markdown
## Pooling (ProcSector)

Enemies, stations and loot are pooled by the ProcSector plugin (`Plugins/ProcSector`, read its CLAUDE.md before touching it).

- Never call `Destroy`, `K2_DestroyActor` or `SetLifeSpan` on enemies, stations or loot. Use `Despawn` (<file where it lives>).
- `BeginPlay` runs once per actor. Per-life state is set in `ResetForNewLife()`, which runs from `BeginPlay` and `OnAcquiredFromPool`.
- A dead enemy is still a valid actor. Never use `IsValid` alone to mean "alive" or "targetable"; use the targetable flag.
- Anything holding references to enemies must react to `OnTargetableChanged` or `OnReleasedToPool`, not only `EndPlay`.
- New enemy and loot classes implement `IPoolable` and set `AutoPossessAI` to `PlacedInWorldOrSpawned` (enemies).
- Enemy loot drops come from `UActorPoolSubsystem::Acquire`, never from `SpawnActor`.
```

Fill in the real file path.

## Conventions

- No `TMap`.
- Const correctness throughout.
- Logic in C++, Blueprint for visuals only.
- `/** */` doc comment on every new class, member and function.
- Prefer small, single-purpose additions over rewriting existing enemy code.

## Acceptance

1. Builds cleanly (build command in `Plugins/ProcSector/CLAUDE.md`).
2. No `Destroy`, `K2_DestroyActor` or `SetLifeSpan` remains on enemy, station or loot paths, except the fallback inside `Despawn`.
3. The summary lists every changed file, and ends with the manual editor steps, at least:
   - Set the real enemy, loot and station Blueprint classes in the zone spawn entries of `DA_EnvironmentPreset` (replacing the placeholders).
   - Check `AutoPossessAI` on each enemy Blueprint, since a Blueprint can override the C++ default.
   - Remove any Blueprint-side `Destroy` nodes found in Phase 0.
4. Manual test plan, with `ProcSector.Debug 1` on, to include in the summary:
   - Kill an enemy: it vanishes, the explosion plays, the overlay's pool "stored" count rises, and a locked target clears immediately.
   - Warp away and back several times: enemies come back at full health with working AI, and the pool total stops growing after the first visits.
   - Lock onto an enemy, warp out mid-fight, and come back: no lock on a hidden enemy, and no radar blips for released enemies.
   - Kill an enemy that drops loot, then warp before picking it up: the loot does not appear in the next sector.
   - Pick up sector-scattered loot, warp away and back: it stays gone.
   - A hand-placed enemy (not from the plugin) still dies and is destroyed normally.
