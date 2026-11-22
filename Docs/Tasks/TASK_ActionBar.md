# Task: action bar (replaces the weapon strip)

Read Startech's root `CLAUDE.md` first. The HUD, hub and save tasks are done. Reuse `DA_HudStyle`, `M_HUD_ChamferPanel`, the HUD drawing helpers and the item catalog in `UStartechSettings`.

## Goal

A row of cut-corner slots at the bottom centre, matching the HUD's sci-fi style. For now it holds the ammo types, grouped by weapon group: LL1–LL4 for lasers, SD1 and SD2 for the drainer. You see what's selected, how much is left, and whether your current target is in range for each group. Pressing a slot's key selects that ammo.

It replaces the current weapon strip (`UHudWeaponStripWidget`). The speed readout (`UHudSpeedReadoutWidget`) stays, to the right of the bar.

It must be built so other slot kinds (rockets, abilities, consumables) can be added later without changing the bar itself.

## Phase 0: discover, report, then stop

Report, with file and line references:
- how ammo is selected today (keys, input actions, per weapon group or global), and whether each weapon group has its own selected ammo
- the events for ammo counts: whether there is one per ammo type, or only for the selected ammo
- how weapon range is known per group
- the existing number-key bindings in `IMC_Ship`
- what `UHudWeaponStripWidget` shows that the bar must keep

Then stop.

## Phase 1: data and input

**Bar layout as data.** `UActionBarLayout`, a data asset `DA_ActionBarLayout`, holds an ordered list of slots. Each slot has:
- a slot kind; only `Ammo` for now, as a C++ enum or tag
- an item tag, for example `Item.Ammo.LL1`
- a group label shown above its group ("Lasers", "Drainer")
- an input action

The layout is fixed for now. A player-configurable bar is out of scope, but the data shape must allow it later.

**Default layout:**

| Key | Slot | Group |
|---|---|---|
| 1 | LL1 | Lasers |
| 2 | LL2 | Lasers |
| 3 | LL3 | Lasers |
| 4 | LL4 | Lasers |
| 5 | SD1 | Drainer |
| 6 | SD2 | Drainer |

Keys 7–0 are left free for future slots and aren't shown.

**Input:** `IA_Slot1` … `IA_Slot6` on keys 1–6 in `IMC_Ship`, created by an editor script. If ammo selection already uses those keys, replace the old bindings and report what was removed. Pressing a slot's key selects that ammo for its weapon group. Nothing happens while the hub or the pause menu is open.

**Events:** if they're missing, add the smallest events needed:
- a count change per ammo type
- a selected-ammo change per weapon group
- range per weapon group, so the bar can tell whether the current target is within each group's range. Reuse `UTargetRangeComponent` where possible.

**Generic slot state.** Each slot kind maps to a small struct the widget draws, without knowing what the slot is:
- `bSelected`, `Count`, `bLow`, `bEmpty`, `bOutOfRange`, `bDisabled`
- `CooldownFraction` and `CooldownSecondsLeft`, unused for ammo
- `ActiveFraction`, unused for ammo

A small provider per slot kind fills it in; for now that's only the ammo provider. This is what lets future rockets and abilities plug in.

## Phase 2: the widget

**`UHudActionBarWidget`**, drawn in `NativePaint`, event-driven, inside an Invalidation Box.

**Slots:**
- 56×56 cut-corner squares, using the same corner cut as the panels, with a 6 px gap between slots and 16 px between groups.
- The group label sits above each group, in secondary text at 11 pt.

**Inside each slot:**
- **Key label:** top left, 11 pt secondary.
- **Count:** bottom right, 11 pt, shortened above 9,999 as "12.4k".
- **Icon:** in the centre, drawn in code, no textures:
  - lasers: vertical bars, as many as the tier (LL1 is one bar, LL4 is four)
  - drainer: small downward chevrons, one per tier (SD1 one, SD2 two)
  - for future items, an optional icon texture in the item catalog replaces the drawn icon

**States, which must be readable at a glance:**

| State | Look |
|---|---|
| Normal | Panel fill at 25%, `Empty segment` border, Text-coloured icon |
| Selected | Panel fill at 45%, 2 px Player-colour border, a 3 px Player-colour underline under the slot, Player-tinted icon. One selected slot per group. |
| Low | Count in the Hull colour (amber), below a `LowAmmoThreshold` in the style (default 100) |
| Empty | Count `0` in Danger; slot and icon at 55% opacity |
| Out of range | Key label in Danger, while a target is locked and beyond that group's range |
| Disabled | Whole slot at 40% opacity, for later use |
| Cooldown (later) | A dark radial sweep over the slot with the seconds left in the centre, and one Player-colour edge flash when it's ready again |
| Active (later) | A thin bar under the slot draining with `ActiveFraction` |

Draw the cooldown and active states now, driven by the struct, so they're ready. Nothing sets them yet.

**Feedback:** selecting a slot briefly brightens its border, over 0.15 s. Ammo running out (reaching 0) flashes the slot's count once in Danger.

**Style:** add the bar's values to `DA_HudStyle`: slot size, gaps, low threshold, opacities and timings.

## Phase 3: replace the weapon strip

- Remove `UHudWeaponStripWidget` from `WB_GameHUD`'s layout steps, and from the code once nothing uses it.
- Anything it showed that the bar doesn't (for example the weapon name) is dropped, unless Phase 0 shows it's needed. Report it.
- The speed readout stays.

**Manual editor steps (list them in the summary):**
- In `WB_GameHUD`, delete the weapon strip's Invalidation Box.
- Add an Invalidation Box holding `UHudActionBarWidget`, anchored bottom centre, Alignment 0.5, 1, Position 0, −28, Size about 520 × 96 (state the exact width for 6 slots and 2 groups).
- Move the speed readout so it sits right of the bar: give the exact position.
- Run the input script, or confirm it ran.

## Conventions

The same as before: no `TMap`, const correctness, logic in C++, doc comments, Blueprint only for layout.

## Acceptance

1. Builds cleanly. Saved games still load: the selected ammo per group is restored and shows as selected.
2. A test plan in the summary:
   - Keys 1–6 select ammo; the selected slot per group shows its border and underline, and firing uses that ammo.
   - Fire until LL1 drops below 100: the count turns amber. Empty it: 0 in red, the slot dims, and the count flashes once.
   - Buy ammo in the shop: counts update with the hub open and after closing it.
   - Lock a target out of laser range but inside drainer range (or the reverse): only the out-of-range group's key labels turn red.
   - Keys do nothing while the hub or pause menu is open.
   - The speed readout still works, positioned next to the bar.
   - An automated test (as with the menu input bug): press 1–6 and check the selected ammo changes.
