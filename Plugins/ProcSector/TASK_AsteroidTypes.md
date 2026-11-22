# Task: several asteroid types per sector

Read `Plugins/ProcSector/CLAUDE.md` first. All its rules apply. The play-area, enemy-pooling and zone-respawn tasks are done; build on that code.

## Goal

A preset can list several asteroid types: different meshes, sizes, tints and shares of the total count. Sectors can then have their own mix, such as icy chunks near Glacier or red rocks near Rust. Today `AAsteroidField` draws every asteroid with one mesh.

**For now asteroids are visual only.** Mining, shooting them apart, bouncing off ships by mass, and collision damage come later. Don't build any of that now, but don't block it either (see "Later" at the end).

## 1. Data (`Data/SectorTypes.h`)

New struct, each field with a doc comment:

```cpp
USTRUCT(BlueprintType)
struct PROCSECTOR_API FAsteroidTypeParams
{
	GENERATED_BODY()

	TSoftObjectPtr<UStaticMesh> Mesh;              // mesh for this type
	TSoftObjectPtr<UMaterialInterface> Material;   // optional override; empty = the mesh's own material
	float Weight = 1.0f;                           // share of the field's Count, relative to the other types
	float MinScale = 2.0f;                         // per-axis scale range for this type
	float MaxScale = 12.0f;
	FLinearColor Tint = FLinearColor(0.25f, 0.22f, 0.2f);  // written to the material's "Color" parameter
};
```

In `FAsteroidFieldParams`, add `TArray<FAsteroidTypeParams> Types`.

**When `Types` is empty**, the field behaves exactly as today: the field's own mesh, plus the field-level `MinScale`, `MaxScale` and `Tint`. Keep those fields and say in their comments that they apply only when `Types` is empty.

## 2. Asteroid field (`Environment/AsteroidField`)

**Components.** One instanced mesh component per type, created on demand and reused between sectors, the same way `ACelestialSkyActor` manages its spheres.
- Unused components are cleared and hidden.
- The existing `Instances` component stays as the component for the "no types" case.
- Use `UInstancedStaticMeshComponent`, with the same collision and shadow settings as `Instances`.

**Loading meshes.** Type meshes are soft references. Load them synchronously in `BeginFill`; this runs during the warp, so a short load is hidden.

**Choosing a type, deterministically.** For each asteroid, after all of its existing draws, draw one more value to pick a type by weight. With `Types` empty, make no extra draw, so existing sectors stay identical. Then use the type's scale range and tint.

**Ids stay global.** An asteroid's id is still its generation order across all types, so `FSectorState::RemovedAsteroidIds` keeps working. Keep two lookups, both as `TArray`s (no `TMap`):
- id to (type index, instance index)
- per type: instance index to id, for hits

Update these to use the lookups:
- `RemoveAsteroid(Id)`
- `GetAsteroidIdFromHit(Hit)`: find which component was hit, then map `Hit.Item` to the id
- `ClearField`
- `GetAsteroidCount`

Add one query for later gameplay:

```cpp
/** Returns the asteroid's type index (-1 when Types is empty) and its world transform. False if the id is unknown or removed. */
bool GetAsteroidInfo(const int32 AsteroidId, int32& OutTypeIndex, FTransform& OutTransform) const;
```

**Tint.** Use one dynamic material instance per component, writing the type's `Tint` to `TintParameter` ("Color").

## 3. Validation (`UEnvironmentPreset::IsDataValid`)

For each type:
- Mesh is set
- Weight > 0
- MaxScale >= MinScale

## 4. Preview and docs

- `ASectorPreviewer` keeps working with types. It calls the same fill.
- Documentation:
  - **Section 8:** asteroid types, with the advice that 3–5 types is the sweet spot, since each type is one more draw call.
  - **Section 7:** the generated meshes (`SM_Asteroid_*`) and `M_Asteroid`, built by `Tools/build_asteroid_material.py`. The material multiplies the meshes' baked vertex colours by `Color` and varies brightness per instance with `PerInstanceRandom`.
  - **API reference:** entries for the new members.

## Out of scope (and "Later")

None of this gets built now. It is the planned direction, so the design above stays compatible with it:

- **Interactive asteroids:** instances near a ship or under fire get swapped for pooled physics actors (mass from type and scale, health, resources), and swapped back when they come to rest far away. `GetAsteroidInfo` plus `RemoveAsteroid` is the hook for that swap.
- **Types gain gameplay fields later:** density for mass, health, resource tag, and collision damage above an impact speed.
- **Instanced mesh collision stays simple blocking for now.** Ships stop on contact, with no bounce.

## Acceptance

1. Builds cleanly.
2. With `Types` empty, every sector looks exactly as before (same positions, same ids).
3. With 5 types set, asteroids show all five meshes in roughly the weighted proportions. Mining (`RemoveAsteroid` via a hit) still works on every type, and a mined asteroid of any type stays gone after warping away and back.
4. The summary ends with the manual editor steps:
   - Import the five `SM_Asteroid_*` FBX files into `/Game/Meshes/Asteroids`:
     - Import Vertex Color: **Replace**
     - Build Nanite: **on**
     - Combine Meshes: **on**
     - Collision is taken from the included `UCX_` hulls, so turn **Generate Missing Collision** off
   - Run `Tools/build_asteroid_material.py`, then set `M_Asteroid` as the material on all five meshes.
   - In each preset's Asteroids, add 5 types with these meshes. Example weights:
     - Lumpy 3
     - Potato 2
     - Shard 1.5
     - Cratered 1
     - Chunk 2
   - Tints per sector to taste.
