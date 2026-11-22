# ProcSector plugin

Place this file at `Plugins/ProcSector/CLAUDE.md`. It applies to all work inside the plugin folder.

ProcSector regenerates one persistent Unreal level into different seeded space sectors behind a jump-gate warp. No level is ever loaded or unloaded: the level's contents are cleared and rebuilt while the screen is faded out.

- Engine: Unreal Engine 5.8 (C++). Runtime module `ProcSector`, export macro `PROCSECTOR_API`, log category `LogProcSector`.
- Host project: Startech (single-player space game). The plugin must not depend on any Startech class; Startech depends on the plugin, never the reverse.
- Full guide and API reference: `Plugins/ProcSector/Docs/DOCUMENTATION.md`. The code is the source of truth when they disagree.

## Folder layout (`Source/ProcSector/Public` and `Private` mirror each other)

| Folder | Contents |
|---|---|
| `Data/` | `SectorTypes.h` (all structs and enums), `UEnvironmentPreset`, `USectorCatalog`, `UProcSectorSettings`, `FSectorStateStore`, `FPlayAreaMath` (play-area fractions to world positions) |
| `Factories/` | Every class that creates objects or derives values: `FEnvironmentParamsFactory`, `FStarSystemSkyFactory`, `FSectorGenStepFactory`, `FEnvironmentActorFactory`, `FDefaultCatalogFactory` |
| `Core/` | `USectorDirectorSubsystem` (the facade) and `USectorGenStep` (step base class) |
| `Steps/` | One `USectorGenStep` subclass per concern |
| `Environment/` | Display actors (`APlanetActor`, `ASectorLightingRig`, `ASpaceBackdrop`, `ACelestialSkyActor`, `AAsteroidField`), `FViewAnchor`, `ASectorPreviewer`, `UPlayAreaSyncComponent` (sizes a host bounds box from the play area) |
| `Population/` | `UActorPoolSubsystem`, `IPoolable`, `FZoneSpawnPlanner`, relevance subsystem and component, placeholder actors |
| `Gates/` | `AJumpGate`, `UWarpTransitionComponent` |
| `Debug/` | Overlay subsystem (`ProcSector.Debug 1`; `2` also draws the play-area box) and console commands (`ProcSector.Goto`, `.Regenerate`, `.RerollSeeds`, `.List`) |

## How a regeneration runs

1. `UWarpTransitionComponent::BeginWarp` locks input and fades out. When the screen is covered it calls `USectorDirectorSubsystem::RequestSector`.
2. `RequestSector` builds the `FSectorGenContext`, in this exact order:
   1. `FEnvironmentParamsFactory::Build(Preset, Seed)`: preset values plus seeded jitter.
   2. `FStarSystemSkyFactory::Apply`: overrides the sun, the sun disc and the sky bodies from the star system (only if the sector has a `BodyId`).
   3. `FEnvironmentParamsFactory::ResolvePlanetshine`: derives planetshine from the final sun and planet.

   Anything derived from other values goes after what it depends on.
3. Director `Tick`: phase `TearingDown` calls `Clear` on every step. Phase `Generating` runs each step's `Begin` once, then `TickSlice` per frame until it returns true, within `GenerationBudgetMs` per frame.
4. `OnSectorReady` fires. The warp component moves the ship to the arrival gate, fades in and unlocks input.

Default step order (`FSectorGenStepFactory::GetDefaultStepClasses`): Lighting, Backdrop, Planet, CelestialSky, Asteroids, Gates, Population.

## Rules that must not be broken

### Determinism
Every random value comes from an `FRandomStream` seeded from the sector seed, drawn in a fixed order. The same seed must always produce the same sector.
- Never reorder, remove or insert random draws inside an existing sequence. Add new draws at the end of the sequence (see the `NebulaColorB` line at the end of `FEnvironmentParamsFactory::Build`), or give the new code its own stream with `DeriveSeed(Context.Seed, <new unique salt>)`.
- Each step that needs randomness owns a unique `SeedSalt` constant. Asteroids use `0xA57E401D` and Population uses `0x50B0C1A7`. New steps pick a new constant.
- If a change knowingly alters existing layouts, say so in the summary.

