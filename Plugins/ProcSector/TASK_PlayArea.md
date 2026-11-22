# Task: a single play-area size per sector

Read `Plugins/ProcSector/CLAUDE.md` first. All its rules apply.

## Problem

The size of the playable space is currently written in two places that must be kept in agreement by hand:

- **Startech's `BP_SectorBounds`:** a box with extent (200000, 200000, 50000), which warns and then destroys the player outside it.
- **The plugin:** absolute centimetre values for zone radii, asteroid radii and gate locations, in presets and in `FDefaultCatalogFactory`.

When one changes, the other silently disagrees: gates outside the bounds, asteroids above the ceiling.

## Goal

Each sector stores its play-area size once. Everything the plugin places inside a sector scales from that size, and the bounds actor reads it. Changing a sector's size then needs one edit, and different sectors can have different sizes.

## Design

### 1. Data (`Data/SectorTypes.h`)

- **`FSectorDefinition`:** add `FVector PlayAreaExtent = FVector(200000.0, 200000.0, 50000.0);`. Half-size of the playable box, centred on the level origin, in centimetres.
- **`FZoneSpawnTable`:** replace `InnerRadius`, `OuterRadius` and `HalfHeight` with `InnerFraction`, `OuterFraction` and `HeightFraction` (floats, clamped 0 to 1).
  - Zones are generated as rings in unit space, then multiplied per axis by `PlayAreaExtent`. A non-square play area therefore gives elliptical rings that still fit the box.
  - `HeightFraction` 1 reaches the box's floor and ceiling.
- **`FAsteroidFieldParams`:** same change. Replace `InnerRadius`, `OuterRadius` and `HalfHeight` with `InnerFraction`, `OuterFraction` and `HeightFraction`.
- **`EAsteroidDistribution`:** add a third value, `Box`. It fills the whole box evenly, keeping out of a central ellipse of `InnerFraction` (so the station area stays clear).
  - Points are drawn uniformly in [-1, 1] on each axis, then scaled by `HeightFraction` on Z, `OuterFraction` on X and Y, and the extent.
  - Any point whose horizontal unit-space radius is below `InnerFraction` is redrawn, at most 16 times; after that the last draw is accepted.
  - The redraw loop must be deterministic: same stream, same order.
- **`FGateLink`:** replace `Location` with `NormalizedLocation` (each axis -1 to 1, multiplied by the extent).

Renaming these `UPROPERTY`s loses values in existing assets. That is acceptable: the project's preset and catalog assets still hold only default values. No CoreRedirects are needed.

Every new and changed member gets a doc comment, per the conventions.

### 2. Shared conversion helper

Add a small header-only helper, `Data/PlayAreaMath.h` (`FPlayAreaMath`), so the conversion lives in one place:

- `static FVector ToWorld(const FVector& Normalized, const FVector& Extent)`: per-axis multiply.
- `static FVector RandomPointInRing(FRandomStream& Stream, float InnerFraction, float OuterFraction, float HeightFraction, const FVector& Extent)`
  - Moves `FZoneSpawnPlanner::RandomPointInRing` here.
  - Keep its existing draw order (radius blend, angle, height), then scale by the extent.
  - Area-even distribution, as now.

The asteroid field and the spawn planner both use this helper. Neither keeps its own ring maths.

### 3. Consumers

- **`FZoneSpawnPlanner::BuildPlan`:** takes the extent as a new parameter and uses `FPlayAreaMath`.
- **`USectorGenStep_Population::Begin`:** passes `Context.Definition.PlayAreaExtent`.
- **`AAsteroidField::BeginFill`:** takes the extent as a new parameter and stores it for `MakeNextTransform`.
  - Belt: ring via `FPlayAreaMath`.
  - Sphere: the existing even-volume method in unit space, scaled per axis, giving an ellipsoid.
  - Box: as described above.
- **`USectorGenStep_AsteroidField::Begin`:** passes the extent.
- **`ASectorPreviewer`:** has no sector, so give it a `PreviewPlayAreaExtent` property with the same default.
- **`USectorGenStep_Gates`:** places gates at `FPlayAreaMath::ToWorld(Link.NormalizedLocation, Extent)`.
- **`AJumpGate::Configure`:** takes the world location as a separate argument, since the link no longer holds one.
- **`USectorDirectorSubsystem::GetArrivalTransform`:** resolves the gate's world location the same way.

