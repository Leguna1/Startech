# Task: shop polish (matching buttons, bulk buying and selling)

Read Startech's root `CLAUDE.md` first. The hub, Upgrades tab, item catalog, save and sector tier tasks are done. Reuse `DA_HudStyle`, the hub widgets and helpers, and the existing buy and sell code. Don't change the plugins.

This task comes before `TASK_CombatBalance_1_Numbers.md`. That task adds LL2 and LL3 ammo to the shop, and ammo is bought in hundreds, so bulk buying has to exist first.

## Decisions already made

- **Buttons:** the shop's buttons are plain engine-default buttons. They must match the rest of the hub (Upgrades tab, tab rail, quest board): same shape, colours, hover, pressed and disabled states, and font, all from `DA_HudStyle`.
- **Bulk quantities:** the player can buy or sell ×1, ×10, ×100, ×1,000 or Max in one action.
- **One quantity selector for the whole shop,** not one per row. It sits at the top of the shop tab and applies to every Buy and Sell button.
- **Max** means as many as possible:
  - buying: the lower of what you can afford and, for items that take cargo space, what fits in your free cargo
  - selling: everything you hold of that item
- **Clamping:** if the chosen amount is more than you can afford or fit, the button buys the largest amount that works and its label says so. For example, with ×100 selected but room for only 34, the label reads "Buy 34". If not even one fits, the button is disabled and shows the reason, in the same style as the Upgrades tab ("Need 12 more credits", "Cargo full").
- **Button labels show the total,** for example "Buy 100 · 1,500 cr" and "Sell 40 · 920 cr", with the credits in the Credits colour.
- **Shortcuts:** keys 1–5 pick ×1, ×10, ×100, ×1,000 and Max while the shop tab is open. Holding Shift while clicking a Buy or Sell button uses Max for that one click.
- **One transaction per click:** a bulk buy is one call, one credit change, one cargo change, one save request and one notice, not a loop of single purchases.

## Phase 0: discover, report, then stop

Report, with file and line references:
1. **The shop widgets:** which parts are C++ (`BindWidget`) and which were placed by hand in widget Blueprints. List every button in the shop and where it lives. Tell me which buttons the user placed in the designer.
2. **The hub's styled buttons:** how the Upgrades tab, tab rail and other hub buttons get their look (a C++ widget class, a shared Blueprint, a style asset, materials). Is there already one reusable button class?
3. **Buy and sell today:** the functions, how price and cargo are checked, and how credits and cargo change, including events, save requests and notices.
4. **Items:** which items take cargo space and how much, and which don't (ammo had no capacity limit last time). Where buy and sell prices come from.

Then stop.

## Phase 1: one shared button

- If the hub already has a reusable styled button, use it. If not, make one: `UStartechButton` (C++, `UCommonButtonBase` or `UUserWidget` around a `UButton`, whichever matches the existing hub code), with:
  - variants Primary (buy, upgrade, accept), Secondary (sell, cancel) and Toggle (the quantity chips; a selected state)
  - normal, hovered, pressed, selected and disabled looks, all from `DA_HudStyle`
  - a label and an optional second line (for the price or the reason)
- Move the Upgrades tab's button onto it too if it isn't already, so there's one source of truth.
- **Buttons in widget Blueprints the user built:** if you can swap them reliably with an editor Python script, do it and tell me what changed. If not, give exact step-by-step editor instructions (which widget, which button to replace, what class to put in, which variable name to keep) and stop until the user confirms.

## Phase 2: bulk buy and sell

- **Core:** `BuyItem(ItemId, Quantity)` and `SellItem(ItemId, Quantity)` replace or wrap the single-item functions. Each returns a result with the amount actually traded and, on failure, a reason. Add `GetMaxBuyable(ItemId)` and `GetMaxSellable(ItemId)` for the labels.
- **Prices:** total = unit price × quantity. If a unit price is fractional (the combat task may price ammo below 1 credit a round), round the total up when buying and down when selling, so the player can't profit from rounding.
- **Quantity selector:** a row of five Toggle buttons at the top of the shop tab: ×1, ×10, ×100, ×1,000, Max. ×1 is selected when the hub opens.
- **Rows:** each row's Buy and Sell labels update when the selection, credits or cargo change. Event-driven, no per-frame work, like the Upgrades tab.
- **Notices:** one notice per transaction, e.g. "Bought 100 LL2 ammo".

## Tests

Automation tests for:
- buying ×100 with enough credits and cargo
- clamping by credits and by free cargo, and the label showing the clamped number
- Max for buying and for selling
- refusing with the right reason when not even one fits
- fractional unit prices rounding up on buy and down on sell
- a bulk buy causing exactly one credit change event and one save request

Then a real play check: open the hub with `Startech.Test.Hub 1 Shop`, take a screenshot of the shop with ×100 selected, and buy once with a real mouse click if you can. Use test slot 3; overwriting it is fine.

## Done when

- every shop button matches the hub style, and there's one shared button class
- bulk buy and sell work with the five quantities, the shortcuts and Shift-click
- tests pass, and `CLAUDE.md` describes the shared button and the bulk trade functions