### Stable ids
Asteroid id = generation order. Spawn id = position in the spawn plan. `FSectorState` stores these ids to remember mined asteroids and consumed spawns. Anything that changes counts or generation order changes the ids and invalidates stored state. Call it out when it happens.

### Pooling
- Gameplay actors (enemies, loot, stations, gates) come from `UActorPoolSubsystem::Acquire`, never from `SpawnActor` directly.
- Never `Destroy` a pooled actor. Use `UActorPoolSubsystem::Release`, or `USectorDirectorSubsystem::ConsumeSpawnedActor` for actors the sector generated (that also records the removal when the spawn entry says so).
- `BeginPlay` runs once per actor, not once per use. Per-use resets belong in `IPoolable::OnAcquiredFromPool`, and shutdown in `OnReleasedToPool`.
- Release turns off every component tick and clears every timer on the actor and its components; acquire restores each component tick to `bStartWithTickEnabled` (`FComponentTickUtils`). `URelevanceComponent` sleeps and wakes component ticks the same way.

### Responsibilities
- The director knows no step by name. Add, remove or reorder steps only through `FSectorGenStepFactory` or the `StepClasses` setting.
- Display actors (`Environment/`) only apply the parameters they are given. They never choose values; choices live in the factories.
- A step builds and clears exactly one concern. `Clear` must be safe to call before any `Begin`.
- Long work in a step checks `FPlatformTime::Seconds()` against `DeadlineSeconds` and returns `false` to continue next frame.
- `FZoneSpawnPlanner` decides what spawns where and never touches the world.

### Rendering details already decided
- The planet (`bFollowViewer`) and `ACelestialSkyActor` are anchored to the camera through `FViewAnchor` and tick in `TG_PostUpdateWork`. They must never become reachable.
- The lighting rig has two directional lights. The sun has `ForwardShadingPriority = 1` and is the atmosphere sun light. Planetshine has `ForwardShadingPriority = 0`, no shadows, and is not an atmosphere light.
- Fog is off by default (`FogDensity = 0`). At this level's scale, densities above roughly 0.000002 turn the lower half of the view opaque.
- The planet and celestial bodies never cast shadows.

## Code conventions (the project owner's standing rules)

- No `TMap`. Use a `TArray` with linear search (`FindByPredicate`, `IndexOfByPredicate`); the collections here are small.
- Const correctness throughout: const parameters, const getters, const locals where possible.
- All logic in C++. Blueprint subclasses only assign meshes, materials and effects.
- Clean architecture even for jam work: single responsibility, factories for creation, the patterns listed in the documentation (Factory, Strategy, Facade, Object Pool, Observer, State, Command).
- Every class, struct, enum value, `UPROPERTY`, function and member variable in a public header has a `/** */` doc comment directly above it, saying what it is for (variables) or what it does (functions). The API reference in the documentation is generated from these comments, so a missing comment means a missing reference entry.
- `#include "X.generated.h"` is the last include in a header. Prefer forward declarations in headers and full includes in `.cpp` files.
- Log through `UE_LOG(LogProcSector, ...)`.
- Keep `SectorTypes.h` the single home for the plugin's data structs.

## Building and checking

The editor must be closed (or Live Coding disabled) before a command-line build:

```
"C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" StartechEditor Win64 Development -Project="D:\Unreal Projects\Startech\Startech.uproject" -WaitMutex
```

After a header change, a full build is required; hot reload is not enough.

Claude Code cannot open the editor or edit `.uasset` files (Blueprints, data assets, materials). When a change needs editor work, finish the C++ and end the summary with exact manual steps: which asset, which property, which value.

When behaviour or the public API changes:
- update the matching prose section of `Docs/DOCUMENTATION.md`
- update the entries for changed or added members in its API reference section, using the same wording as the new doc comments