### 4. Director API

```cpp
/** Half-size of the current sector's playable box, centred on the level origin. During a regeneration, the sector being generated. */
UFUNCTION(BlueprintPure, Category = "ProcSector")
FVector GetPlayAreaExtent() const;
```

Return `Context.Definition.PlayAreaExtent`, the same source the steps use.

### 5. Keeping a bounds box in sync: `UPlayAreaSyncComponent` (new, `Gates/` or `Environment/`, your choice; justify it)

A component for any actor that owns a `UBoxComponent`. It is generic, so it stays in the plugin and does not reference Startech.

- **Properties:**
  - `FName BoxComponentName`: None means the first `UBoxComponent` on the owner.
  - `FVector ExtentPadding = FVector(10000.0, 10000.0, 5000.0)`: added to the play area, so the warning starts just outside the content.
- **BeginPlay:**
  - Subscribe to the director's `OnSectorReady`.
  - If the director already has a current sector (`GetCurrentSectorId()` is not None), apply immediately.
- **On sector ready:** set the box's unscaled extent to `GetPlayAreaExtent() + ExtentPadding`. Log a warning once if no box is found.
- **EndPlay:** unsubscribe.

### 6. Built-in catalog (`FDefaultCatalogFactory`)

Keep the current look of the test sectors in the new units:

- Every sector: `PlayAreaExtent = (200000, 200000, 50000)`.
- Gates: `NormalizedLocation` (0.925, 0, 0) and (-0.925, 0, 0), which is 185000 cm.
- Asteroids: `Box` distribution, `InnerFraction` 0.15, `OuterFraction` 0.95, `HeightFraction` 0.9. Keep the per-sector counts.
- Zones (`MakeZone` takes fractions):

| Zone | Inner | Outer | Height |
|---|---|---|---|
| Core | 0 | 0 | 0 |
| Inner | 0.10 | 0.35 | 0.8 |
| Middle | 0.35 | 0.65 | 0.8 |
| Outer | 0.65 | 0.95 | 0.8 |

### 7. Validation

**`UEnvironmentPreset::IsDataValid`:**
- every fraction is within 0 to 1
- `OuterFraction >= InnerFraction` for zones and asteroids

**`USectorCatalog::IsDataValid`:**
- each component of `PlayAreaExtent` is above 0
- each component of a gate's `NormalizedLocation` is within -1 to 1
- warn, without failing, if a gate lies within 0.05 of the edge, because the player would hit the bounds warning when arriving

### 8. Debug

`ProcSector.Debug 2` keeps the overlay and also draws the current play-area box each frame with `DrawDebugBox`. The overlay gains one line: `Play area: X x Y x Z m` (full size in metres).

## Out of scope

- Scaling asteroid counts automatically with volume.
- Play areas not centred on the origin.
- Moving zones off rings (enemy layout stays nested around the station).

## Determinism note

This change intentionally moves every asteroid, spawn and gate. Ids keep their meaning (generation order), but stored sector state from before the change no longer matches. There is no save system yet, so nothing needs migrating. Mention it in the summary.

## Acceptance

1. Builds cleanly with the command in `CLAUDE.md`.
2. No absolute zone, asteroid or gate distances remain anywhere in the plugin, except `PlayAreaExtent` defaults and `ExtentPadding`.
3. The documentation is updated: prose sections 8 (authoring), 9 (integration: add a section on `UPlayAreaSyncComponent`), 11 (migration checklist, bounds item) and 12 (limits), plus API reference entries for every changed or new member.
4. The summary ends with the manual editor steps, at least:
   - Add `UPlayAreaSyncComponent` to `BP_SectorBounds` and remove any hard-coded extent logic from it.
   - Re-enter values in `DA_EnvironmentPreset` and `DA_SectorCatalog` if they were filled after this spec was written.
   - In play, with `ProcSector.Debug 2`:
     - the drawn box matches the bounds box minus the padding
     - asteroids fill the box apart from the central clearing
     - gates sit inside it
     - warping still lands the ship near the return gate
5. A test with a non-square extent, for example (300000, 150000, 50000) on one sector: rings become ellipses, nothing spawns outside the box, and the bounds box resizes when warping in and out of that sector.
