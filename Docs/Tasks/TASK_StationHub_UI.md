# Task: station hub (Quests and Shop tabs), HUD quest tracker and quest notices

Read Startech's root `CLAUDE.md` and `Plugins/QuestFramework/CLAUDE.md` first. The QuestFramework core and Startech quest tasks are done, and the HUD tasks are done. Reuse `DA_HudStyle` (its colours, font sizes and timings), the cut-corner material `M_HUD_ChamferPanel`, and the HUD's drawing helpers.

## What the player gets

- **The hub.** While docked, the existing key that opens the shop now opens the **station hub**: a full-screen panel over the game. The shop becomes one of its tabs.
- **Quests tab:** view, accept, turn in and abandon quests, and take bounties.
- **Outside the station:** a quest tracker in the HUD's top-right corner, and short quest notices.

No quest testing should need console commands after this.

## Phase 0: discover, report, then stop

Report, with file and line references:
- how the shop key and docking work today, the shop widget (`ShopScreenWidget`) and how it's created and closed
- the input mode and mouse cursor handling while the shop is open
- any docking prompt text
- whether the QuestFramework API offers a way to ask whether a quest can be accepted or turned in without attempting it (see Phase 1)

Then stop.

## Phase 1: plugin queries (the only plugin change allowed)

The buttons must show their disabled reason before the player clicks. If they don't exist yet, add these generic queries to `UQuestSubsystem`, and document them in `Plugins/QuestFramework/Docs/DOCUMENTATION.md`:
- `bool CanAccept(FName QuestId, FText& OutReason) const`: locked, active, on cooldown, or category limit reached ("Limit reached: 3 bounties")
- `bool CanTurnIn(FName QuestId, FText& OutReason) const`: not ready, or the turn-in gate refuses

`Accept` and `TurnIn` must use the same checks, so the button and the action never disagree.

## Phase 2: the hub

**Opening and closing:**
- While docked, the shop key opens the hub; nothing happens when not docked. Escape closes it and you stay docked. The **Undock** button undocks and closes it.
- Change the docking prompt, if one exists, to "Press <key> to open Terra station", using the station's display name.
- Opening tab: **Quests** when any quest is `ReadyToTurnIn`, otherwise the last tab used (default Quests).
- **Input:** UI-only input mode with the mouse cursor while open. Restore the previous mode on close. The ship can't receive flight input while the hub is open.
- **The game keeps running,** so timed quests keep counting. The player is docked and invulnerable anyway.

**Structure:**
- **Root `WBP_StationHub`** with a C++ parent `UStationHubWidget`. Like the HUD, each panel builds its own content in C++, and the Blueprint only places the panels. That keeps the manual editor steps small.
- **`UHubTopBarWidget`, full width:**
  - the station display name on the left, 14 pt
  - credits in the Credits colour and "Cargo used / capacity", both updating live from the existing events
  - an **Undock** cut-corner button on the right
- **`UHubTabRailWidget`, left, about 110 px wide:**
  - **Quests**, with a gold count of `ReadyToTurnIn` quests when above 0, and **Shop**
  - **Upgrades**, **Ship log** and **Achievements**, shown greyed and not clickable, with a small "Coming later" under them
  - the selected tab gets the Panel fill colour and a 3 px Player-colour bar on its left edge
- **Content area: a `UWidgetSwitcher`:**
  - **Shop:** the existing `ShopScreenWidget`, embedded unchanged except for removing its own close button and its own credits display, if it has them. Report what you changed.
  - **Quests:** `UHubQuestsTabWidget`, below.

**Quests tab layout:** a list on the left (about 45%) and details on the right.

**`UQuestListWidget`**, with group headers in secondary text, 11 pt:
- **Active:** each row shows the title (12 pt) and, on the right, its state:
  - "Ready" in the Advantage colour
  - the timer as m:ss for timed quests, amber below 30 s and red below 10 s
  - a short summary underneath, such as "Story · 1/2 objectives" or "Timed · 5 / 8 drifters"
- **Available:** story and timed quests that can be accepted.
- **Bounty board:** the current offers, with the reward in the Credits colour and the header "Bounty board · refreshes when you dock".
- **Completed (N):** collapsed behind a **Show** toggle.
- The selected row gets a Player-colour border. The first row is selected on open.

