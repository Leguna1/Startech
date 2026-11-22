# Task: death sequence (a moment for the explosion before loot)

Read Startech's root `CLAUDE.md` first. The combat balance task is done, including the combat log. Reuse the pooling, health, loot, quest event and respawn code that exists. Don't change the plugins, except where enemy pooling needs a reset hook (say so first).

## The problem

When a ship's hull reaches 0, it drops its loot and vanishes in the same frame. There's no moment for an explosion, and a kill doesn't feel like anything.

## Decisions already made

- **A short dying state between hull 0 and removal:** 1 s by default (`DeathDuration` in `DA_CombatBalance`, or in the settings if that fits better). Same for enemies and the player.
- **What counts as "dead" happens at hull 0, at the start of the dying state,** so nothing waits on the animation:
  - the kill is credited: quest `TargetDestroyed` event, combat log, kill rewards decided
  - the ship stops: no firing, no AI, no boost, no input for the player
  - the ship can't be targeted or locked any more, and the player's lock on it is released, so nobody wastes shots on a wreck
  - its collision stops blocking shots and ships (so a wreck can't absorb the next volley)
- **During the dying state:**
  - the ship keeps its momentum and drifts, with a slow random tumble that builds up (cheap, code only)
  - the hull flickers or flashes towards white (a material parameter if the ship materials have one; skip it if not and tell me)
  - two or three small "secondary explosion" hooks at random points on the hull, spread over the second
- **At the end of the dying state:**
  - the final explosion: a hook that spawns a Niagara system and plays a sound at the ship's location
  - the ship mesh hides
  - **then** the loot drops, at the explosion's position
  - an enemy goes back to the pool, fully reset (no leftover tumble, flicker, disabled collision or timers)
- **Player death:** the same sequence, then the existing death flow (cargo loss, respawn at Terra) starts after it. The camera stays on the wreck during the second. Pausing during the dying state is fine; the timer pauses too.
- **Effects are hooks, not art.** I'll build the Niagara systems myself from templates later. Add soft references in data (`DeathExplosionFX`, `SecondaryExplosionFX`, `DeathSound`, per ship type, with a default), and leave them empty or use a placeholder from the engine's content if one exists. Nothing must break when they're empty.
- **No "hit the wreck to detonate it early."** Not now.

## Phase 0: discover, report, then stop

Report, with file and line references:
1. What happens today when hull reaches 0, for enemies and for the player, in order: events fired, loot drop, quest event, combat log, pool release, player death flow.
2. How the targeting and lock code would notice a target dying, and what it does today when a target vanishes.
3. Whether the ship materials have a parameter usable for a flash, and how the hit flash (if any) works.
4. Anything in pooling that could leave a released enemy in a half-dying state.
5. Whether moving the loot drop 1 s later breaks anything (the combat log's `GetLastDrop`, quests that count collected items, the test pilot).

Then stop.

## Phase 1: the dying state

Build it as described, for enemies and the player. Keep it in one place (a small component or the health component's death handling, whichever fits the existing code), so other ship types get it for free.

## Tests

- automation tests: the kill is credited at hull 0 (not after the delay); loot appears only after the delay, at the wreck's position; a dying ship can't be targeted and doesn't block shots; a pooled enemy reused after dying is fully reset; the player death flow starts after the delay
- a real play check in Terra with the test pilot or a short manual fight: kill a few drifters, die once, and describe what you see. Screenshots of a wreck mid-tumble are welcome.

## Done when

- every ship death has a 1 s dying state, then the explosion hooks, then the loot
- nothing is credited late, and no shots are wasted on wrecks
- pooled enemies come back clean
- `CLAUDE.md` describes the dying state and the effect hooks, so the Niagara systems can be plugged in later
