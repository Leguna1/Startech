# ProcSector: one level, many sectors

ProcSector regenerates a single persistent Unreal level into different seeded space sectors. The player flies into a jump gate, the screen fades, the level's contents are rebuilt behind the fade, and the player arrives in what looks like a different place. No level is loaded or unloaded.

This package contains two things:

- **`Plugins/ProcSector`**: the reusable system. This is the only part you move to your real project.
- **`SectorSandbox`**: a throwaway host project (a test ship and a game mode) that exists only so the plugin can be tested on its own.

Target engine: Unreal Engine 5.8 (written against 5.8.4).

## Status: read this first

The code was written without access to an Unreal build machine, so **it has not been compiled or run**. It was checked statically (include paths, brace balance, declaration and definition pairing, documentation coverage). Expect that the first build may surface a small number of compile errors, most likely a missing include or a signature that changed in 5.8. Section 12 lists the places I consider most likely to need a touch. Nothing in the design depends on those details.

## Contents

1. Folder layout
2. How it works
3. Design patterns and where they are used
4. Setup and first run
5. Test plan
6. Console commands and the debug overlay
7. Giving it real visuals
8. Authoring your own sectors
9. Making your own actors work with it
10. Extending the generator
11. Moving the plugin to another project
12. Known limits and troubleshooting
13. API reference (every class, variable and function)

---

## 1. Folder layout

```
SectorSandbox/
  SectorSandbox.uproject
  Config/                         project settings for the sandbox only
  Source/SectorSandbox/           test ship and game mode (do not migrate)
  Docs/DOCUMENTATION.md           this file
  Plugins/ProcSector/
    ProcSector.uplugin
    Source/ProcSector/
      Public|Private/
        Data/         structs, preset and catalog assets, settings, state store
        Factories/    everything that creates objects
        Core/         the director subsystem and the generation step base class
        Steps/        one generation step per concern
        Environment/  planet, lighting rig, backdrop, asteroid field, previewer, play-area sync component
        Population/   actor pool, spawn planner, relevance, placeholder actors
        Gates/        jump gate and the warp transition component
        Debug/        overlay and console commands
    Tools/
      build_asteroid_material.py  editor Python script that builds M_Asteroid (section 7)
```

The module's export macro is `PROCSECTOR_API`. The log category is `LogProcSector`.

## 2. How it works

### The regeneration cycle

```
Ship enters AJumpGate
   -> UWarpTransitionComponent::BeginWarpFromTrigger
         CanBeginWarp (game rule, if bound)       refused: OnWarpRefused, retry while inside the gate
   -> UWarpTransitionComponent::BeginWarp        lock input, fade out
   -> (screen fully covered)
   -> USectorDirectorSubsystem::RequestSector    build context from catalog + seed + saved state
   -> Phase TearingDown                          every step: Clear()
   -> Phase Generating                           each step: Begin(), then TickSlice() per frame
         Lighting -> Backdrop -> Planet -> CelestialSky -> Asteroids -> Gates -> Population
   -> OnSectorReady
   -> UWarpTransitionComponent::CompleteArrival  move ship to arrival gate, fade in, unlock input
```

The director spends at most `GenerationBudgetMs` (default 4 ms) per frame on generation, so the frame rate stays stable during the warp. The warp lasts as long as generation needs, with a minimum hold so a fast regeneration does not look like a flicker.

### Determinism and persistence

Every random decision is drawn from an `FRandomStream` seeded from the sector's seed, in a fixed order. The same sector therefore regenerates identically every time, which means nothing about the layout needs to be saved.

What the player changes is stored separately as small id lists per sector (`FSectorState`): asteroids that were mined, and spawned actors that were consumed and are flagged to be remembered. On regeneration those ids are skipped. Asteroid ids and spawn ids are simply the generation order, which is stable for a given seed.

### Who is responsible for what

| Class | Single responsibility |
|---|---|
| `USectorDirectorSubsystem` | Sequences the regeneration cycle. Knows no step by name. |
| `USectorGenStep` and subclasses | Each builds and clears exactly one concern. |
| `FEnvironmentParamsFactory` | Turns preset + seed into final parameters. |
| `FSectorGenStepFactory` | Creates the step pipeline. |
| `FEnvironmentActorFactory` | Finds or spawns the one-per-level environment actors. |
| `FDefaultCatalogFactory` | Builds the built-in test catalog. |
| `FZoneSpawnPlanner` | Decides what spawns where. Never touches the world. |
| `UActorPoolSubsystem` | Creates, stores and reuses actors. |
| `URelevanceSubsystem` | Decides when each relevance component is checked. |
| `URelevanceComponent` | Decides whether its owner sleeps, and applies it. |
| `FSectorStateStore` | Stores per-sector player changes. |
| `APlanetActor`, `ASectorLightingRig`, `ASpaceBackdrop`, `AAsteroidField` | Display the parameters they are given. They never choose them. |
| `AJumpGate` | Detects the ship and asks its warp component to warp. |
| `UWarpTransitionComponent` | Owns the warp sequence on the player side. |
| `USectorDebugSubsystem` | Displays state. Reads only. |

## 3. Design patterns and where they are used

- **Factory**: four factories, listed above. No class outside `Factories/` and the pool decides which concrete class to create. The director gets its steps from `FSectorGenStepFactory`, steps get their actors from `FEnvironmentActorFactory`, and all gameplay actors come from the pool.
- **Strategy / Template Method**: `USectorGenStep` defines the fixed lifecycle (`Clear`, `Begin`, `TickSlice`); each subclass supplies the behaviour. Reordering or replacing steps is a settings change.
- **Facade**: `USectorDirectorSubsystem` is the only class gameplay code needs to call (`RequestSector`, `ConsumeSpawnedActor`).
- **Object Pool**: `UActorPoolSubsystem` with the `IPoolable` reset hooks.
- **Observer**: `OnSectorReady`, `OnSectorTransitionStarted`, `OnWarpStarted`, `OnWarpFinished`, `OnRelevanceChanged`, `OnAsteroidRemoved`. The asteroid field, for example, does not know the director exists; it broadcasts, and the asteroid step forwards.
- **State**: the director's `ESectorPhase` and the warp component's `EWarpState` are small explicit state machines.
- **Command**: each console command is an object bound to one action.
- **Data-driven design**: sectors, presets, spawn tables and class choices are all data (assets and settings), not code.

Conventions followed throughout: no `TMap` (small arrays with linear search), const correctness on parameters and getters, all logic in C++ with Blueprint reserved for assigning meshes and materials.

## 4. Setup and first run

### Prerequisites

- Unreal Engine 5.8.x installed.
- A C++ toolchain for your platform (Visual Studio 2022 with the game development workload on Windows) and Rider or Visual Studio.

### Build

1. Unzip the package. Keep the folder name `SectorSandbox`.
2. Right-click `SectorSandbox.uproject` and choose **Generate project files** (or open the `.uproject` directly in Rider).
3. Build the `SectorSandboxEditor` target in the `Development Editor` configuration and launch the editor. If Unreal offers to rebuild missing modules when you double-click the `.uproject`, accept.

### Create the test level

The package contains no binary assets, so you create one empty level by hand. This takes a minute.

1. **File > New Level > Empty Level**. Do not use Basic or Open World; they contain lights that would stack with the plugin's lighting rig.
2. Save it as `Content/Maps/Sandbox`.
3. **Edit > Project Settings > Maps & Modes**: set both **Editor Startup Map** and **Game Default Map** to `Sandbox`. The default game mode is already `SandboxGameMode` (set in `Config/DefaultEngine.ini`).
4. Press **Play**.

You do not place anything in the level. On BeginPlay the director finds no environment actors, so the factory spawns them, and because no catalog is assigned it uses the built-in four-sector test catalog (Terra, Rust, Glacier, Jove).

### Controls (sandbox ship)