**`UQuestDetailsWidget`:**
- **Header:** the title at 16 pt and a category badge: Story in Player tones, Timed in Hull tones, Bounty in Credits tones.
- **Description** at 12 pt, wrapped.
- **Objectives:** a checkbox (Advantage colour when done), the description with `{Current}` and `{Required}` filled in, a right-aligned "Current / Required", and a 10-segment progress bar for objectives that count above 1.
- **Time limit** for timed quests: "Time limit 4:00", or "Time left 2:41" while active.
- **Rewards:** credits in the Credits colour, items as "200 × LL1" (display names from the tradeable goods list).
- **Handover line** when a collect objective consumes items: "20 Ferrite will be handed over".
- **Main button** (a cut-corner button, Player colour), depending on the state:
  - **Accept** for available quests and bounty offers
  - **Turn in** when ready
  - while active and not ready, disabled showing "In progress"
  - when refused, disabled with the reason from `CanAccept` or `CanTurnIn` underneath in secondary text
- **Abandon** as a quiet text button next to it, only for active quests, with a confirmation ("Abandon First contract? Progress is lost." Abandon / Keep).

**Updates:** everything refreshes from the quest events (`OnQuestStatusChanged`, `OnObjectiveProgress`, `OnQuestTimeChanged`, `OnBoardRefreshed`). Never every frame.

## Phase 3: HUD quest tracker and notices

**`UHudQuestTrackerWidget`** in `WB_GameHUD`'s top-right corner (the space kept free for it):
- **Shows every active quest,** up to 5 under the category limits, ordered timed first, then story, then bounties.
- **Each entry:**
  - the title (12 pt Text colour), with the timer right-aligned for timed quests (amber below 30 s, red below 10 s)
  - one line for the current objective: the first incomplete one, or the next in order. For example "Chasers destroyed 4 / 10" or "Visit Rust", in secondary text.
  - when ready: "Ready to turn in at <home station name>" in the Advantage colour
- **Hidden when no quest is active.** It folds and boots with the rest of the HUD during warps.
- **Event-driven.** Put it in an Invalidation Box, except the timer text, which updates once per second from `OnQuestTimeChanged`.

**`UHudQuestNoticeWidget`**, just below the tracker:
- **Short notices:**
  - "Objective complete: Visit Rust"
  - "Ready to turn in: First contract"
  - "Quest failed: Drifter sweep" in Danger
  - "Quest complete: First contract"
- **Behaviour:** at most 3 stacked; each lasts 4 s and fades out.
- **No reward notices here.** Turn-in rewards already show in the pickup feed through the credits and cargo events, so don't duplicate them.

## Styling

- Everything uses `DA_HudStyle`. Add hub-specific values there: tab rail width, row heights, button colours for normal, hover, pressed and disabled.
- **Buttons:** cut-corner brushes from `M_HUD_ChamferPanel` dynamic material instances, with the hover state brighter and the disabled state at 40% opacity.
- **Text is in sentence case;** the tab names in the rail are proper labels.
- **No new textures or fonts.**

## Manual editor steps (list them in the summary)

- **Build `WBP_StationHub`** (parent `UStationHubWidget`): top bar across the top, tab rail on the left, and the content switcher filling the rest. Give exact anchors, offsets and sizes for 1920×1080, as in the HUD task.
- **Add the tracker and notices to `WB_GameHUD`:**

| Element | Anchor | Alignment | Position | Size |
|---|---|---|---|---|
| `UHudQuestTrackerWidget` | top right | 1, 0 | −32, 32 | 380 × 300 |
| `UHudQuestNoticeWidget` | top right | 1, 0 | −32, 344 | 380 × 120 |

- **Remove anything that opened the old shop directly,** such as Blueprint key bindings.

## Conventions

The same as before: no `TMap`, const correctness, logic in C++, doc comments, Blueprint only for layout.

## Acceptance

1. Builds cleanly.
2. Summary lists every changed file, the plugin API added in Phase 1 (if any), and the manual steps.
3. A test plan in the summary, with no console commands needed:
   - Dock: the prompt names the station. Press the key: the hub opens on Quests (or on Shop if nothing is ready). Escape closes it; Undock leaves the station.
   - Accept First contract: it moves to Active; the tracker shows it with "Visit Rust" after undocking. Warp to Rust: notice "Objective complete: Visit Rust", and the tracker shows the collect progress.
   - Collect Ferrite: the tracker counts up. Sell some in the Shop tab: it counts down. Reach 20: notice "Ready to turn in", plus the gold badge on the Quests tab when docked. Turn in: rewards appear in the pickup feed and the top bar updates; the quest moves to Completed.
   - Accept Drifter sweep: the timer runs in the tracker and the list; amber below 30 s, red below 10 s. Let it fail: the "Quest failed" notice, and it shows its cooldown in the list.
   - Accept 3 bounties: a 4th shows the disabled reason "Limit reached: 3 bounties" before clicking.
   - Abandon an active quest: the confirmation appears; Keep cancels and Abandon removes it.
   - The Shop tab works exactly as the old shop did.