| Key | Action |
|---|---|
| W / S | Thrust forward / backward |
| A / D | Turn left / right |
| Space or E | Up |
| Left Ctrl or Q | Down |
| Left Shift | Boost |
| F | Mine the asteroid or consume the actor in front of the ship |
| ` (tilde) | Console |

### What you should see

- A tinted, lit scene with a large coloured sphere (the planet) in the distance.
- A grey cylinder at the level centre (station placeholder), red cones (enemies), yellow cubes (loot), and grey boxes (asteroids) filling the play area apart from a clearing around the station.
- Two flat discs with floating labels on the X axis, 1.85 km either side of the centre (the jump gates). Fly along +X from the start to reach the first one.
- An overlay in the top-left corner with the sector name, seed, play-area size, timings, pool and relevance counts.

Everything uses engine basic shapes at this stage. Section 7 covers real visuals.

## 5. Test plan

Run these in order. Each has an expected result.

1. **First generation.** Press Play. *Expected:* overlay shows `Sector: Terra`, phase `Idle`, and six step timings. Output Log contains `LogProcSector: Sector 'Terra' ready`.
2. **Warp through a gate.** Fly +X with boost into the disc labelled `Rust`. *Expected:* input locks, screen fades to black, then fades in with a rust-coloured planet and tint, more asteroids, and the ship placed just inside the gate labelled `Terra`, facing the centre. Overlay shows `Sector: Rust`.
3. **Determinism.** Note the position of a few asteroids and enemies near the arrival gate. Warp back to Terra and return to Rust. *Expected:* identical layout.
4. **Console regeneration.** Run `ProcSector.Regenerate`. *Expected:* the sector rebuilds in place with no visible change.
5. **Asteroid persistence.** Fly up to an asteroid, press F. *Expected:* it disappears. Warp away and back. *Expected:* it is still gone, and every other asteroid is unchanged.
6. **Remembered spawns.** Press F on a yellow cube. *Expected:* it disappears, pool `stored` count rises by one. Warp away and back. *Expected:* that cube is still gone.
7. **Non-remembered spawns.** Press F on a red cone, warp away and back. *Expected:* the cone is back (enemies respawn by design).
8. **Pool reuse.** Watch the overlay over several warps. *Expected:* `active + stored` stops growing after the densest sector (Jove) has been visited once. That shows actors are reused rather than spawned.
9. **Relevance.** Sit at the centre, then fly to the outer edge. *Expected:* the `awake` count changes as you move and is always well below `tracked`.
10. **Frame budget.** Run `stat unit`, then warp to Jove (3000 asteroids). *Expected:* no frame spike during the fade beyond a few milliseconds. Try `GenerationBudgetMs` at 1 and at 16 in Project Settings to see the trade-off between smoothness and warp length.
11. **Seed reroll.** Run `ProcSector.RerollSeeds`. *Expected:* a different layout and colours for the same sector name, and previously mined asteroids are back.
12. **Direct jump.** Run `ProcSector.Goto Glacier`. *Expected:* the sector changes immediately with no fade and the ship stays where it is.
13. **Busy guard.** Run `ProcSector.Goto Jove` twice quickly. *Expected:* the second is refused with a warning in the log.
14. **Data validation** (after you create your own assets in section 8). Give a catalog gate a destination that does not exist and save. *Expected:* a validation error naming the sector.

## 6. Console commands and the debug overlay

| Command | Effect |
|---|---|
| `ProcSector.Debug 1` / `0` | Show or hide the overlay. The sandbox game mode turns it on automatically. The overlay includes `Play area: X x Y x Z m`, the full size of the current sector's play area, and `Respawns pending: N`, the consumed spawns waiting to come back during this visit. |
| `ProcSector.Debug 2` | The overlay plus the current play-area box, drawn in orange around the level origin every frame. |
| `ProcSector.List` | Log every sector id in the catalog. |
| `ProcSector.Goto <SectorId>` | Regenerate as that sector without the warp sequence. |
| `ProcSector.Regenerate` | Rebuild the current sector with the same seed. |
| `ProcSector.RerollSeeds` | New random seeds for all sectors; forgets sector state. |

Filter the Output Log by `LogProcSector` to see only this system.

## 7. Giving it real visuals

All visuals are assigned in Blueprint subclasses; no C++ changes are needed.

1. Create Blueprint subclasses of `PlanetActor`, `SpaceBackdrop`, `AsteroidField` and `JumpGate`.
2. Assign materials and meshes in their defaults (see below).
3. Either place one of each in the level, or set them under **Project Settings > Plugins > ProcSector > Classes** so the factory spawns your subclasses.

Parameter names are properties on each actor, so you can rename them to match existing materials.

### Planet material (Surface, Default Lit)

| Parameter | Type | Meaning |
|---|---|---|
| `Color` | Vector | Primary surface colour |
| `SecondaryColor` | Vector | Secondary surface colour |
| `CloudColor` | Vector | Cloud colour |
| `CloudCoverage` | Scalar 0..1 | Cloud amount |
| `NoiseScale` | Scalar | Feature size |

Suggested graph: sample a tiling greyscale noise texture with UVs multiplied by `NoiseScale`; use it as the alpha of a `Lerp(Color, SecondaryColor)`. Sample a second noise, compare it against `1 - CloudCoverage` with a `SmoothStep`, and use the result to `Lerp` toward `CloudColor`. Feed Base Color. A noise texture is far cheaper than the procedural Noise node and looks the same at this distance.

Assign it to **Planet Material** on your planet Blueprint.

### Atmosphere material (Surface, Unlit, Blend Mode Additive)

| Parameter | Type | Meaning |
|---|---|---|
| `Color` | Vector | Rim colour |
| `Intensity` | Scalar | Brightness |

Graph: `Fresnel` (exponent about 3) multiplied by `Color` and `Intensity` into Emissive Color. Assign it to **Atmosphere Material**. The shell stays hidden until a material is assigned.

### Backdrop material (Surface, Unlit, Two Sided, "Is Sky" enabled)

| Parameter | Type | Meaning |
|---|---|---|
| `StarTint` | Vector | Star colour |
| `StarDensity` | Scalar 0..1 | Star amount |
| `NebulaColor` | Vector | First nebula layer colour |
| `NebulaColorB` | Vector | Second nebula layer colour |
| `NebulaIntensity` | Scalar | Nebula brightness |
| `SunDirection` | Vector | World-space direction toward the sun |
| `SunColor` | Vector | Sun disc colour |
| `SunCosRadius` | Scalar | Cosine of the sun disc's half-angle |
| `SunBrightness` | Scalar | Sun disc emissive strength |

The recommended route is the packed sky texture `T_SpaceMasks` (stars in red, two nebula layers in green and blue) and the editor script `Tools/build_backdrop_material.py`, which builds this whole material including the sun disc. Run it with Tools > Execute Python Script.

Manual graph: a high-frequency noise or star texture, thresholded by `StarDensity`, times `StarTint`; plus a low-frequency noise times `NebulaColor` and `NebulaIntensity`; sum into Emissive Color. Enabling **Is Sky** lets the lighting rig's real-time sky light pick the backdrop up as ambient light. Assign it to **Backdrop Material**. The backdrop stays hidden until a material is assigned.

### Asteroids and gates

- Asteroid Blueprint: select the `Instances` component, set your rock mesh and material. The material needs a vector parameter named `Color` (or change **Tint Parameter**). `Instances` is only used by presets without asteroid types; with types, each type brings its own mesh (see *Asteroid types* in section 8). The type components copy `Instances`' collision and shadow settings.

#### Asteroid meshes and material

Five rock meshes, `SM_Asteroid_Lumpy`, `SM_Asteroid_Potato`, `SM_Asteroid_Shard`, `SM_Asteroid_Cratered` and `SM_Asteroid_Chunk`, are generated as FBX files with baked vertex colours (darker cracks and craters) and `UCX_` convex hulls for collision. Import them into `/Game/Meshes/Asteroids` with **Import Vertex Color: Replace**, **Build Nanite** on, **Combine Meshes** on, and **Generate Missing Collision** off (the `UCX_` hulls are the collision).

`M_Asteroid` is built by `Plugins/ProcSector/Tools/build_asteroid_material.py`. Run it in the editor (Tools > Execute Python Script, with the Python Editor Script Plugin enabled); running it again rebuilds the existing asset. The material multiplies the meshes' baked vertex colours by the `Color` parameter (which receives each type's `Tint`) and varies brightness per instance with `PerInstanceRandom`, between the `BrightnessMin` and `BrightnessMax` parameters (0.75 and 1.25), so neighbouring rocks of one type do not look identical. `Roughness` is a parameter too. It is flagged for instanced static meshes and Nanite. Set it as the material on all five meshes.
- Gate Blueprint: replace the mesh on `GateMesh`. Resize the `Trigger` sphere to fit.

### Previewing without pressing Play

Place your planet, lighting rig, backdrop and asteroid field Blueprints in a level, add a `SectorPreviewer` actor, pick a preset and seed, and press **Apply Preview** in its Details panel. Press **Clear Preview Asteroids** before saving the level.

## 8. Authoring your own sectors

1. **Presets.** Content Browser > right-click > Miscellaneous > Data Asset > `EnvironmentPreset`. Fill in **Base Params** (planet, atmosphere, sun, ambient, backdrop, asteroids, zones). Set the variation amounts: **Hue Jitter Degrees**, **Scalar Jitter Fraction**, **Randomize Sun And Sky**.
2. **Zones.** Each zone is a ring around the level origin with spawn entries. Its size is given as fractions of the sector's play area (see below): `Inner Fraction` and `Outer Fraction` horizontally, `Height Fraction` vertically, each 0 to 1. For the station-at-centre layout use a zone with all three at 0 holding the station, then rings outward. Each entry either respawns during the visit (**Respawn Delay** above 0, for enemies and loot) or is remembered (**Remember When Consumed**, for stations and one-off loot), never both; see *Respawning and spacing* below.
3. **Catalog.** Data Asset > `SectorCatalog`. Add one entry per sector: id, display name, seed, preset, **Play Area Extent**, and gates (destination id and **Normalized Location**). For the arrival point to work, a sector you can arrive in from A needs a gate that leads back to A.
4. **Assign it.** Project Settings > Plugins > ProcSector > **Catalog**, and optionally **Start Sector Id**.

One preset can back many sectors: different seeds give different colours, sizes, sun angles and layouts within the preset's variation range.

### Respawning and spacing

Zone spawns can come back while the player is still in the sector, so no level-placed spawner volumes are needed. Each spawn entry has:

| Field | Default | Meaning |
|---|---|---|
| **Respawn Delay** | 0 | Seconds after a spawn is consumed (looted, killed) before it comes back. 0: gone until the next visit, as before. |
| **Respawn Offset Min** / **Max** | 3000 / 15000 | The respawn appears this far from where the old one was consumed, never on the exact spot (cm). |
| **Min Distance From Player** | 15000 | A respawn never appears closer than this to the player; it waits until the player is far enough (cm). |
| **Min Spacing** | 0 | Minimum distance to every other spawn of the sector, in the initial layout and on respawn (cm). 0 disables it. |

**Respawn or remember, not both.** An entry with Respawn Delay above 0 is never remembered, even with **Remember When Consumed** ticked, and data validation reports the combination as an error. Use Respawn Delay for regular enemies and farmable loot; use Remember When Consumed for stations and one-off loot.

These four distances are in centimetres, not play-area fractions: they describe a neighbourhood around one point, so they do not scale with the sector. That is an intended exception to the fractions rule.

How a respawn is placed: when its delay is up, up to 8 random points are tried at a random distance between the two offsets from the consumed spot, with the height aimed inside the zone's height band. A point is accepted only inside the zone's ring and height, inside the play-area box, at least Min Distance From Player from the player's pawn (the camera when there is no pawn), and at least Min Spacing from every live spawn of the sector. If none is accepted, the respawn waits 5 seconds and tries different points. Respawns use seeded streams, but because they depend on where the player is, they are not part of the repeatable layout. They keep the spawn id of the spawn they replace. Warping out drops every pending respawn.

Min Spacing in the initial layout: a planned point closer than Min Spacing to any point already planned (any entry, any zone) is redrawn, at most 8 times, then the last draw is kept. Redraws come from a separate stream, so entries with Min Spacing 0 keep exactly the layout they had before; only entries with spacing move. Spawn ids are unchanged.

The built-in catalog uses Respawn Delay 30 and Min Distance From Player 15000 for enemies, and Respawn Delay 60, offsets 3000 to 15000 and Min Spacing 8000 for loot. The station does not respawn and is remembered.

### Play area: one size per sector

Each sector stores the size of its playable space once, as `Play Area Extent` on its catalog entry: the half-size of a box centred on the level origin, in centimetres (default 200000 x 200000 x 50000, so a 4 km x 4 km x 1 km box). Nothing else in the plugin holds an absolute distance. Zones, asteroids and gates are authored as fractions of that box and multiplied by it per axis when the sector is generated:

| What | Fields | Meaning |
|---|---|---|
| Zone (`FZoneSpawnTable`) | `Inner Fraction`, `Outer Fraction`, `Height Fraction` | Ring between the two fractions of the horizontal half-size; height as a fraction of the half-height. |
| Asteroids (`FAsteroidFieldParams`) | `Inner Fraction`, `Outer Fraction`, `Height Fraction` | Same, interpreted per distribution (below). |
| Gate (`FGateLink`) | `Normalized Location` | Each axis -1 to 1; (0.925, 0, 0) is 92.5 percent of the way to the +X edge. |

So resizing a sector is one edit, and two sectors sharing a preset can still have different sizes. A non-square extent stretches rings into ellipses that still fit the box; `Height Fraction` 1 reaches the box's floor and ceiling. The bounds actor follows the same value through `UPlayAreaSyncComponent` (section 9), and `ProcSector.Debug 2` draws the box.

Data validation checks the fractions (0 to 1, outer not below inner), that every extent component is above 0, and that every gate's `Normalized Location` is within -1 to 1. A gate within 0.05 of the edge gets a warning: an arriving ship would start inside the bounds warning.

### The star system: making the sky tell you where you are

The catalog has a **Star System**: one sun and a list of planets (`Bodies`). Give each sector a **Body Id** naming the planet it sits next to. From then on that sector's sky is derived, not random:

- **Sun**: its direction comes from where the planet is on its orbit, and its disc shrinks with orbit radius, so an inner-planet sector has a large sun and an outer one a small sun. Sunlight comes from the visible sun, so the lit sides of every body agree.
- **Neighbours**: every other planet appears as a small lit sphere in its true direction. Jump from one planet to another and the one you left is in the sky behind you.
- **Moons**: listed on a body and shown only in that body's sector, placed relative to the near planet.

Directions are true to the layout; sizes are exaggerated so neighbours read as planets (at real scale they would be single pixels). Tune this on the star system: `Body Angular Radius At Unit Distance`, `Min/Max Body Angular Radius` and `Sun Angular Radius At Unit Orbit`. Orbits lie in the horizontal plane, so with a ship that stays level the sun and planets sit around the horizon; use `Orbit Height` to lift some above or below it.

Everything in the sky is anchored to the camera: the near planet (`Follow Viewer` on the planet actor), and the distant bodies (`CelestialSkyActor`). They keep their direction and size wherever the player flies and can never be reached. The sun disc is drawn by the backdrop material from the `SunDirection`, `SunColor`, `SunCosRadius` and `SunBrightness` parameters.

To give distant bodies your planet look, make a Blueprint subclass of `CelestialSkyActor`, set **Body Material** to your planet material, and assign the Blueprint under Project Settings > Plugins > ProcSector > Classes > **Celestial Sky Class**.

A sector with no Body Id (or a catalog with no bodies) keeps its preset sun and shows no neighbours.

### Planetshine

Space has no air to scatter light, so the side of anything facing away from the sun would be pitch black. The lighting rig adds a second, shadowless directional light coming from the near planet, tinted with its surface colour: the light reflected off the planet. Near Terra dark sides turn faintly blue, near Rust faintly red.

Its intensity follows the planet's phase: strongest when the sun is behind you and you see the planet's day side, weakest when the sun is behind the planet. Tune it per preset under **Ambient > Planetshine**:

- `Planetshine Strength`: fraction of the sun's intensity (default 0.1). 0 turns it off.
- `Planetshine Minimum Phase`: share kept when you see the night side (default 0.35), so dark sides never go fully black.

Direction, colour and final intensity are derived from the planet and sun and are not edited by hand.

### Asteroid distribution

`Asteroids > Distribution` in a preset chooses the volume. All distances are fractions of the sector's play area:

- **Box** fills the whole play-area box evenly, out to `Outer Fraction` horizontally and `Height Fraction` vertically, keeping out of a central ellipse of `Inner Fraction` so the station area stays clear. Points that land in the clearing are redrawn (at most 16 times, deterministically). The built-in catalog uses Box with 0.15 / 0.95 / 0.9.
- **Sphere** fills a shell between `Inner Fraction` and `Outer Fraction` in every direction, stretched per axis by the play area into an ellipsoid. `Height Fraction` is not used.
- **Belt** is a flat ring between the two fractions, limited vertically by `Height Fraction`.

A box or sphere has far more volume than a belt, so raise `Count` if it looks sparse; instanced asteroids stay cheap into the thousands. Counts do not scale with the play area: a larger sector with the same count is sparser. Spawn zones are still rings with their own `Height Fraction`; raise it if enemies and loot should spread vertically too.

### Asteroid types

`Asteroids > Types` in a preset lists the kinds of rock in the field, so each sector can have its own mix (icy chunks near Glacier, red rocks near Rust). Each type has:

| Field | Default | Meaning |
|---|---|---|
| **Mesh** | none | The type's mesh. Loaded while the screen is faded out. |
| **Material** | none | Optional override. Empty uses the mesh's own material. |
| **Weight** | 1 | Share of the field's `Count`, relative to the other types (3 next to 1 gives three times as many). |
| **Min Scale** / **Max Scale** | 2 / 12 | Per-axis scale range of this type. |
| **Tint** | dark grey-brown | Written to the material's `Color` parameter for this type. |

- **Empty `Types`** keeps the old behaviour: the field actor's own mesh, with the field-level `Min Scale`, `Max Scale` and `Tint`. Those three fields only apply when `Types` is empty.
- **3 to 5 types is the sweet spot.** Each type is its own instanced mesh component, so each type is one more draw call (more with several material slots); beyond five the variety gain is small.
- **Positions and ids do not depend on the types.** The type of each asteroid is picked by weight from a separate seeded stream, so adding, removing or reweighting types changes which mesh, size and tint each rock gets, but never where it is or its id. Mined asteroids stay mined. Changing `Count` or the distribution still moves everything, as before.
- Data validation reports a type with no mesh, a weight of 0 or less, or `Max Scale` below `Min Scale`.

Example (the five generated meshes from section 7): Lumpy 3, Potato 2, Shard 1.5, Cratered 1, Chunk 2.

## 9. Making your own actors work with it

### Spawned actors (enemies, loot, stations, NPCs)

Any actor class can go in a spawn entry. Because actors are pooled:

- `BeginPlay` runs once per actor, not once per use. Put per-use resets (health, AI state, velocity) in `IPoolable::OnAcquiredFromPool`, and shutdown in `OnReleasedToPool`. Implement the interface in C++ or Blueprint, on the actor or on any of its components.
- What the pool switches for you: on release it calls `OnReleasedToPool`, then turns off the actor's tick and **every component's tick**, clears **every timer** bound to the actor or one of its components, and turns off collision and rendering. On acquire it places the actor, restores the actor's tick and each component's tick to their class's `bStartWithTickEnabled`, turns collision and rendering back on, and then calls `OnAcquiredFromPool`. So `OnReleasedToPool` only needs to stop what is neither ticking nor on a timer (AI that runs elsewhere, effects, references held by other actors), and a component that should stay switched off in a new life must turn its tick off again in `OnAcquiredFromPool`.
- A component that turns its own tick on and off at runtime (for example only while projectiles are in flight) is reset to `bStartWithTickEnabled` on acquire, so clear that work in the pool hooks.
- A stored actor is still a valid actor, just hidden. Anything that keeps references to pooled actors (target lists, radar, quest counters) must react to release, not to `EndPlay` or `IsValid`.
- The pool spawns new actors with a plain `SpawnActor`, not a deferred spawn, so nothing can be set on the actor before its `BeginPlay`. Per-type settings must be class defaults: one Blueprint subclass per type.
- When an actor is killed, looted or otherwise removed, **do not call `Destroy`**. Call:

```cpp
GetWorld()->GetSubsystem<USectorDirectorSubsystem>()->ConsumeSpawnedActor(this);
```

  That returns it to the pool and, if its spawn entry is flagged, remembers the removal. If the entry has a **Respawn Delay**, it queues the respawn instead (see section 8).
- To count what is alive (quests, scores, wave logic), call `USectorDirectorSubsystem::CountLiveSpawns(ActorClass)`: the number of live sector-spawned actors of that class or a subclass. `CountPendingRespawns()` gives the number waiting to come back.
- **Loot fills itself.** A zone places the pickup actor straight from the pool, so nothing hands it contents. A loot class must roll its own contents in its per-life reset (called from `OnAcquiredFromPool`), on every spawn and every respawn. Startech's `ALootPickup` rolls its `LootTable` up to 20 times until something comes out, and despawns itself through the normal consume path if it is still empty. Assign a pickup Blueprint with a loot table to zone entries, not a spawner volume.

### Sleeping when far away

Add a `URelevanceComponent`. By default it turns the owner's tick and every component's tick off beyond `SleepDistance`, and on waking restores each to its class's `bStartWithTickEnabled`. A component that enables its own tick at runtime, or one that was switched off on purpose (a dead enemy's AI), is restored to that default on wake, so guard such logic in the component itself. Bind `OnRelevanceChanged` to pause AI, movement components or looping effects. Enable `bDisableCollisionWhenAsleep` or `bHideWhenAsleep` only where that is safe for gameplay.

### Mining asteroids

From a hit result:

```cpp
if (AAsteroidField* Field = Cast<AAsteroidField>(Hit.GetActor()))
{
    Field->RemoveAsteroid(Field->GetAsteroidIdFromHit(Hit));
}
```

The removal is recorded automatically. This works on every asteroid type: the field maps the hit component and instance to the asteroid's id.

To inspect an asteroid before acting on it (for example, later, to swap it for a physics actor), call `GetAsteroidInfo(Id, TypeIndex, Transform)`: it gives the type index (-1 when the preset has no types) and the world transform, and returns false for an unknown or already-removed id.

### The player ship

Add a `UWarpTransitionComponent` to your ship. Its root primitive must generate overlap events. Bind `OnWarpStarted` and `OnWarpFinished` for warp-tunnel effects and sound. If your ship reads input in a way that `DisableInput` does not stop, also check `IsWarping()` before applying input (the sandbox ship does this).

#### Refusing warps: `CanBeginWarp` and `OnWarpRefused`

The game can veto a warp without the plugin knowing why. Bind the native single-cast delegate `CanBeginWarp` (`bool (FName DestinationSectorId, FText& OutReason)`) from C++, usually in the ship's `BeginPlay`:

```cpp
Warp->CanBeginWarp.BindUObject(this, &AMyShip::CanWarp);
```

`BeginWarp` asks it last, after its own checks (not already warping, director idle, sector known), so a refusal is only reported for a warp that could otherwise start. Unbound, every warp is allowed. When it returns false, the warp does not start, `BeginWarp` returns false and `OnWarpRefused(Reason)` is broadcast with the text you set, for the HUD.

Gates call `BeginWarpFromTrigger(DestinationSectorId, Gate)` instead of `BeginWarp`. After a `CanBeginWarp` refusal it retries every `RefusedRetrySeconds` (default 0.25 s, four times a second) while the ship still overlaps the gate's trigger, so the warp starts by itself as soon as it is allowed. Retries are quiet: `OnWarpRefused` fires once per entry, not on every retry. Retrying stops when the ship leaves the trigger, when the gate is pooled away (its collision goes off), when a warp starts, or on `CancelWarpRetry()`. `IsRetryingWarp()` is true while a retry is pending, which the HUD can use to keep a refusal message up.

### Keeping the level bounds in sync: `UPlayAreaSyncComponent`

Your level probably has a bounds actor (a box that warns the player and then removes them when they leave). Instead of typing its size by hand, add a `UPlayAreaSyncComponent` to it:

1. Open the bounds actor's Blueprint and add the **Play Area Sync** component.
2. If the actor has more than one `UBoxComponent`, set **Box Component Name** to the one that marks the bounds. Left at None, the first box is used.
3. Set **Extent Padding** (default 10000, 10000, 5000) to how far outside the generated content the warning should start.
4. Remove any hard-coded extent the Blueprint sets on the box.

On `BeginPlay` the component subscribes to the director's `OnSectorReady` and, if a sector is already current, applies it immediately. On every sector ready it sets the box's unscaled extent to `GetPlayAreaExtent() + ExtentPadding`, so the bounds resize when the player warps between sectors of different sizes. If no box is found it logs one warning. Keep the bounds actor at the level origin with scale 1: the play area is centred on the origin and the component writes the unscaled extent. The component only sets the size; the warning and removal logic stays in the bounds actor.

Other code can read the same value with `USectorDirectorSubsystem::GetPlayAreaExtent()` (Blueprint pure). During a regeneration it already returns the sector being generated.

### Saving

`USectorDirectorSubsystem::GetStateStore().GetAllStates()` returns an array of plain `USTRUCT`s you can put in a `USaveGame`. On load, call `GetMutableStateStore().SetAllStates(...)` before the first `RequestSector`; turn off **Auto Generate On Begin Play** in the settings so you control that moment. Save the current sector id alongside.

## 10. Extending the generator

To add a new stage, for example hazard volumes:

1. Subclass `USectorGenStep`. Override `GetStepName`, `Begin`, `TickSlice` and `Clear`. If the work is long, check `FPlatformTime::Seconds()` against `DeadlineSeconds` and return `false` to continue next frame.
2. Take any random numbers from an `FRandomStream` seeded with `DeriveSeed(Context.Seed, YourSalt)`.
3. Get actors from `UActorPoolSubsystem` and return them in `Clear`.
4. Add any parameters you need to `FEnvironmentParams`.
5. List the steps, including yours, under Project Settings > Plugins > ProcSector > **Step Classes**, or add your class to `FSectorGenStepFactory::GetDefaultStepClasses`.

The director needs no change.

One rule protects existing sectors: never reorder or insert random calls in the middle of an existing sequence. Add new random draws at the end of a sequence or in their own stream.

## 11. Moving the plugin to another project

Because everything reusable is a plugin, migration is a folder copy.

1. Close the target project's editor.
2. Copy `Plugins/ProcSector` into the target project's `Plugins` folder (create it if needed). Do **not** copy `Source/SectorSandbox`, `Config`, or the `.uproject`.
3. Delete `Binaries` and `Intermediate` inside the copied plugin folder if they exist.
4. Open the target `.uproject` in a text editor and add to its `"Plugins"` array:

```json
{ "Name": "ProcSector", "Enabled": true }
```

5. In the target game module's `.Build.cs`, add `"ProcSector"` to `PublicDependencyModuleNames`, so your game code can include its headers.
6. Regenerate project files and build.
7. Copy over any content you made for it (presets, catalog, materials, Blueprint subclasses) with the editor's **Migrate** command, or place such content in the plugin's own `Content` folder from the start so it travels with the plugin.
8. In the target project, assign your catalog and classes under Project Settings > Plugins > ProcSector. These values are stored in `Config/DefaultGame.ini` under `[/Script/ProcSector.ProcSectorSettings]`; you can copy that section instead.

### Integration checklist for the target project

- Add `UWarpTransitionComponent` to the player ship.
- Remove directional lights, sky lights and fog from the level, or do not use `ASectorLightingRig` (remove the Lighting step from **Step Classes**).
- Replace the placeholder classes in your presets' spawn entries with your real enemy, loot and station classes.
- Route every "this actor is gone" path through `ConsumeSpawnedActor` instead of `Destroy`.
- Implement `IPoolable` on actors that hold per-use state.
- If the level uses World Partition, mark the environment actors (planet, lighting rig, backdrop, asteroid field) as not spatially loaded, or let the factory spawn them at runtime.
- Consider `r.DefaultFeature.AutoExposure=False` (the sandbox sets it) or a fixed exposure in a post-process volume; auto exposure tends to over-brighten mostly black scenes.
- Add `UPlayAreaSyncComponent` to your level-bounds actor and remove any hard-coded extent from it, so the bounds always match each sector's `Play Area Extent` (section 9). Set the extent per sector in the catalog; the default is 200000 x 200000 x 50000 cm.
- Turn the overlay off for shipping: it is off by default and only the sandbox game mode enables it.

## 12. Known limits and troubleshooting

### Limits

- One lighting setup and one scale are shared by all sectors. Sectors differ in look and content, not in structure.
- The play area is a box centred on the level origin; off-centre play areas are not supported. Spawn zones are rings (ellipses for a non-square play area) centred on the origin, so the enemy layout stays nested around the station. Asteroids fill a ring, an ellipsoidal shell or the box.
- Asteroid and spawn counts do not scale with the play area; a larger sector with the same counts is sparser.
- Changing a sector's `Play Area Extent` moves every asteroid, spawn and gate in it. Ids keep their meaning (generation order) but positions change.
- The star system is static: bodies do not move along their orbits.
- Asteroids are visual only: instanced meshes with simple blocking collision. Ships stop on contact; there is no mass, bounce, damage or breaking apart yet.
- A type whose mesh fails to load keeps its asteroids' ids and positions but draws nothing (a warning is logged).
- Removed asteroids are scaled to near zero rather than deleted, to keep ids stable. Their collision shrinks with them.
- Enemy movement is not synchronised with regeneration: actors that have moved are simply returned to the pool and re-placed.
- Sector state lives in memory until you save it (section 9).
- Pending respawns are not saved or remembered: leaving a sector drops them, and the next visit starts from the full layout (minus remembered entries).
- Zones are rings centred on the origin. A zone whose ring has no width (Inner and Outer Fraction equal, such as the station's core zone) leaves respawns no valid point, so they keep waiting; do not give such entries a Respawn Delay.

### If the first build fails

These are the spots most likely to need a small fix, since they could not be compiled here:

- `Misc/DataValidation.h` and the `IsDataValid(FDataValidationContext&)` signature in `EnvironmentPreset` and `SectorCatalog`.
- `UInstancedStaticMeshComponent::AddInstances` argument list in `AsteroidField.cpp`.
- `ExponentialHeightFogComponent` and `SkyLightComponent` setter names in `SectorLightingRig.cpp`.
- `BuildSettingsVersion.Latest` / `EngineIncludeOrderVersion.Latest` in the two `.Target.cs` files.
- A "missing include" error for a type: add the include it names to the `.cpp` reporting it.

### Runtime problems

| Symptom | Likely cause |
|---|---|
| Nothing generates | The level's game mode is not running as a game world, or **Auto Generate On Begin Play** is off. Check the log for `LogProcSector`. |
| Scene far too bright or washed out | Extra lights in the level, or auto exposure. Use an Empty Level and see the checklist above. |
| Planet or sky not visible | Planet: check `bVisible` and that the location is inside the backdrop sphere. Sky: no **Backdrop Material** assigned yet. |
| Gate does nothing | The ship has no `UWarpTransitionComponent`, or its root does not generate overlap events. |
| Gate does nothing, then warps later by itself | The game's `CanBeginWarp` refused the warp (see `OnWarpRefused`); it is retried while the ship stays inside the gate. |
| Ship arrives at its old position | The destination sector has no gate leading back to the sector you came from. |
| Bounds warning on arrival, or gates outside the bounds | The bounds actor has no `UPlayAreaSyncComponent`, is not at the origin or is scaled, or a gate's `Normalized Location` is too close to the edge (data validation warns). |
| A consumed actor reappears | Its spawn entry has **Remember When Consumed** off, or it was destroyed instead of passed to `ConsumeSpawnedActor`. |
| Layout changed after an edit | The order of random calls changed, or the preset or seed changed. Expected; see the rule in section 10. |
| Enemy state carries over between uses | Reset it in `OnAcquiredFromPool`. |

---

## 13. API reference

This section is generated from the documentation comments in the headers, so it matches the code exactly. It lists every type with its purpose, every variable with its purpose, and every function with what it does.

### ProcSector plugin

#### `ProcSectorLog.h`

- `DECLARE_LOG_CATEGORY_EXTERN(LogProcSector, Log, All)`: Log category used by every ProcSector class. Filter the Output Log by "LogProcSector".

#### `Core/SectorDirectorSubsystem.h`

- `DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSectorReady, FName, SectorId)`: Broadcast when a sector has finished generating and is playable.
- `DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSectorTransitionStarted, FName, FromSectorId, FName, ToSectorId)`: Broadcast when regeneration starts, before anything is cleared.

**`USectorDirectorSubsystem`** (class)

The single entry point for changing sectors (Facade pattern). It owns the regeneration cycle: clear the old sector, run each generation step within a per-frame time budget, then announce the new sector. It does not know what any step builds; it only sequences them. One instance exists per game world and is fetched with GetWorld()->GetSubsystem<USectorDirectorSubsystem>().

Variables:

- `OnSectorReady` (`FOnSectorReady`): Fires when a sector has finished generating.
- `OnSectorTransitionStarted` (`FOnSectorTransitionStarted`): Fires when regeneration starts.
- `Catalog` (`TObjectPtr<USectorCatalog>`): The catalog in use: the asset from the settings, or the built-in test catalog.
- `Steps` (`TArray<TObjectPtr<USectorGenStep>>`): The generation pipeline, in execution order. Created by FSectorGenStepFactory.
- `Context` (`FSectorGenContext`): Input handed to every step during the regeneration in progress.
- `StateStore` (`FSectorStateStore`): Remembered player changes per sector.
- `Phase` (`ESectorPhase`): Current phase of the cycle.
- `CurrentSectorId` (`FName`): Sector that is currently playable.
- `PreviousSectorId` (`FName`): Sector the player came from.
- `PendingSectorId` (`FName`): Sector being generated.
- `StepIndex` (`int32`): Index of the step currently running.
- `SeedSalt` (`int32`): Random offset added to every catalog seed; changed by RerollSeeds.
- `StepAccumulatedSeconds` (`double`): Working time the current step has used so far, in seconds.
- `PendingTimings` (`TArray<FSectorStepTiming>`): Timings collected during the regeneration in progress, published when it finishes.
- `LastTimings` (`TArray<FSectorStepTiming>`): Timings of the most recent finished regeneration.
- `LastGenerationMs` (`float`): Total working time of the most recent finished regeneration, in milliseconds.
- `bInitializedPipeline` (`bool`): True once EnsureInitialized has run.

Functions:

- `virtual void OnWorldBeginPlay(UWorld& InWorld) override`: Generates the start sector when the settings ask for automatic generation.
- `virtual void Tick(float DeltaTime) override`: Advances the regeneration cycle by one time-budgeted slice. While idle with a current sector, calls TickLive on every step instead.
- `virtual TStatId GetStatId() const override`: Stat id required by the tickable interface.
- `bool RequestSector(FName SectorId)`: Starts regenerating the level as the given sector. Returns false if busy or the sector is unknown.
- `bool RegenerateCurrentSector()`: Regenerates the current sector in place, for example after changing settings.
- `void RerollSeeds()`: Picks a new random salt for every seed, forgets all sector state and regenerates. A debugging aid.
- `bool IsBusy() const`: True while a regeneration is in progress.
- `ESectorPhase GetPhase() const`: Current phase of the regeneration cycle.
- `FName GetCurrentSectorId() const`: Id of the sector that is currently playable. NAME_None before the first generation finishes.
- `FName GetPreviousSectorId() const`: Id of the sector the player came from. NAME_None after the first generation.
- `int32 GetCurrentSeed() const`: Seed in effect for the current or pending sector.
- `FVector GetPlayAreaExtent() const`: Half-size of the current sector's playable box, centred on the level origin. During a regeneration, the sector being generated.
- `const FEnvironmentParams& GetCurrentParams() const`: Environment parameters of the current or pending sector.
- `const USectorCatalog* GetCatalog()`: The catalog in use, created or loaded on first need. Never null after the first call.
- `const USectorCatalog* GetCatalog() const`: The catalog in use, or nullptr if nothing has needed it yet.
- `bool GetArrivalTransform(FName FromSectorId, FTransform& OutTransform) const`: Computes where an arriving ship should appear: just inside the gate that leads back to FromSectorId, with the gate's NormalizedLocation resolved against the current sector's PlayAreaExtent. Returns false when the current sector has no such gate, in which case the ship should stay where it is.
- `void RecordAsteroidRemoved(const int32 AsteroidId)`: Stores that an asteroid of the current sector was removed, so it stays gone on later visits.
- `bool ConsumeSpawnedActor(AActor* Actor)`: Call when the player loots or destroys an actor that the sector generated. The owning step returns it to the pool and, if its spawn entry asks for it, the removal is remembered. Returns false when no step owns the actor.
- `int32 CountLiveSpawns(TSubclassOf<AActor> ActorClass) const`: Number of live sector-spawned actors of ActorClass (or a subclass), summed over all steps.
- `int32 CountPendingRespawns() const`: Number of consumed sector spawns waiting to respawn during the current visit, summed over all steps.
- `const TArray<FSectorStepTiming>& GetLastStepTimings() const`: Per-step timings of the most recent regeneration.
- `float GetLastGenerationMs() const`: Total working time of the most recent regeneration, in milliseconds.
- `const FSectorStateStore& GetStateStore() const`: Read-only access to the per-sector state, for saving.
- `FSectorStateStore& GetMutableStateStore()`: Mutable access to the per-sector state, for loading a save before the first RequestSector.
- `virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override`: Limits the subsystem to game and Play-In-Editor worlds.
- `void EnsureInitialized()`: Loads or creates the catalog and builds the step pipeline, once.
- `void TearDownCurrentSector()`: Calls Clear on every step.
- `void BeginCurrentStep()`: Calls Begin on the step at StepIndex and resets its timing accumulator.
- `void FinishGeneration()`: Marks the pending sector as current, returns to Idle and broadcasts OnSectorReady.
- `int32 ResolveSeed(const FSectorDefinition& Definition) const`: Combines a sector's catalog seed with the reroll salt.

#### `Core/SectorGenStep.h`

**`FSectorGenContext`** (struct)

Everything a generation step may read while building a sector. Filled by the director once per regeneration and passed to every step unchanged.

Variables:

- `World` (`UWorld*`): World being generated into.
- `PreviousSectorId` (`FName`): Sector the player is leaving; NAME_None for the very first generation.
- `Definition` (`FSectorDefinition`): Copy of the catalog entry of the sector being generated.
- `Params` (`FEnvironmentParams`): Final environment parameters, already randomised by the seed.
- `State` (`FSectorState`): Copy of the changes the player made to this sector on earlier visits.
- `Seed` (`int32`): Seed in effect for this sector (catalog seed plus any reroll salt).

**`USectorGenStep`** (class)

Base class for one stage of sector generation (Strategy pattern). The director calls Clear on every step, then for each step in order: Begin once, and TickSlice every frame until it returns true. A step has exactly one concern, such as "the asteroid field" or "the jump gates".

Functions:

- `virtual FName GetStepName() const`: Short name shown in the debug overlay and timing list.
- `virtual void Begin(const FSectorGenContext& Context)`: Called once before the first TickSlice. Resolve actors and prepare work here; do nothing slow.
- `virtual bool TickSlice(const FSectorGenContext& Context, const double DeadlineSeconds)`: Does a slice of work and returns true when the step is finished. Long steps must check FPlatformTime::Seconds against DeadlineSeconds and return false to continue next frame.
- `virtual void Clear(UWorld& World)`: Removes whatever this step generated for the previous sector. Must be safe to call before any Begin.
- `virtual bool TryConsumeActor(AActor& Actor, int32& OutSpawnId, bool& bOutRemember)`: Asks the step whether it owns Actor and, if so, to remove it from the sector. Returns true when handled. OutSpawnId and bOutRemember tell the director what to record.
- `virtual void TickLive(UWorld& World, float DeltaSeconds)`: Called every frame while the sector is playable (director phase Idle). For steps that maintain the live sector, such as respawns. Default: does nothing.
- `virtual int32 CountLiveActors(const UClass* ActorClass) const`: Number of live actors of ActorClass (or a subclass) this step placed. Default: 0.
- `virtual int32 CountPendingRespawns() const`: Number of consumed actors this step will bring back during the current visit. Default: 0.
- `static int32 DeriveSeed(const int32 SectorSeed, const uint32 StepSalt)`: Mixes the sector seed with a per-step constant so steps do not share a random sequence.

#### `Data/EnvironmentPreset.h`

**`UEnvironmentPreset`** (class)

Hand-authored template for a kind of destination (Earth-like, Mars-like). FEnvironmentParamsFactory combines a preset with a seed to produce the final parameters, so one preset yields many slightly different sectors.

Variables:

- `BaseParams` (`FEnvironmentParams`): The un-randomised parameters this preset starts from.
- `HueJitterDegrees` (`float`): Largest hue shift, in degrees, applied to each colour by the seed. 0 keeps colours exact.
- `ScalarJitterFraction` (`float`): Largest relative change applied to numeric values by the seed (0.15 = plus or minus 15 percent).
- `bRandomizeSunAndSky` (`bool`): True lets the seed pick the sun direction and sky rotation; false keeps the authored values.

Functions:

- `virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override`: Editor data validation: every zone and asteroid fraction lies within 0 to 1, outer fractions are not below inner ones, spawn counts and scales are consistent, no entry both respawns and is remembered, respawn offsets are ordered, no respawn or spacing value is negative, and every asteroid type has a mesh, a weight above 0 and MaxScale not below MinScale.

#### `Data/ProcSectorSettings.h`

**`UProcSectorSettings`** (class)

Project-wide configuration, shown under Project Settings > Plugins > ProcSector and saved to Config/DefaultGame.ini. Every value has a working default, so the plugin runs with nothing assigned.

Variables:

- `Catalog` (`TSoftObjectPtr<USectorCatalog>`): Catalog of sectors to use. Empty: a four-sector test catalog is generated in code.
- `StartSectorId` (`FName`): Sector generated when the level starts. None or unknown: the catalog's first sector.
- `bAutoGenerateOnBeginPlay` (`bool`): True generates the start sector automatically on BeginPlay. Turn off to call RequestSector yourself.
- `GenerationBudgetMs` (`float`): Milliseconds per frame the director may spend generating. Lower is smoother but takes more frames.
- `StepClasses` (`TArray<TSoftClassPtr<USectorGenStep>>`): Generation steps in execution order. Empty: the built-in pipeline is used.
- `AsteroidsPerBatch` (`int32`): Asteroid instances added per batch while filling the field.
- `GateArrivalDistance` (`float`): Distance from a gate, toward the level centre, at which an arriving ship is placed.
- `PlanetClass` (`TSoftClassPtr<APlanetActor>`): Planet actor class to spawn when the level contains none. Use a Blueprint subclass to assign materials.
- `LightingRigClass` (`TSoftClassPtr<ASectorLightingRig>`): Lighting rig class to spawn when the level contains none.
- `BackdropClass` (`TSoftClassPtr<ASpaceBackdrop>`): Backdrop class to spawn when the level contains none.
- `CelestialSkyClass` (`TSoftClassPtr<ACelestialSkyActor>`): Celestial sky class (distant planets and moons) to spawn when the level contains none.
- `AsteroidFieldClass` (`TSoftClassPtr<AAsteroidField>`): Asteroid field class to spawn when the level contains none.
- `GateClass` (`TSoftClassPtr<AJumpGate>`): Jump gate class spawned for every gate link.
- `RelevanceChecksPerFrame` (`int32`): Relevance components evaluated per frame. 32 re-checks 500 actors about every 16 frames.

Functions:

- `static const UProcSectorSettings& Get()`: Returns the settings object. Never null.
- `virtual FName GetCategoryName() const override`: Places the settings page under the "Plugins" section of Project Settings.

#### `Data/SectorCatalog.h`

**`USectorCatalog`** (class)

The list of every sector in the game and how their gates connect. Assign one in Project Settings > Plugins > ProcSector. If none is assigned, FDefaultCatalogFactory builds a four-sector test catalog in code.

Variables:

- `Sectors` (`TArray<FSectorDefinition>`): All sectors. Stored as an array and searched linearly; catalogs are small.
- `StarSystem` (`FStarSystemDefinition`): The sun and planets all sectors belong to. Leave Bodies empty to not use a star system.

Functions:

- `const FSectorDefinition* FindSector(const FName SectorId) const`: Returns the sector with the given id, or nullptr when there is none.
- `bool ContainsSector(const FName SectorId) const`: Returns true when a sector with the given id exists.
- `virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override`: Editor data validation: unique ids, presets assigned, every PlayAreaExtent component above 0, every gate leads to a real sector with a NormalizedLocation within -1 to 1 (warns when a gate is within 0.05 of the edge).

#### `Data/SectorStateStore.h`

**`FSectorStateStore`** (class)

Remembers what the player changed in each sector (mined asteroids, consumed spawns). Its only job is storage; the director decides when to record and when to read. To add save games, serialise the array returned by GetAllStates.

Variables:

- `States` (`TArray<FSectorState>`): One entry per sector that has changes. Searched linearly.

Functions:

- `const FSectorState* Find(const FName SectorId) const`: Returns the stored state for a sector, or nullptr when nothing was recorded.
- `FSectorState GetCopy(const FName SectorId) const`: Returns a copy of the sector's state, or an empty state carrying the id.
- `void RecordAsteroidRemoved(const FName SectorId, const int32 AsteroidId)`: Records that an asteroid was removed. Duplicate ids are ignored.
- `void RecordSpawnConsumed(const FName SectorId, const int32 SpawnId)`: Records that a remembered spawn was consumed. Duplicate ids are ignored.
- `void Reset()`: Forgets everything, for example after seeds are rerolled and ids no longer match.
- `const TArray<FSectorState>& GetAllStates() const`: Read-only access to every stored state, for saving.
- `void SetAllStates(const TArray<FSectorState>& InStates)`: Replaces every stored state, for loading.
- `FSectorState& FindOrAdd(const FName SectorId)`: Returns the sector's state, creating an empty one when missing.

#### `Data/PlayAreaMath.h`

**`FPlayAreaMath`** (struct)

Converts play-area fractions into world positions. A sector's playable space is a box centred on the level origin with half-size FSectorDefinition::PlayAreaExtent; everything the plugin places is authored in unit space (each axis -1 to 1) and multiplied per axis by that extent, so one edit resizes a sector. Header-only so the asteroid field and the spawn planner share exactly the same maths.

Functions:

- `static FVector ToWorld(const FVector& Normalized, const FVector& Extent)`: Converts a unit-space position (each axis -1 to 1) to a world position by multiplying each axis by the extent.
- `static FVector RandomPointInRing(FRandomStream& Stream, const float InnerFraction, const float OuterFraction, const float HeightFraction, const FVector& Extent)`: Picks a random point in a flat ring around the origin, evenly distributed over the ring's area, and returns it in world space. The fractions are clamped to 0 to 1 and OuterFraction is raised to InnerFraction when smaller. A non-square extent turns the ring into an ellipse that still fits the box. Draws exactly three values from the stream, in this order: radius blend, angle, height.
- `static bool IsInsideRing(const FVector& Location, const float InnerFraction, const float OuterFraction, const float HeightFraction, const FVector& Extent)`: True when the world position lies in the ring that RandomPointInRing draws from, with the same unit-space maths and the same clamping of the fractions: horizontal unit radius between Inner and Outer, unit height within HeightFraction. An axis with zero extent only accepts a coordinate of zero.
- `static bool IsInsidePlayArea(const FVector& Location, const FVector& Extent)`: True when the world position lies inside the play-area box (each axis within plus or minus the extent).
- `static bool ToUnit(const FVector& Location, const FVector& Extent, FVector& OutUnit)`: Converts a world position to unit space by dividing each axis by the extent; the inverse of ToWorld. An axis with zero extent maps to 0 when the coordinate is zero; otherwise returns false.

#### `Data/SectorTypes.h`

**`ESectorPhase`** (enum class)

Phase of the sector director's regeneration cycle.

Values:

- `Idle`: Nothing is being generated; the current sector is playable.
- `TearingDown`: The previous sector's generated content is about to be cleared.
- `Generating`: Generation steps are running, a few milliseconds per frame.

**`EAsteroidDistribution`** (enum class)

Shape of the volume asteroids are scattered in.

Values:

- `Belt`: A flat ring around the level origin between InnerFraction and OuterFraction, limited vertically by HeightFraction. Elliptical when the play area is not square.
- `Sphere`: A shell between InnerFraction and OuterFraction in every direction, stretched per axis by the play area into an ellipsoid. HeightFraction is not used.
- `Box`: The whole play-area box filled evenly, keeping out of a central ellipse of InnerFraction so the station area stays clear. OuterFraction scales X and Y, HeightFraction scales Z.

**`FPlanetParams`** (struct)

Look and placement of the background planet.

Variables:

- `bVisible` (`bool`): False hides the planet entirely (deep-space sectors).
- `Location` (`FVector`): World location of the planet centre. Keep it well outside the playable bounds.
- `Radius` (`float`): Planet radius in Unreal units.
- `SurfaceColorA` (`FLinearColor`): Primary surface colour (oceans on an Earth-like, dust on a Mars-like).
- `SurfaceColorB` (`FLinearColor`): Secondary surface colour (land masses, rock, ice).
- `CloudColor` (`FLinearColor`): Colour of the cloud layer.
- `CloudCoverage` (`float`): How much of the surface clouds cover, 0 (none) to 1 (overcast).
- `NoiseScale` (`float`): Scale of the surface noise in the planet material; larger means smaller features.
- `SpinDegreesPerSecond` (`float`): Planet spin around its vertical axis in degrees per second.

**`FAtmosphereParams`** (struct)

Look of the thin glowing shell around the planet.

Variables:

- `Color` (`FLinearColor`): Colour of the atmospheric rim glow.
- `Intensity` (`float`): Brightness multiplier of the rim glow. 0 disables the shell.
- `ThicknessScale` (`float`): Shell size relative to the planet (1.03 = 3 percent larger than the surface).

**`FSunParams`** (struct)

The sector's sun, implemented as one movable directional light.

Variables:

- `Rotation` (`FRotator`): Direction the sunlight travels in.
- `Color` (`FLinearColor`): Sunlight colour.
- `Intensity` (`float`): Directional light intensity in lux.

**`FAmbientParams`** (struct)

Fill light, fog and colour grading that tint the whole sector.

Variables:

- `SkyLightColor` (`FLinearColor`): Sky light colour (fills the shadowed side of objects).
- `SkyLightIntensity` (`float`): Sky light intensity.
- `FogColor` (`FLinearColor`): Fog colour; reads as a faint nebula haze over long distances.
- `FogDensity` (`float`): Exponential fog density. 0 turns fog off. At this level's scale use about 0.000002; larger values turn the lower half of the view opaque.
- `SceneTint` (`FLinearColor`): Post-process tint multiplied over the final image. White means no tint.
- `PlanetshineStrength` (`float`): Strength of the light reflected off the near planet, as a fraction of the sun's intensity. 0 turns planetshine off.
- `PlanetshineMinimumPhase` (`float`): Share of planetshine kept when the player sees the planet's night side (the sun behind it). 1 ignores the planet's phase; 0 is physically correct but leaves dark sides black again.
- `PlanetshineRotation` (`FRotator`): Direction the planetshine travels in, from the planet toward the player. Derived by FEnvironmentParamsFactory::ResolvePlanetshine.
- `PlanetshineColor` (`FLinearColor`): Planetshine colour, taken from the planet's surface colours. Derived.
- `PlanetshineIntensity` (`float`): Final planetshine intensity in lux, after strength and phase. 0 hides the light. Derived.

**`FBackdropParams`** (struct)

Look of the star and nebula sky sphere.

Variables:

- `StarTint` (`FLinearColor`): Colour multiplied over the stars.
- `StarDensity` (`float`): Star density, 0 (empty) to 1 (dense).
- `NebulaColor` (`FLinearColor`): Nebula colour.
- `NebulaColorB` (`FLinearColor`): Colour of the second nebula layer (the blue channel of the sky mask texture).
- `NebulaIntensity` (`float`): Nebula brightness. 0 hides the nebula.
- `Rotation` (`FRotator`): Rotation of the whole sky sphere, so two sectors never show the same sky.
- `SunDirection` (`FVector`): World-space direction from the viewer toward the sun. Derived from the sun light or the star system; not authored.
- `SunColor` (`FLinearColor`): Colour of the visible sun disc. Derived; not authored.
- `SunAngularRadiusDegrees` (`float`): Half the angle the sun disc covers in the sky, in degrees. The real sun seen from Earth is about 0.27.
- `SunBrightness` (`float`): Emissive strength of the sun disc. Values well above 1 make it bloom. 0 hides the disc.

**`FSkyBodyParams`** (struct)

One distant body (neighbouring planet or moon) as it appears in the current sector's sky. Derived from the star system.

Variables:

- `BodyId` (`FName`): Name of the body, for debugging.
- `Direction` (`FVector`): World-space direction from the viewer toward the body.
- `AngularRadiusDegrees` (`float`): Half the angle the body covers in the sky, in degrees.
- `ColorA` (`FLinearColor`): Primary surface colour.
- `ColorB` (`FLinearColor`): Secondary surface colour.

**`FAsteroidTypeParams`** (struct)

One kind of asteroid in a field: its mesh, look, size and share of the field's Count. Each type is drawn by its own instanced mesh component, so each type adds a draw call.

Variables:

- `Mesh` (`TSoftObjectPtr<UStaticMesh>`): Mesh drawn for this type. Loaded when the field is filled.
- `Material` (`TSoftObjectPtr<UMaterialInterface>`): Optional material for this type. Empty uses the mesh's own material.
- `Weight` (`float`): Share of the field's Count, relative to the weights of the other types (3 next to 1 is three times as many).
- `MinScale` (`float`): Smallest per-axis scale applied to this type's mesh.
- `MaxScale` (`float`): Largest per-axis scale applied to this type's mesh.
- `Tint` (`FLinearColor`): Colour written to the material's tint parameter ("Color") for this type.

**`FAsteroidFieldParams`** (struct)

Asteroids scattered around the level origin. Distances are fractions of the sector's FSectorDefinition::PlayAreaExtent.

Variables:

- `Distribution` (`EAsteroidDistribution`): Shape of the volume the asteroids fill.
- `Count` (`int32`): Number of asteroid instances to generate.
- `InnerFraction` (`float`): Inner edge of the field as a fraction of the play area's horizontal half-size (0 = the origin, 1 = the box edge). Box: radius of the central clearing.
- `OuterFraction` (`float`): Outer edge of the field as a fraction of the play area's horizontal half-size. Must not be below InnerFraction.
- `HeightFraction` (`float`): Vertical reach as a fraction of the play area's half-height; 1 reaches the floor and ceiling. Used by Belt and Box.
- `MinScale` (`float`): Smallest per-axis scale applied to an asteroid mesh. Only used when Types is empty; each type has its own range.
- `MaxScale` (`float`): Largest per-axis scale applied to an asteroid mesh. Only used when Types is empty; each type has its own range.
- `Tint` (`FLinearColor`): Colour sent to the asteroid material. Only used when Types is empty; each type has its own tint.
- `Types` (`TArray<FAsteroidTypeParams>`): Kinds of asteroid in the field, each taking a weighted share of Count. Empty: every asteroid uses the field actor's own mesh with MinScale, MaxScale and Tint above, exactly as before types existed. Three to five types is the sweet spot; each type is one more draw call.

**`FZoneSpawnEntry`** (struct)

One actor type to spawn inside a zone.

Variables:

- `ActorClass` (`TSoftClassPtr<AActor>`): Actor class to spawn (enemy, loot, station, NPC). Loaded when the sector is generated.
- `MinCount` (`int32`): Fewest instances of this class in the zone.
- `MaxCount` (`int32`): Most instances of this class in the zone.
- `bRememberWhenConsumed` (`bool`): True: once consumed (looted, destroyed) it stays gone when the player returns to the sector. False: it respawns on every visit, which suits regular enemies. Respawn or remember, not both: an entry with RespawnDelay above 0 is never remembered.
- `RespawnDelay` (`float`): Seconds after a spawn is consumed (looted, killed) before it respawns during the same visit. 0 keeps it gone until the next visit. Above 0, bRememberWhenConsumed is ignored.
- `RespawnOffsetMin` (`float`): Smallest distance from the consumed spot to the respawn point, in centimetres. A local distance around a point, so it is absolute: an intended exception to the play-area fractions.
- `RespawnOffsetMax` (`float`): Largest distance from the consumed spot to the respawn point, in centimetres. Not below RespawnOffsetMin. A local distance around a point, so it is absolute: an intended exception to the play-area fractions.
- `MinDistanceFromPlayer` (`float`): A respawn never appears closer than this to the player, in centimetres; it waits until the player is far enough. A local distance around a point, so it is absolute: an intended exception to the play-area fractions.
- `MinSpacing` (`float`): Minimum distance to any other spawn of this sector, in centimetres, in the initial layout and on respawn. 0 disables it. A local distance around a point, so it is absolute: an intended exception to the play-area fractions.

**`FZoneSpawnTable`** (struct)

A ring around the level origin and what spawns inside it. The ring is defined in unit space and multiplied per axis by the sector's FSectorDefinition::PlayAreaExtent, so a non-square play area gives an elliptical ring that still fits the box.

Variables:

- `ZoneName` (`FName`): Designer-facing zone name (Core, Inner, Middle, Outer). Used in logs only.
- `InnerFraction` (`float`): Inner edge of the ring as a fraction of the play area's horizontal half-size. 0 with OuterFraction 0 places everything at the origin.
- `OuterFraction` (`float`): Outer edge of the ring as a fraction of the play area's horizontal half-size; 1 touches the box edge. Must not be below InnerFraction.
- `HeightFraction` (`float`): Vertical reach of the zone as a fraction of the play area's half-height; 1 reaches the floor and ceiling.
- `Entries` (`TArray<FZoneSpawnEntry>`): Actor types spawned in this zone.

**`FEnvironmentParams`** (struct)

Everything that describes how one destination looks and what it contains.

Variables:

- `Planet` (`FPlanetParams`): Background planet.
- `Atmosphere` (`FAtmosphereParams`): Glow shell around the planet.
- `Sun` (`FSunParams`): Sun light.
- `Ambient` (`FAmbientParams`): Sky light, fog and scene tint.
- `Backdrop` (`FBackdropParams`): Star and nebula sky sphere.
- `Asteroids` (`FAsteroidFieldParams`): Asteroid belt.
- `Zones` (`TArray<FZoneSpawnTable>`): Spawn zones, usually ordered from the centre outward.
- `SkyBodies` (`TArray<FSkyBodyParams>`): Distant planets and moons visible from this sector. Filled from the star system; not authored.

**`FMoonDefinition`** (struct)

A moon of a body. Only shown while the player is in that body's sector.

Variables:

- `MoonId` (`FName`): Name of the moon, for debugging.
- `OffsetFromPlanet` (`FRotator`): Where the moon sits in the sky relative to its planet: pitch is up or down, yaw is left or right, in degrees.
- `RelativeRadius` (`float`): Apparent size as a fraction of the planet's apparent size (0.2 looks like a moon a fifth as wide).
- `ColorA` (`FLinearColor`): Primary surface colour.
- `ColorB` (`FLinearColor`): Secondary surface colour.

**`FCelestialBodyDefinition`** (struct)

A planet of the star system. Sectors refer to it by BodyId.

Variables:

- `BodyId` (`FName`): Unique name; FSectorDefinition::BodyId refers to it.
- `OrbitRadius` (`float`): Distance from the sun in arbitrary orbit units. Only the ratios between bodies matter (Earth 1, Mars 1.5, Jupiter 5.2).
- `OrbitAngleDegrees` (`float`): Position along the orbit, in degrees. Spread the bodies out so they appear in different parts of the sky.
- `OrbitHeight` (`float`): Height above the orbital plane in orbit units. Small values keep bodies from lining up exactly on the horizon.
- `BodyRadius` (`float`): Relative size of the body (Earth 1, Mars 0.5, Jupiter 11). Larger bodies look bigger from other sectors.
- `ColorA` (`FLinearColor`): Primary surface colour when seen from another sector.
- `ColorB` (`FLinearColor`): Secondary surface colour when seen from another sector.
- `Moons` (`TArray<FMoonDefinition>`): Moons shown while the player is in this body's sector.

**`FStarSystemDefinition`** (struct)

One sun and its planets. Every sector is a place in this system, so the sun's direction and size and the positions of the other planets are worked out from where the sector really is.

Variables:

- `SunColor` (`FLinearColor`): Colour of the sunlight and the sun disc in every sector.
- `SunAngularRadiusAtUnitOrbit` (`float`): Half-angle of the sun disc, in degrees, as seen from orbit radius 1. It shrinks in proportion further out. Exaggerated on purpose.
- `SunFalloffExponent` (`float`): How fast sunlight dims with orbit radius. 0 keeps every sector equally bright; 2 is physically correct and very dark far out.
- `BodyAngularRadiusAtUnitDistance` (`float`): Half-angle, in degrees, of a body of radius 1 seen from one orbit unit away. Raise it to make neighbours easier to see.
- `MinBodyAngularRadius` (`float`): Smallest half-angle a neighbouring body is drawn at, so distant ones stay visible as planets.
- `MaxBodyAngularRadius` (`float`): Largest half-angle a neighbouring body is drawn at, so none rivals the planet you are next to.
- `Bodies` (`TArray<FCelestialBodyDefinition>`): The planets. Empty: the star system is not used and sectors keep their preset sun.

**`FGateLink`** (struct)

One jump gate inside a sector.

Variables:

- `DestinationSectorId` (`FName`): Sector the gate leads to. Must exist in the same catalog.
- `NormalizedLocation` (`FVector`): Position of the gate inside the play area, each axis from -1 to 1, multiplied by the sector's PlayAreaExtent. Keep it at least 0.05 away from the edge, or the arriving player starts in the bounds warning.

**`FSectorDefinition`** (struct)

One destination the player can travel to.

Variables:

- `SectorId` (`FName`): Unique identifier used by gates, saves and console commands.
- `DisplayName` (`FText`): Player-facing name, shown on gate labels.
- `Seed` (`int32`): Seed for every random choice in this sector. Same seed, same sector.
- `BodyId` (`FName`): Planet of the catalog's star system this sector is next to. None: the sector keeps its preset sun and shows no neighbours.
- `Preset` (`TObjectPtr<UEnvironmentPreset>`): Preset the sector's environment is built from.
- `PlayAreaExtent` (`FVector`): Half-size of the playable box, centred on the level origin, in centimetres. Zones, asteroids and gates scale from it, and UPlayAreaSyncComponent sizes the bounds box from it.
- `Gates` (`TArray<FGateLink>`): Jump gates present in this sector.

**`FSectorState`** (struct)

Changes the player has made to a sector, reapplied whenever it is regenerated.

Variables:

- `SectorId` (`FName`): Sector these changes belong to.
- `RemovedAsteroidIds` (`TArray<int32>`): Ids of asteroids that were mined or destroyed.
- `ConsumedSpawnIds` (`TArray<int32>`): Spawn ids of remembered actors that were consumed (looted, destroyed).

**`FSectorStepTiming`** (struct)

How long one generation step took during the last regeneration.

Variables:

- `StepName` (`FName`): Name of the step, from USectorGenStep::GetStepName.
- `Milliseconds` (`float`): Total time the step spent working, summed over all frames, in milliseconds.

#### `Debug/SectorDebugSubsystem.h`

**`USectorDebugSubsystem`** (class)

Draws an on-screen overlay with the current sector, seed, play-area size, generation timings, pending respawns, pool usage and relevance counts. Only reads from the other subsystems. Toggle with the console variable "ProcSector.Debug 1" (0 hides it); "ProcSector.Debug 2" also draws the play-area box in the world.

Variables:

- `PlayAreaBoxLevel` (`static constexpr int32`): Console variable value from which the play-area box is drawn as well as the overlay.
- `FirstMessageKey` (`static constexpr int32`): First on-screen message key used by the overlay, chosen to avoid clashing with other debug messages.

Functions:

- `virtual void Tick(float DeltaTime) override`: Prints the overlay lines for this frame when the console variable is on, and draws the play-area box when it is 2 or more.
- `virtual TStatId GetStatId() const override`: Stat id required by the tickable interface.
- `virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override`: Limits the subsystem to game and Play-In-Editor worlds.
- `static void PrintLine(const int32 LineIndex, const FString& Text, const FColor& Color)`: Prints one overlay line. Each line uses its own key so it is replaced in place instead of scrolling.

#### `Environment/AsteroidField.h`

- `DECLARE_MULTICAST_DELEGATE_OneParam(FOnAsteroidRemoved, int32 /*AsteroidId*/)`: Broadcast when an asteroid is removed. The parameter is the asteroid's stable id.

**`FAsteroidSlot`** (struct)

Where one asteroid lives: which type's component holds it and at which instance index.

Variables:

- `TypeIndex` (`int32`): Index into FAsteroidFieldParams::Types, or INDEX_NONE for the untyped Instances component.
- `InstanceIndex` (`int32`): Instance index inside that component.

**`AAsteroidField`** (class)

Asteroids drawn as instanced meshes, so thousands cost a handful of draw calls. With FAsteroidFieldParams::Types empty, every asteroid is an instance of the Instances component's mesh. With types, each type gets its own instanced mesh component (created on demand and reused between sectors), and each asteroid takes a type by weight. They fill a flat belt, an ellipsoidal shell or the whole play-area box, chosen by FAsteroidFieldParams::Distribution; every distance is a fraction of the play-area extent passed to BeginFill. Filling is split into batches so it can be spread over several frames. An asteroid's id is its generation order, which is stable for a given seed; that is what makes "this asteroid was mined" survive a regeneration. Ids count across all types.

Variables:

- `OnAsteroidRemoved` (`FOnAsteroidRemoved`): Subscribe to learn about removed asteroids (the asteroid generation step records them).
- `Instances` (`TObjectPtr<UInstancedStaticMeshComponent>`): Holds every asteroid instance when FAsteroidFieldParams::Types is empty; cleared and hidden otherwise. Assign a different mesh or material in a Blueprint subclass. Type components copy its collision and shadow settings.
- `TintParameter` (`FName`): Vector parameter on the asteroid material receiving FAsteroidFieldParams::Tint, or each type's Tint.
- `RemovedScale` (`static constexpr float`): Scale given to removed asteroids. Keeping the instance alive keeps every other id stable.
- `MaxBoxRedraws` (`static constexpr int32`): Most redraws the Box distribution makes for a point inside the central clearing before accepting the last draw.
- `ActiveParams` (`FAsteroidFieldParams`): Parameters of the fill in progress or last completed.
- `ActivePlayAreaExtent` (`FVector`): Play-area half-size of the fill in progress or last completed, in centimetres.
- `Stream` (`FRandomStream`): Seeded random stream; advancing it in a fixed order is what makes the field repeatable.
- `TypeStream` (`FRandomStream`): Separate seeded stream for type picks, so choosing types never shifts the main stream's positions, rotations or scales.
- `TypeSeedSalt` (`static constexpr uint32`): Constant mixed into the fill seed for TypeStream.
- `PendingRemovedIds` (`TArray<int32>`): Ids that must be generated as removed during the current fill.
- `NextAsteroidId` (`int32`): Id the next generated asteroid will receive. Equals the number generated so far.
- `bFilling` (`bool`): True between BeginFill and the batch that completes the field.
- `TintMID` (`TObjectPtr<UMaterialInstanceDynamic>`): Runtime material instance of Instances that the tint is written to.
- `TypeComponents` (`TArray<TObjectPtr<UInstancedStaticMeshComponent>>`): One instanced mesh component per asteroid type, index-matched with FAsteroidFieldParams::Types. Extra ones are empty and hidden.
- `TypeMIDs` (`TArray<TObjectPtr<UMaterialInstanceDynamic>>`): Runtime material instance of each type component, index-matched with TypeComponents. Null when the type has no material.
- `TotalTypeWeight` (`float`): Sum of the active types' weights (negative weights count as 0).
- `Slots` (`TArray<FAsteroidSlot>`): Id to slot: where each generated asteroid lives. Indexed by asteroid id.
- `TypeInstanceIds` (`TArray<TArray<int32>>`): Per type, instance index to asteroid id, for resolving hits. Index-matched with FAsteroidFieldParams::Types.

Functions:

- `AAsteroidField()`: Creates the instanced mesh component and assigns the engine's basic cube mesh.
- `void BeginFill(const FAsteroidFieldParams& Params, const FVector& PlayAreaExtent, const int32 Seed, const TArray<int32>& RemovedIds)`: Clears the field and prepares a new seeded fill. Loads the type meshes and materials synchronously and sets up one component per type; unused components stay empty and hidden. PlayAreaExtent is the half-size of the sector's playable box; the fractions in Params are multiplied by it per axis. RemovedIds lists asteroids that must stay gone (taken from the sector state).
- `bool FillSlice(const double DeadlineSeconds)`: Adds batches of instances until the field is complete or the deadline passes. DeadlineSeconds is an absolute FPlatformTime::Seconds value. Returns true when complete.
- `void ClearField()`: Removes every instance from every component and cancels any fill in progress.
- `bool RemoveAsteroid(const int32 AsteroidId)`: Hides one asteroid and broadcasts OnAsteroidRemoved. Returns false if the id is invalid or already removed.
- `int32 GetAsteroidIdFromHit(const FHitResult& Hit) const`: Returns the asteroid id a trace hit, on any type's component, or INDEX_NONE when the hit was not on this field.
- `bool GetAsteroidInfo(const int32 AsteroidId, int32& OutTypeIndex, FTransform& OutTransform) const`: Returns the asteroid's type index (-1 when Types is empty) and its world transform. False if the id is unknown or removed.
- `int32 GetAsteroidCount() const`: Number of asteroid slots generated so far, including removed ones.
- `bool IsFilling() const`: True while a fill has been started and is not yet complete.
- `FTransform MakeNextTransform(int32& OutTypeIndex)`: Builds the transform of the next asteroid from the random stream and reports its type. The main stream's draws are the same with or without types, so adding types never moves an asteroid or changes an id. With types, the type is drawn by weight from TypeStream and the three scale draws are mapped onto that type's range.
- `int32 PickTypeIndex()`: Draws the type of the next asteroid by weight from TypeStream. Always exactly one draw.
- `void EnsureTypeComponents(const int32 Count)`: Creates type components until at least Count exist. Components are kept and reused between sectors.
- `void SetUpTypeComponent(const int32 TypeIndex, const FAsteroidTypeParams& Type)`: Gives the component for TypeIndex its mesh, material and tint, and shows it.
- `UInstancedStaticMeshComponent* GetComponentForType(const int32 TypeIndex) const`: The component holding asteroids of TypeIndex: Instances for INDEX_NONE, else that type's component. Null if missing.
- `FVector MakeSphereLocation()`: Draws the next asteroid location for the Sphere distribution: an even-volume shell in unit space, scaled per axis into an ellipsoid.
- `FVector MakeBoxLocation()`: Draws the next asteroid location for the Box distribution: even over the box, redrawn while inside the central clearing.

#### `Environment/CelestialSkyActor.h`

**`ACelestialSkyActor`** (class)

Draws the distant planets and moons of the current sector as small lit spheres. The actor follows the camera, so the bodies behave like part of the sky: they keep their direction and size wherever the player flies and can never be reached. Each body is its own sphere so it can reuse the planet material; there are only a handful.

Variables:

- `SceneRoot` (`TObjectPtr<USceneComponent>`): Root that is moved to the camera; every body sphere is attached to it.
- `BodyMesh` (`TObjectPtr<UStaticMesh>`): Mesh used for every body. The engine's basic sphere by default.
- `BodyMaterial` (`TObjectPtr<UMaterialInterface>`): Optional material for the bodies; use your planet material so neighbours match it. Empty: the mesh's own material.
- `MeshBaseRadius` (`float`): Radius of BodyMesh at scale 1. The engine's basic sphere is 50.
- `BodyDistance` (`float`): Distance from the camera at which bodies are placed. It must be farther than the near planet (so that planet can hide them) and, added to the level radius, smaller than the backdrop sphere's radius.
- `ColorAParameter` (`FName`): Vector parameter receiving FSkyBodyParams::ColorA. "Color" matches BasicShapeMaterial and the planet material.
- `ColorBParameter` (`FName`): Vector parameter receiving FSkyBodyParams::ColorB.
- `BodyComponents` (`TArray<TObjectPtr<UStaticMeshComponent>>`): One sphere per body slot.
- `BodyMIDs` (`TArray<TObjectPtr<UMaterialInstanceDynamic>>`): Runtime material instance of each sphere, index-matched with BodyComponents.

Functions:

- `ACelestialSkyActor()`: Creates the root and loads the engine's basic sphere mesh.
- `void ApplyParams(const TArray<FSkyBodyParams>& Bodies)`: Shows one sphere per sky body with the given direction, size and colours, and hides any spare spheres.
- `virtual void Tick(float DeltaSeconds) override`: Moves the actor to the camera every frame, after the camera has been updated.
- `void EnsureBodyCount(const int32 Count)`: Creates sphere components until at least Count exist. Components are kept and reused between sectors.

#### `Environment/PlanetActor.h`

**`APlanetActor`** (class)

The background planet: one sphere plus a slightly larger atmosphere shell. It only knows how to display the parameters it is given; it never chooses them. One instance lives in the level and is restyled on every jump.

Variables:

- `bFollowViewer` (`bool`): True treats FPlanetParams::Location as an offset from the camera (a skybox-style planet). False places it in the world.
- `SceneRoot` (`TObjectPtr<USceneComponent>`): Root that the planet is positioned by.
- `PlanetMesh` (`TObjectPtr<UStaticMeshComponent>`): The planet surface sphere.
- `AtmosphereMesh` (`TObjectPtr<UStaticMeshComponent>`): The atmosphere shell sphere, hidden until AtmosphereMaterial is assigned.
- `Spin` (`TObjectPtr<URotatingMovementComponent>`): Spins the planet mesh at the rate given by the parameters.
- `PlanetMaterial` (`TObjectPtr<UMaterialInterface>`): Optional surface material. Empty: the mesh's own material is used (BasicShapeMaterial by default).
- `AtmosphereMaterial` (`TObjectPtr<UMaterialInterface>`): Optional atmosphere material (translucent or additive fresnel). Empty: the shell stays hidden.
- `MeshBaseRadius` (`float`): Radius of the sphere mesh at scale 1. The engine's basic sphere is 50.
- `SurfaceColorAParameter` (`FName`): Vector parameter receiving FPlanetParams::SurfaceColorA. "Color" matches BasicShapeMaterial.
- `SurfaceColorBParameter` (`FName`): Vector parameter receiving FPlanetParams::SurfaceColorB.
- `CloudColorParameter` (`FName`): Vector parameter receiving FPlanetParams::CloudColor.
- `CloudCoverageParameter` (`FName`): Scalar parameter receiving FPlanetParams::CloudCoverage.
- `NoiseScaleParameter` (`FName`): Scalar parameter receiving FPlanetParams::NoiseScale.
- `AtmosphereColorParameter` (`FName`): Vector parameter on the atmosphere material receiving FAtmosphereParams::Color.
- `AtmosphereIntensityParameter` (`FName`): Scalar parameter on the atmosphere material receiving FAtmosphereParams::Intensity.
- `ViewOffset` (`FVector`): Offset from the camera to the planet centre, taken from FPlanetParams::Location.
- `PlanetMID` (`TObjectPtr<UMaterialInstanceDynamic>`): Runtime instance of the surface material that parameters are written to.
- `AtmosphereMID` (`TObjectPtr<UMaterialInstanceDynamic>`): Runtime instance of the atmosphere material. Null while no AtmosphereMaterial is assigned.

Functions:

- `APlanetActor()`: Creates the components and assigns the engine's basic sphere mesh.
- `void ApplyParams(const FPlanetParams& Planet, const FAtmosphereParams& Atmosphere)`: Moves, scales and recolours the planet and its atmosphere to match the parameters.
- `virtual void Tick(float DeltaSeconds) override`: Keeps the planet at a fixed offset from the player's camera, so it never gets closer however far the player flies.
- `void EnsureMaterialInstances()`: Creates the dynamic material instances on first use.
- `void UpdateFollowLocation()`: Places the planet at ViewOffset from the camera, or from the level origin when there is no camera.

#### `Environment/SectorLightingRig.h`

**`ASectorLightingRig`** (class)

Owns every light-related component of the level: sun, planetshine, sky light, fog and a scene tint. It only applies the parameters it is given. If you place your own lights in the level, remove them or they will stack with this rig.

Variables:

- `SceneRoot` (`TObjectPtr<USceneComponent>`): Root component; the rig's position does not matter.
- `Sun` (`TObjectPtr<UDirectionalLightComponent>`): The sun. Movable so colour and direction can change at runtime.
- `Planetshine` (`TObjectPtr<UDirectionalLightComponent>`): Light reflected off the near planet. No shadows; fills the side of objects facing away from the sun.
- `SkyLight` (`TObjectPtr<USkyLightComponent>`): Fill light. Uses real-time capture so it follows the backdrop and fog automatically.
- `Fog` (`TObjectPtr<UExponentialHeightFogComponent>`): Thin fog that gives each sector a coloured haze.
- `PostProcess` (`TObjectPtr<UPostProcessComponent>`): Unbound post-process volume used only for the scene colour tint.

Functions:

- `ASectorLightingRig()`: Creates the movable light, sky light, fog and unbound post-process components.
- `void ApplyParams(const FSunParams& SunParams, const FAmbientParams& Ambient)`: Applies sun and planetshine direction, colours, intensities, fog and tint, then refreshes the sky light.

#### `Environment/PlayAreaSyncComponent.h`

**`UPlayAreaSyncComponent`** (class)

Keeps a bounds box the size of the current sector's play area. Add it to any actor that owns a UBoxComponent (for example the host game's sector-bounds actor): whenever a sector becomes ready, the box's unscaled extent is set to USectorDirectorSubsystem::GetPlayAreaExtent plus ExtentPadding. The actor should sit at the level origin with scale 1, because the play area is centred on the origin. It only applies the size; what the box does with the player is left to its owner.

Variables:

- `BoxComponentName` (`FName`): Name of the UBoxComponent on the owner to resize. None uses the owner's first UBoxComponent.
- `ExtentPadding` (`FVector`): Added to the play-area extent, so the bounds warning starts just outside the generated content.
- `BoundDirector` (`TWeakObjectPtr<USectorDirectorSubsystem>`): Director this component is subscribed to, kept so EndPlay can unsubscribe.
- `bWarnedMissingBox` (`bool`): True once the missing-box warning has been logged, so it is not repeated on every sector change.

Functions:

- `UPlayAreaSyncComponent()`: Disables ticking; the component only reacts to sector changes.
- `void ApplyPlayArea()`: Sets the box's unscaled extent to the director's play-area extent plus ExtentPadding. Logs a warning once if no box is found.
- `virtual void BeginPlay() override`: Subscribes to the director's OnSectorReady and applies the play area at once if a sector is already current.
- `virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override`: Unsubscribes from the director.
- `void HandleSectorReady(FName SectorId)`: Receives the director's OnSectorReady and applies the new sector's play area.
- `UBoxComponent* FindBox() const`: Returns the box to resize: the one named BoxComponentName, or the owner's first UBoxComponent when the name is None.

#### `Environment/SectorPreviewer.h`

**`ASectorPreviewer`** (class)

Editor helper. Place one in a level that also contains the environment actors, pick a preset and seed, and press "Apply Preview" in the Details panel to see the destination without pressing Play. Has no effect at runtime.

Variables:

- `PreviewPreset` (`TObjectPtr<UEnvironmentPreset>`): Preset to preview.
- `PreviewSeed` (`int32`): Seed to preview. Change it to see the preset's variation range.
- `PreviewPlayAreaExtent` (`FVector`): Half-size of the play area the previewed asteroids are scaled to, standing in for FSectorDefinition::PlayAreaExtent (the previewer has no sector).
- `bPreviewAsteroids` (`bool`): True also fills the asteroid field, which can take a moment for large counts.

Functions:

- `ASectorPreviewer()`: Disables ticking; this actor only exists for its Details panel button.
- `void ApplyPreview()`: Builds parameters from PreviewPreset and PreviewSeed and applies them to the environment actors in this level.
- `void ClearPreviewAsteroids()`: Removes previewed asteroids so they are not saved into the level.

#### `Environment/SpaceBackdrop.h`

**`ASpaceBackdrop`** (class)

A huge inward-facing sphere showing stars and nebulae through one parameter-driven material. Hidden until BackdropMaterial is assigned (in a Blueprint subclass), because the look lives entirely in that material.

Variables:

- `SkySphere` (`TObjectPtr<UStaticMeshComponent>`): The sky sphere mesh. No collision, no shadows.
- `BackdropMaterial` (`TObjectPtr<UMaterialInterface>`): Sky material: unlit, two-sided, with "Is Sky" enabled. Empty: the backdrop stays hidden.
- `SphereScale` (`float`): Uniform scale of the sphere. Must enclose the planet; 40000 gives a 20 km radius.
- `StarTintParameter` (`FName`): Vector parameter receiving FBackdropParams::StarTint.
- `StarDensityParameter` (`FName`): Scalar parameter receiving FBackdropParams::StarDensity.
- `NebulaColorParameter` (`FName`): Vector parameter receiving FBackdropParams::NebulaColor.
- `NebulaColorBParameter` (`FName`): Vector parameter receiving FBackdropParams::NebulaColorB.
- `NebulaIntensityParameter` (`FName`): Scalar parameter receiving FBackdropParams::NebulaIntensity.
- `SunDirectionParameter` (`FName`): Vector parameter receiving the world-space direction toward the sun (FBackdropParams::SunDirection) in its RGB.
- `SunColorParameter` (`FName`): Vector parameter receiving FBackdropParams::SunColor.
- `SunCosRadiusParameter` (`FName`): Scalar parameter receiving the cosine of FBackdropParams::SunAngularRadiusDegrees, which the material compares against.
- `SunBrightnessParameter` (`FName`): Scalar parameter receiving FBackdropParams::SunBrightness.
- `BackdropMID` (`TObjectPtr<UMaterialInstanceDynamic>`): Runtime instance of BackdropMaterial. Null while no material is assigned.

Functions:

- `ASpaceBackdrop()`: Creates the sky sphere component and assigns the engine's basic sphere mesh.
- `void ApplyParams(const FBackdropParams& Params)`: Sends colours and densities to the material and rotates the sphere.

#### `Environment/ViewAnchor.h`

**`FViewAnchor`** (class)

Shared answer to "where is the player looking from?", used by everything that is anchored to the camera like a skybox (the near planet and the distant sky bodies).

Functions:

- `static FVector GetViewLocation(const UWorld* World)`: Returns the first player's camera location, or the level origin when there is no player camera (for example in the editor).

#### `Factories/DefaultCatalogFactory.h`

**`FDefaultPresetLook`** (struct)

Compact description of one built-in test preset, expanded by FDefaultCatalogFactory.

Variables:

- `Name` (`FName`): Object name of the preset.
- `SurfaceA` (`FLinearColor`): Primary planet surface colour.
- `SurfaceB` (`FLinearColor`): Secondary planet surface colour.
- `CloudCoverage` (`float`): Cloud coverage, 0 to 1.
- `PlanetRadius` (`float`): Planet radius in Unreal units.
- `Atmosphere` (`FLinearColor`): Atmosphere rim colour.
- `Sun` (`FLinearColor`): Sunlight colour.
- `Haze` (`FLinearColor`): Fog and nebula colour.
- `Tint` (`FLinearColor`): Post-process tint.
- `AsteroidTint` (`FLinearColor`): Asteroid colour.
- `AsteroidCount` (`int32`): Number of asteroids before seed variation.
- `EnemyDensity` (`float`): Multiplier on enemy counts, so some sectors are more dangerous.

**`FDefaultCatalogFactory`** (class)

Factory for a complete catalog built in code: four presets and four sectors joined in a ring. It exists so the plugin can be tested with zero assets. Real projects assign their own USectorCatalog asset in the settings and this factory is never called.

Functions:

- `static USectorCatalog* CreateTransientCatalog(UObject* Outer)`: Creates the test catalog as a transient object owned by Outer.
- `static UEnvironmentPreset* MakePreset(UObject* Outer, const FDefaultPresetLook& Look)`: Creates one transient preset from a compact look description.
- `static void AddDefaultZones(FEnvironmentParams& Params, const float EnemyDensity)`: Adds the Core, Inner, Middle and Outer zones using the placeholder actor classes.
- `static FZoneSpawnTable MakeZone(const FName ZoneName, const float InnerFraction, const float OuterFraction, const float HeightFraction)`: Builds one spawn zone. The fractions are of the sector's play area (see FZoneSpawnTable).
- `static FZoneSpawnEntry MakeEntry(UClass* ActorClass, const int32 MinCount, const int32 MaxCount, const bool bRememberWhenConsumed)`: Builds one spawn entry with no respawn and no spacing.
- `static FZoneSpawnEntry MakeEnemyEntry(UClass* ActorClass, const int32 MinCount, const int32 MaxCount)`: Builds an enemy entry that respawns like the old enemy spawn volume: after 30 s, at least 150 m from the player.
- `static FZoneSpawnEntry MakeLootEntry(UClass* ActorClass, const int32 MinCount, const int32 MaxCount)`: Builds a loot entry that respawns like the old loot scatter volume: after 60 s, 30 to 150 m away, 80 m apart.

#### `Factories/EnvironmentActorFactory.h`

**`FEnvironmentActorFactory`** (class)

Factory for the one-per-level environment actors (planet, lighting rig, backdrop, asteroid field). It reuses an actor already placed in the level and only spawns one when none exists, which is why the plugin works both in a hand-built level and in a completely empty one.

Functions:

- `static TActor* FindOrSpawn(UWorld& World, const TSubclassOf<TActor> ActorClass)`: Returns the first actor of the type found in the world, or spawns one of ActorClass at the origin.
- `static TSubclassOf<TObject> ResolveClass(const TSoftClassPtr<TObject>& SoftClass)`: Loads a class chosen in the settings, falling back to the native class when none is set.

#### `Factories/EnvironmentParamsFactory.h`

**`FEnvironmentParamsFactory`** (class)

Factory that turns a preset plus a seed into the final environment parameters. It is the only place where sector randomness for the look of a destination is decided, and it is deterministic: the same preset and seed always give the same result.

Functions:

- `static FEnvironmentParams Build(const UEnvironmentPreset& Preset, const int32 Seed)`: Builds the parameters for one sector from its preset and seed.
- `static void ResolvePlanetshine(FEnvironmentParams& Params)`: Works out the light reflected off the near planet from the final sun and planet values: its direction (from the planet toward the player), colour (the planet's surface) and intensity (strength times how much of the planet's day side faces the player). Call it last, after anything that can still move the sun or the planet, such as FStarSystemSkyFactory.
- `static FLinearColor JitterHue(const FLinearColor& Color, const float MaxDegrees, FRandomStream& Stream)`: Shifts a colour's hue by up to MaxDegrees in either direction, keeping saturation, value and alpha.
- `static float JitterScalar(const float Value, const float Fraction, FRandomStream& Stream)`: Scales a value by a random factor between (1 - Fraction) and (1 + Fraction).

#### `Factories/SectorGenStepFactory.h`

**`FSectorGenStepFactory`** (class)

Factory for the generation pipeline. The director asks it for step objects and never names a concrete step class, so steps can be added, removed or reordered from the project settings without touching the director.

Functions:

- `static TArray<USectorGenStep*> CreatePipeline(UObject* Outer, const TArray<TSoftClassPtr<USectorGenStep>>& StepClasses)`: Creates one step object per class, in order, owned by Outer. An empty list produces the built-in pipeline from GetDefaultStepClasses.
- `static void GetDefaultStepClasses(TArray<TSubclassOf<USectorGenStep>>& OutClasses)`: Fills OutClasses with the built-in steps in their intended order.

#### `Factories/StarSystemSkyFactory.h`

**`FStarSystemSkyFactory`** (class)

Factory that works out what the sky looks like from one planet of a star system: where the sun is and how big it looks, which other planets are visible and where, and where the home planet's moons sit. Directions are true to the system's layout; sizes are deliberately exaggerated so neighbours read as planets rather than dots.

Functions:

- `static bool Apply(const FStarSystemDefinition& System, const FName HomeBodyId, FEnvironmentParams& Params)`: Rewrites the sun, the sun disc and the sky bodies of Params for a viewer next to HomeBodyId. Returns false and changes nothing when the system has no body with that id.
- `static FVector GetBodyPosition(const FCelestialBodyDefinition& Body)`: Position of a body in orbit units, with the sun at the origin and orbits in the horizontal plane.
- `static void AddNeighbours(const FStarSystemDefinition& System, const FCelestialBodyDefinition& Home, const FVector& HomePosition, TArray<FSkyBodyParams>& OutBodies)`: Adds one sky body per other planet of the system, as seen from HomePosition.
- `static void AddMoons(const FCelestialBodyDefinition& Home, const FPlanetParams& Planet, TArray<FSkyBodyParams>& OutBodies)`: Adds the home planet's moons, placed relative to where the near planet appears.

#### `Gates/JumpGate.h`

**`AJumpGate`** (class)

A trigger that starts a warp to another sector when a pawn carrying a UWarpTransitionComponent flies into it. Gates are pooled and reconfigured for each sector; the gate itself knows nothing about generation, it only asks the pawn's component to warp.

Variables:

- `Trigger` (`TObjectPtr<USphereComponent>`): Overlap volume the ship must enter. Root component.
- `GateMesh` (`TObjectPtr<UStaticMeshComponent>`): Placeholder visual (a flattened cylinder). Replace the mesh in a Blueprint subclass.
- `Label` (`TObjectPtr<UTextRenderComponent>`): Floating text showing the destination's display name.
- `DestinationSectorId` (`FName`): Sector this gate leads to. Set by Configure; editable for gates placed by hand.

Functions:

- `AJumpGate()`: Creates the trigger, placeholder ring mesh and destination label.
- `void Configure(const FGateLink& Link, const FVector& WorldLocation, const FText& DestinationLabel)`: Moves the gate to WorldLocation, turns it toward the level centre and sets its destination and label from the link. WorldLocation is the link's NormalizedLocation resolved against the sector's play area (FPlayAreaMath::ToWorld).
- `FName GetDestinationSectorId() const`: Sector this gate currently leads to.
- `virtual void BeginPlay() override`: Binds the trigger's overlap event.
- `void HandleTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)`: Starts a warp on the overlapping pawn's UWarpTransitionComponent, if it has one, through BeginWarpFromTrigger: a warp the game refuses is retried while the pawn stays inside the trigger.

#### `Gates/WarpTransitionComponent.h`

- `DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWarpStarted, FName, DestinationSectorId)`: Broadcast when a warp starts. Start warp-tunnel effects and sounds here.
- `DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWarpFinished, FName, SectorId)`: Broadcast when the player regains control in the new sector.
- `DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWarpRefused, FText, Reason)`: Broadcast when the game refuses a warp through CanBeginWarp. Reason is the text the game gave, for the HUD.
- `DECLARE_DELEGATE_RetVal_TwoParams(bool, FCanBeginWarp, FName, FText&)`: Asked by BeginWarp before a warp starts, when bound. Return false to refuse it and fill OutReason with a message for the player.

**`EWarpState`** (enum class)

Stage of a warp in progress (State pattern, kept as a simple enum-driven machine).

Values:

- `Idle`: Not warping; the player has control.
- `FadingOut`: The screen is fading to the warp colour. Nothing has been regenerated yet.
- `Generating`: The screen is fully covered and the director is rebuilding the level.

**`UWarpTransitionComponent`** (class)

Add to the player's ship. It is the only class that knows the warp sequence: lock input, fade out, ask the director for the new sector, wait until it is ready, move the ship to the arrival gate, fade in, unlock input. The regeneration is hidden because it only starts once the screen is covered.

Variables:

- `OnWarpStarted` (`FOnWarpStarted`): Fires when a warp starts.
- `OnWarpFinished` (`FOnWarpFinished`): Fires when the warp has finished and input is restored.
- `CanBeginWarp` (`FCanBeginWarp`): Optional game rule consulted by BeginWarp before a warp starts (native only). Unbound means every warp is allowed. Bind it on the owning pawn, for example to refuse warps in combat.
- `OnWarpRefused` (`FOnWarpRefused`): Fires when CanBeginWarp refuses a warp, with its reason. Not fired again by the retries.
- `FadeOutSeconds` (`float`): Seconds the screen takes to fade out before regeneration starts.
- `FadeInSeconds` (`float`): Seconds the screen takes to fade back in after arrival.
- `MinimumHoldSeconds` (`float`): Shortest time the screen stays covered, so a fast regeneration does not look like a flicker.
- `FadeColor` (`FLinearColor`): Colour the screen fades to.
- `RefusedRetrySeconds` (`float`): Seconds between retries of a warp refused by CanBeginWarp while the owner stays inside the gate's trigger.
- `State` (`EWarpState`): Current stage of the warp.
- `PendingSectorId` (`FName`): Sector the warp in progress is travelling to.
- `HoldStartSeconds` (`double`): World time at which the screen became fully covered.
- `TimerHandle` (`FTimerHandle`): Timer used for the fade-out wait and the minimum hold.
- `RetryTimerHandle` (`FTimerHandle`): Timer for retrying a refused warp.
- `RetryTrigger` (`TWeakObjectPtr<AActor>`): Trigger the owner must stay inside for a refused warp to be retried.
- `RetrySectorId` (`FName`): Sector the refused warp is retried to.

Functions:

- `UWarpTransitionComponent()`: Disables this component's tick; the sequence is driven by timers and the director's event.
- `bool BeginWarp(FName DestinationSectorId)`: Starts a warp to the sector. Returns false if already warping, the director is busy, the sector is unknown or CanBeginWarp refuses it; a CanBeginWarp refusal also broadcasts OnWarpRefused.
- `bool BeginWarpFromTrigger(FName DestinationSectorId, AActor* Trigger)`: Like BeginWarp, for a warp started by entering Trigger (a jump gate). If CanBeginWarp refuses it, the warp is retried every RefusedRetrySeconds while the owner still overlaps Trigger, and starts by itself once allowed. Retries do not broadcast OnWarpRefused again. Retrying stops when the owner leaves Trigger or a warp starts.
- `void CancelWarpRetry()`: Stops retrying a refused warp, if a retry is pending.
- `bool IsRetryingWarp() const`: True while a warp refused by CanBeginWarp is being retried because the owner is still inside the trigger.
- `bool IsWarping() const`: True from BeginWarp until control is returned.
- `EWarpState GetWarpState() const`: Current stage of the warp.
- `virtual void BeginPlay() override`: Subscribes to the director's OnSectorReady event.
- `virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override`: Unsubscribes and clears timers.
- `bool TryBeginWarp(FName DestinationSectorId, bool bBroadcastRefusal, bool& bOutRefusedByGame)`: BeginWarp's body. bOutRefusedByGame is set when CanBeginWarp refused; OnWarpRefused is broadcast for that only when bBroadcastRefusal is true.
- `void HandleRetryTimer()`: Retry timer callback: stops once the owner has left the trigger, otherwise tries the warp again quietly.
- `void HandleFadeOutFinished()`: Timer callback: the screen is covered, so ask the director to regenerate.
- `void HandleSectorReady(FName SectorId)`: Director callback: the new sector is ready. Waits out the minimum hold, then completes the arrival.
- `void CompleteArrival()`: Moves the ship to the arrival gate, fades in, restores input and broadcasts OnWarpFinished.
- `void StartFade(const float FromAlpha, const float ToAlpha, const float Duration) const`: Fades the owning player's camera between two alpha values.
- `void SetInputLocked(const bool bLocked) const`: Enables or disables player input on the owning pawn.
- `APawn* GetOwnerPawn() const`: Returns the owner as a pawn, or nullptr.
- `APlayerController* GetOwnerPlayerController() const`: Returns the player controller possessing the owner, or nullptr.

#### `Population/ActorPoolSubsystem.h`

**`FActorPoolBucket`** (struct)

All stored and in-use actors of one class.

Variables:

- `ActorClass` (`TObjectPtr<UClass>`): The exact class this bucket holds.
- `Free` (`TArray<TObjectPtr<AActor>>`): Deactivated actors waiting to be reused.
- `ActiveCount` (`int32`): Number of actors of this class currently handed out.

**`UActorPoolSubsystem`** (class)

Object pool for actors (Object Pool pattern, and the factory for every pooled actor). Acquire hands out a stored actor or spawns a new one; Release hides and stores it. Nothing is destroyed between sectors, which avoids spawn cost and garbage-collection spikes.

Variables:

- `Buckets` (`TArray<FActorPoolBucket>`): One bucket per class. Searched linearly; the number of pooled classes is small.

Functions:

- `AActor* Acquire(TSubclassOf<AActor> ActorClass, const FTransform& Transform)`: Returns an active actor of the class at the transform, reusing a stored one when possible. Null if spawning fails.
- `void Release(AActor* Actor)`: Deactivates the actor and stores it for reuse. Safe to call twice and on actors that were not pooled.
- `void Prewarm(TSubclassOf<AActor> ActorClass, int32 Count)`: Spawns Count actors of the class straight into storage, to move spawn cost to a loading moment.
- `int32 GetActiveCount() const`: Total actors currently handed out, across all classes.
- `int32 GetFreeCount() const`: Total actors currently stored, across all classes.
- `FActorPoolBucket& FindOrAddBucket(UClass* ActorClass)`: Returns the bucket for the class, creating it when missing.
- `AActor* SpawnNew(UClass* ActorClass, const FTransform& Transform) const`: Spawns a brand-new actor of the class.
- `static void Activate(AActor& Actor, const FTransform& Transform)`: Places the actor and turns rendering, collision and ticking back on, then notifies IPoolable. The actor's tick and each component's tick are restored to their class's bStartWithTickEnabled.
- `static void Deactivate(AActor& Actor)`: Notifies IPoolable, then turns rendering, collision and ticking off. Every component's tick is disabled and every timer bound to the actor or one of its components is cleared, so nothing keeps running while it is stored.
- `static void NotifyPoolables(AActor& Actor, const bool bAcquired)`: Calls the IPoolable event on the actor and on each of its components that implement it.

#### `Population/ComponentTickUtils.h`

**`FComponentTickUtils`** (struct)

Switches an actor's component ticks and timers together. Turning off only the actor's tick leaves its components (AI, weapons, regeneration) running, so the pool and the relevance component use these helpers to stop and restart a whole actor. Header-only; shared by UActorPoolSubsystem and URelevanceComponent.

Functions:

- `static void SetComponentTicks(AActor& Actor, const bool bEnable)`: Enables or disables the tick of every component of the actor that can tick. Enabling restores each component to its own PrimaryComponentTick.bStartWithTickEnabled, so components that start switched off stay off.
- `static void ClearAllTimers(AActor& Actor)`: Clears every timer bound to the actor and to each of its components.

#### `Population/Poolable.h`

**`UPoolable`** (class)

Reflection stub for IPoolable. Not used directly.

**`IPoolable`** (class)

Implement on an actor or component that needs to reset itself when reused by the pool. A pooled actor is never constructed again, so BeginPlay runs only once: anything that must restart per use (health, AI state, timers, velocity) belongs in OnAcquiredFromPool.

Functions:

- `void OnAcquiredFromPool()`: Called after the actor was activated and placed. Reset per-use state here.
- `void OnReleasedToPool()`: Called just before the actor is hidden and stored. Stop timers, effects and AI here.

#### `Population/RelevanceComponent.h`

- `DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRelevanceChanged, bool, bIsAwake)`: Broadcast when the owner wakes up (true) or goes to sleep (false).

**`URelevanceComponent`** (class)

Add to any actor that should stop costing CPU while the player is far away. URelevanceSubsystem decides when to check; this component decides, from its own distances, whether its owner sleeps, and applies the result to the owner. Bind OnRelevanceChanged to pause anything else, such as AI or looping effects.

Variables:

- `OnRelevanceChanged` (`FOnRelevanceChanged`): Fires when the owner wakes or sleeps.
- `SleepDistance` (`float`): Distance beyond which the owner goes to sleep.
- `WakeDistance` (`float`): Distance within which the owner wakes. Keep it below SleepDistance so the state does not flicker at the edge.
- `bDisableCollisionWhenAsleep` (`bool`): True turns the owner's collision off while asleep. Leave false for anything that must stay targetable from afar.
- `bHideWhenAsleep` (`bool`): True hides the owner while asleep. Leave false when it should stay visible in the distance.
- `bAwake` (`bool`): Current state. Owners start awake.
- `bEvaluationEnabled` (`bool`): False while the owner is stored in the pool.
- `bOwnerTicksWhenAwake` (`bool`): Whether the owner's tick was enabled at BeginPlay, so waking only re-enables ticks that existed.

Functions:

- `URelevanceComponent()`: Disables this component's own tick; the subsystem drives it instead.
- `void Evaluate(const FVector& ViewerLocation)`: Compares the owner's distance to the viewer against the thresholds and sleeps or wakes the owner.
- `void SetAwake(const bool bNewAwake)`: Forces the awake state and applies it to the owner. Broadcasts OnRelevanceChanged when it changes. Asleep, the owner's tick and every component's tick are off; awake, each is restored to its bStartWithTickEnabled.
- `bool IsAwake() const`: True while the owner is awake.
- `bool IsEvaluationEnabled() const`: False while the owner is stored in the pool, during which the subsystem skips it.
- `virtual void OnAcquiredFromPool_Implementation() override`: Pool hook: resumes evaluation and treats the freshly activated owner as awake.
- `virtual void OnReleasedToPool_Implementation() override`: Pool hook: suspends evaluation so a stored actor is never woken.
- `virtual void BeginPlay() override`: Registers with the relevance subsystem and remembers whether the owner ticks.
- `virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override`: Unregisters from the relevance subsystem.

#### `Population/RelevanceSubsystem.h`

**`URelevanceSubsystem`** (class)

Checks a fixed number of relevance components per frame, in rotation, against the player's position. The per-frame cost is constant regardless of actor count; more actors only means each one is checked less often.

Variables:

- `Components` (`TArray<TWeakObjectPtr<URelevanceComponent>>`): Every registered component. Weak so destroyed owners drop out on their own.
- `Cursor` (`int32`): Index of the next component to evaluate.

Functions:

- `void Register(URelevanceComponent& Component)`: Adds a component to the rotation. Called by URelevanceComponent::BeginPlay.
- `void Unregister(URelevanceComponent& Component)`: Removes a component from the rotation. Called by URelevanceComponent::EndPlay.
- `virtual void Tick(float DeltaTime) override`: Evaluates the next batch of components against the player pawn's location.
- `virtual TStatId GetStatId() const override`: Stat id required by the tickable interface.
- `int32 GetTrackedCount() const`: Number of registered components that are valid and not stored in the pool.
- `int32 GetAwakeCount() const`: Number of tracked components whose owner is currently awake.
- `virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override`: Limits the subsystem to game and Play-In-Editor worlds.
- `bool GetViewerLocation(FVector& OutLocation) const`: Finds the location relevance is measured from: the first player's pawn, or its camera when there is no pawn.

#### `Population/SectorPlaceholderActors.h`

**`ASectorPlaceholderActor`** (class)

Coloured basic-shape stand-in used by the built-in test catalog, so the plugin can be tested before any real enemy, loot or station class exists. Not meant for the real game.

Variables:

- `Mesh` (`TObjectPtr<UStaticMeshComponent>`): The visible shape. Blocks everything so it can be hit by traces.
- `Relevance` (`TObjectPtr<URelevanceComponent>`): Puts the placeholder to sleep when the player is far away, to exercise the relevance system.
- `PlaceholderColor` (`FLinearColor`): Colour written to the "Color" parameter of BasicShapeMaterial.
- `ColorMID` (`TObjectPtr<UMaterialInstanceDynamic>`): Runtime material instance the colour is written to.

Functions:

- `ASectorPlaceholderActor()`: Creates the mesh and relevance components. Subclasses pick the shape, size and colour.
- `virtual void BeginPlay() override`: Applies PlaceholderColor to the mesh material.
- `void SetShape(const TCHAR* MeshPath, const FVector& Scale)`: Assigns one of the engine's basic shapes and a scale. Only valid inside a subclass constructor.

**`ASectorPlaceholderEnemy`** (class)

Red cone standing in for an enemy ship. Respawns on every visit.

Functions:

- `ASectorPlaceholderEnemy()`: Sets a red cone.

**`ASectorPlaceholderLoot`** (class)

Yellow cube standing in for world loot. Stays gone once consumed.

Functions:

- `ASectorPlaceholderLoot()`: Sets a small yellow cube.

**`ASectorPlaceholderStation`** (class)

Large grey cylinder standing in for the central station. Stays gone once consumed.

Functions:

- `ASectorPlaceholderStation()`: Sets a large grey cylinder.

#### `Population/ZoneSpawnPlanner.h`

**`FZoneSpawnRules`** (struct)

The respawn and spacing settings of one spawn entry, copied so a spawn knows its rules after the plan is gone.

Variables:

- `RespawnDelay` (`float`): Seconds after consumption before the spawn respawns; 0 means no respawn during the visit.
- `RespawnOffsetMin` (`float`): Smallest distance from the consumed spot to the respawn point (cm).
- `RespawnOffsetMax` (`float`): Largest distance from the consumed spot to the respawn point (cm).
- `MinDistanceFromPlayer` (`float`): A respawn never appears closer than this to the player (cm).
- `MinSpacing` (`float`): Minimum distance to any other spawn of the sector (cm); 0 disables it.

Functions:

- `static FZoneSpawnRules FromEntry(const FZoneSpawnEntry& Entry)`: Copies the rules of a spawn entry. Negative values become 0 and RespawnOffsetMax is raised to RespawnOffsetMin.
- `bool Respawns() const`: True when consumed spawns come back during the visit.

**`FPlannedSpawn`** (struct)

One actor the population step should place.

Variables:

- `SpawnId` (`int32`): Position of this spawn in the plan. Stable for a given seed, so it identifies the spawn across visits.
- `ActorClass` (`TSubclassOf<AActor>`): Class to take from the pool.
- `Transform` (`FTransform`): Where to place the actor.
- `bRememberWhenConsumed` (`bool`): From the spawn entry: whether consuming it must be remembered. Always false for an entry that respawns.
- `ZoneIndex` (`int32`): Index of the zone (in FEnvironmentParams::Zones) the spawn belongs to, so a respawn stays in that zone.
- `Rules` (`FZoneSpawnRules`): Respawn and spacing settings copied from the spawn entry.

**`FZoneSpawnPlanner`** (class)

Decides what spawns where. It only produces lists and points; it never touches the world. Keeping the decision separate from the spawning makes the layout testable and repeatable.

Variables:

- `MaxSpacingRedraws` (`static constexpr int32`): Most redraws of one planned point that is too close to another spawn; the last draw is then accepted.
- `MaxRespawnTries` (`static constexpr int32`): Most candidate points tried per respawn attempt.
- `SpacingSeedSalt` (`static constexpr uint32`): Constant mixed into the plan seed for the spacing redraw stream, kept apart from the layout stream.

Functions:

- `static void BuildPlan(const TArray<FZoneSpawnTable>& Zones, const FVector& PlayAreaExtent, const int32 Seed, TArray<FPlannedSpawn>& OutPlan)`: Appends one planned spawn per actor to OutPlan, for every entry of every zone, using the seed. PlayAreaExtent is the half-size of the sector's playable box; zone fractions are multiplied by it per axis. A point of an entry with MinSpacing above 0 that lands closer than that to a point already in the plan is redrawn, at most MaxSpacingRedraws times, from a separate stream; the count and yaw draws are untouched, so entries without spacing keep exactly the layout they had before spacing existed.
- `static bool ChooseRespawnPoint(FRandomStream& Stream, const FZoneSpawnTable& Zone, const FVector& PlayAreaExtent, const FZoneSpawnRules& Rules, const FVector& ConsumedLocation, const FVector& PlayerLocation, const TArray<FVector>& OccupiedLocations, FVector& OutLocation)`: Picks where a consumed spawn comes back, trying at most MaxRespawnTries random points from the stream. A point is RespawnOffsetMin to RespawnOffsetMax from ConsumedLocation, with its height drawn inside the zone's height band. It is accepted only if it lies inside the zone's ring and height and inside the play-area box, at least MinDistanceFromPlayer from PlayerLocation, and at least MinSpacing from every OccupiedLocations entry. Returns false, leaving OutLocation unchanged, when no try is accepted.
- `static bool IsFarEnough(const FVector& Location, const TArray<FVector>& Others, const float MinSpacing)`: True when Location is at least MinSpacing from every point in Others.

#### `Steps/SectorGenStep_AsteroidField.h`

**`USectorGenStep_AsteroidField`** (class)

Generation step that refills the asteroid field over several frames and forwards "asteroid removed" events to the director so they are remembered.

Variables:

- `SeedSalt` (`static constexpr uint32`): Constant mixed into the sector seed for this step's random sequence.
- `Field` (`TWeakObjectPtr<AAsteroidField>`): The asteroid field this step drives.
- `RemovedHandle` (`FDelegateHandle`): Handle of the subscription to the field's removal event, kept so it is only bound once.

Functions:

- `virtual FName GetStepName() const override`: Returns "Asteroids".
- `virtual void Begin(const FSectorGenContext& Context) override`: Finds or spawns the field, subscribes to its removal event and starts a seeded fill sized by the sector's PlayAreaExtent.
- `virtual bool TickSlice(const FSectorGenContext& Context, const double DeadlineSeconds) override`: Adds asteroid batches until the field is full or the deadline passes.
- `virtual void Clear(UWorld& World) override`: Empties the field.
- `void HandleAsteroidRemoved(const int32 AsteroidId)`: Receives the field's removal event and passes the id to the director for storage.

#### `Steps/SectorGenStep_Backdrop.h`

**`USectorGenStep_Backdrop`** (class)

Generation step that restyles the star and nebula sky sphere.

Variables:

- `Backdrop` (`TWeakObjectPtr<ASpaceBackdrop>`): The backdrop actor this step drives.

Functions:

- `virtual FName GetStepName() const override`: Returns "Backdrop".
- `virtual void Begin(const FSectorGenContext& Context) override`: Finds the level's backdrop actor, or spawns one of the class chosen in the settings.
- `virtual bool TickSlice(const FSectorGenContext& Context, const double DeadlineSeconds) override`: Applies the backdrop parameters. Finishes in one call.

#### `Steps/SectorGenStep_CelestialSky.h`

**`USectorGenStep_CelestialSky`** (class)

Generation step that shows the distant planets and moons visible from the sector.

Variables:

- `Sky` (`TWeakObjectPtr<ACelestialSkyActor>`): The celestial sky actor this step drives.

Functions:

- `virtual FName GetStepName() const override`: Returns "CelestialSky".
- `virtual void Begin(const FSectorGenContext& Context) override`: Finds the level's celestial sky actor, or spawns one of the class chosen in the settings.
- `virtual bool TickSlice(const FSectorGenContext& Context, const double DeadlineSeconds) override`: Applies the sector's sky bodies. Finishes in one call.

#### `Steps/SectorGenStep_Gates.h`

**`USectorGenStep_Gates`** (class)

Generation step that places one pooled jump gate for every gate link of the sector.

Variables:

- `ActiveGates` (`TArray<TWeakObjectPtr<AJumpGate>>`): Gates currently placed in the level by this step.

Functions:

- `virtual FName GetStepName() const override`: Returns "Gates".
- `virtual bool TickSlice(const FSectorGenContext& Context, const double DeadlineSeconds) override`: Takes gates from the pool and places them at each gate link's NormalizedLocation scaled by the sector's PlayAreaExtent. Finishes in one call.
- `virtual void Clear(UWorld& World) override`: Returns every gate of the previous sector to the pool.

#### `Steps/SectorGenStep_Lighting.h`

**`USectorGenStep_Lighting`** (class)

Generation step that applies sun, sky light, fog and scene tint.

Variables:

- `Rig` (`TWeakObjectPtr<ASectorLightingRig>`): The lighting rig this step drives.

Functions:

- `virtual FName GetStepName() const override`: Returns "Lighting".
- `virtual void Begin(const FSectorGenContext& Context) override`: Finds the level's lighting rig, or spawns one of the class chosen in the settings.
- `virtual bool TickSlice(const FSectorGenContext& Context, const double DeadlineSeconds) override`: Applies the sun and ambient parameters. Finishes in one call.

#### `Steps/SectorGenStep_Planet.h`

**`USectorGenStep_Planet`** (class)

Generation step that restyles the background planet and its atmosphere.

Variables:

- `Planet` (`TWeakObjectPtr<APlanetActor>`): The planet actor this step drives. Weak so a deleted actor is simply found or spawned again.

Functions:

- `virtual FName GetStepName() const override`: Returns "Planet".
- `virtual void Begin(const FSectorGenContext& Context) override`: Finds the level's planet actor, or spawns one of the class chosen in the settings.
- `virtual bool TickSlice(const FSectorGenContext& Context, const double DeadlineSeconds) override`: Applies the planet and atmosphere parameters. Finishes in one call.

#### `Steps/SectorGenStep_Population.h`

**`FSpawnedSectorActor`** (struct)

An actor this step placed in the level, with the bookkeeping needed to consume, respawn or release it.

Variables:

- `Actor` (`TWeakObjectPtr<AActor>`): The pooled actor.
- `SpawnId` (`int32`): Stable id of the spawn within the sector, from the spawn plan. A respawn keeps the id of the spawn it replaces.
- `bRememberWhenConsumed` (`bool`): From the spawn entry: whether consuming it must be remembered. Always false for an entry that respawns.
- `ZoneIndex` (`int32`): Zone the spawn belongs to, so its respawn stays in that zone.
- `Rules` (`FZoneSpawnRules`): Respawn and spacing settings copied from the spawn entry.

**`FPendingRespawn`** (struct)

A consumed spawn waiting to come back during the current visit.

Variables:

- `SpawnId` (`int32`): Stable id of the consumed spawn; the respawn is tracked under the same id.
- `ActorClass` (`TSubclassOf<AActor>`): Class to take from the pool.
- `ZoneIndex` (`int32`): Zone the respawn must appear in.
- `Rules` (`FZoneSpawnRules`): Respawn and spacing settings copied from the spawn entry.
- `ConsumedLocation` (`FVector`): Where the spawn was when it was consumed; the respawn appears RespawnOffsetMin to RespawnOffsetMax from it.
- `DueTime` (`double`): World time (UWorld::GetTimeSeconds) from which the respawn is attempted.
- `AttemptCounter` (`int32`): Failed attempts so far; part of the respawn's seed, so each retry tries different points.

**`USectorGenStep_Population`** (class)

Generation step that fills the spawn zones with pooled actors (enemies, loot, stations, NPCs). It executes a plan made by FZoneSpawnPlanner a few actors per frame and skips anything the sector state says was already consumed. While the sector is playable it brings consumed spawns back after their entry's RespawnDelay, near where they were consumed and away from the player.

Variables:

- `SeedSalt` (`static constexpr uint32`): Constant mixed into the sector seed for this step's random sequence.
- `RespawnSeedSalt` (`static constexpr uint32`): Constant mixed into the sector seed for respawn points, which depend on the player and are not part of the layout.
- `RetryDelaySeconds` (`static constexpr double`): Seconds a respawn waits before trying again when none of its candidate points was accepted.
- `Plan` (`TArray<FPlannedSpawn>`): The spawn plan being executed.
- `PlanCursor` (`int32`): Index of the next plan entry to spawn.
- `Spawned` (`TArray<FSpawnedSectorActor>`): Actors currently placed in the level by this step.
- `PendingRespawns` (`TArray<FPendingRespawn>`): Consumed spawns waiting to come back during this visit.
- `Zones` (`TArray<FZoneSpawnTable>`): Zones of the current sector, so a respawn knows its ring and height.
- `PlayAreaExtent` (`FVector`): Half-size of the current sector's play area.
- `SectorSeed` (`int32`): Seed of the current sector, for the respawn streams.
- `LoadedClasses` (`TArray<TObjectPtr<UClass>>`): Keeps the classes of the sector's spawns referenced, for generation and for respawns, so garbage collection cannot unload them.

Functions:

- `virtual FName GetStepName() const override`: Returns "Population".
- `virtual void Begin(const FSectorGenContext& Context) override`: Builds the seeded spawn plan for the sector, with zones scaled by its PlayAreaExtent, and keeps what respawns need.
- `virtual bool TickSlice(const FSectorGenContext& Context, const double DeadlineSeconds) override`: Spawns planned actors from the pool until the plan is done or the deadline passes.
- `virtual void Clear(UWorld& World) override`: Returns every actor this step placed to the pool and drops all pending respawns.
- `virtual bool TryConsumeActor(AActor& Actor, int32& OutSpawnId, bool& bOutRemember) override`: If Actor was placed by this step: releases it to the pool and reports its spawn id. If its entry has a RespawnDelay, queues a respawn after releasing it.
- `virtual void TickLive(UWorld& World, float DeltaSeconds) override`: Brings back every due respawn that finds an accepted point; pushes the others back by RetryDelaySeconds.
- `virtual int32 CountLiveActors(const UClass* ActorClass) const override`: Number of tracked live actors whose class is ActorClass or a subclass of it.
- `virtual int32 CountPendingRespawns() const override`: Number of queued respawns.
- `bool SpawnTracked(UActorPoolSubsystem& Pool, const TSubclassOf<AActor> ActorClass, const FTransform& Transform, const int32 SpawnId, const bool bRememberWhenConsumed, const int32 ZoneIndex, const FZoneSpawnRules& Rules)`: Takes an actor from the pool and tracks it under the spawn id. Returns false if the pool gave nothing.
- `bool TryRespawn(UActorPoolSubsystem& Pool, const FPendingRespawn& Pending, const FVector& PlayerLocation)`: Tries to place one due respawn. Returns true when it was spawned and can leave the queue.
- `static FVector GetPlayerLocation(const UWorld& World)`: Where respawns keep their distance from: the first player's pawn, or the view location without a pawn.

### Sandbox host project (test only)

#### `SandboxGameMode.h`

**`ASandboxGameMode`** (class)

Test-only game mode. Spawns the sandbox ship, moves it off the level centre when the level has no Player Start, and switches the ProcSector debug overlay on. Not part of the plugin; do not migrate it.

Variables:

- `FallbackStartLocation` (`FVector`): Where the ship starts in a level without a Player Start: just outside the station, facing the +X gate.

Functions:

- `ASandboxGameMode()`: Sets ASandboxShipPawn as the default pawn.
- `virtual void StartPlay() override`: Turns on the "ProcSector.Debug" overlay after normal start-up.
- `virtual APawn* SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform) override`: Spawns the pawn at FallbackStartLocation when the chosen start is the level origin, where the station sits.

#### `SandboxShipPawn.h`

**`ASandboxShipPawn`** (class)

Minimal test ship. Reads keys directly so the sandbox needs no input assets: W/S thrust, A/D turn, Space or E up, Left Ctrl or Q down, Left Shift boost, F to mine the asteroid or consume the spawned actor in front of the ship. Not part of the plugin; your real ship only needs a UWarpTransitionComponent.

Variables:

- `Collision` (`TObjectPtr<USphereComponent>`): Collision sphere and root. Generates the overlap that jump gates react to.
- `ShipMesh` (`TObjectPtr<UStaticMeshComponent>`): Visible ship: a cone pointing forward.
- `CameraArm` (`TObjectPtr<USpringArmComponent>`): Holds the camera behind and above the ship.
- `Camera` (`TObjectPtr<UCameraComponent>`): The player's view.
- `Movement` (`TObjectPtr<UFloatingPawnMovement>`): Simple acceleration-based movement with no gravity.
- `Warp` (`TObjectPtr<UWarpTransitionComponent>`): Runs the warp sequence when the ship enters a jump gate.
- `CruiseSpeed` (`float`): Top speed without boost, in Unreal units per second.
- `BoostSpeed` (`float`): Top speed while Left Shift is held.
- `TurnRateDegrees` (`float`): Turn rate in degrees per second.
- `InteractRange` (`float`): How far ahead the F key reaches.
- `InteractRadius` (`float`): Radius of the sweep used by the F key, so aiming does not need to be exact.

Functions:

- `ASandboxShipPawn()`: Creates collision, mesh, camera, movement and the warp transition component.
- `virtual void Tick(float DeltaSeconds) override`: Reads the keys and moves the ship. Ignores input while a warp is in progress.
- `static float ReadAxis(const APlayerController& PlayerController, const FKey& Positive, const FKey& Negative)`: Returns +1, -1 or 0 from a pair of opposing keys.
- `void Interact()`: Sweeps ahead and mines the asteroid or consumes the spawned actor that was hit.

