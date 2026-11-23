# TASK_ControlsScreen: follow-up (key placeholders, the "Condition failed" lines)

## 1. Key placeholders in descriptions

Descriptions and tips in `DA_ControlsHelp` can now name a key through its action. The station hub row uses this.

**How a placeholder resolves:** `{Name}` stands for `IA_Name`.

- `FControlsList::FormatWithKeys` (`Controls/ControlsList.h`) replaces it with that action's current key labels, from
  the same contexts and the same `KeyLabels` as the keycaps.
- Several keys are joined as "W / S".
- An action with no key gives "Unbound".
- An unknown name stays as written, and logs `[Controls] {Name} in "..." names no Input Action in DA_ControlsHelp`.
- The name can be any action mapped in the data's contexts, or one a row names.

**Where it runs:** on every row description in `FControlsList::Build`, and on every tip in
`UControlsScreenWidget::Rebuild`. Both run each time the screen opens, so a rebind shows the next time it's opened.

**The station hub row** now reads, in the script:

> While docked, open the station: shop, quests, upgrades. **{Interact}** or Esc closes it. Undock with the hub's Undock button.

The running game shows "… F or Esc closes it …" (main menu, `Saved/Logs/ControlsMenu3.log`). **Esc stays plain
text** because the hub hard-codes it (`EKeys::Escape` in `UStationHubWidget::NativeOnKeyDown`). It isn't IA_Pause's
mapping, so a placeholder would be wrong if Pause were ever rebound.

### Other texts that still name keys

These still name keys typed in by hand. I left them as you accepted them; each could become a placeholder.

| Text | Typed key | Placeholder |
|---|---|---|
| Throttle | "Hold W …, S to reverse" | `{Throttle}` gives "W / S" for both directions, so the sentence would need rewording ("Hold {Throttle} to fly forward or reverse"). |
| Turn / strafe | "(Tab)" | `{TargetLock}` |
| Lock target | "A/D strafe" | `{Turn}` |
| Tips 1–2 | "Tab", "A/D", "W/S" | `{TargetLock}`, `{Turn}`, `{Throttle}` |

### Tests

`Automation RunTests Startech.Controls` passes 6/6.

- **New, `Startech.Controls.DescriptionPlaceholders`:**
  - `{Lock}` and `{Throttle}` become "Tab" and "W / S".
  - After rebinding Lock from Tab to Q, the text reads "Q".
  - An unknown `{NoSuchAction}` stays as written, with the warning.
- **`RealDataCoversEveryInput` now also checks:**
  - no description or tip in the real data has a placeholder left unreplaced;
  - a row uses `{Interact}`.

`Startech.Flow` passes too. The build is clean (no warnings). The script reran and reported
`Every mapped action has a row`.

### Files

- `Controls/ControlsList.h/.cpp`: `FormatWithKeys`; `Build` uses it.
- `UI/Controls/ControlsScreenWidget.cpp`: tips formatted.
- `Controls/ControlsHelpData.h`: comments.
- `Private/Tests/StartechControlsTests.cpp`: the new test and the new checks.
- `Tools/Menu/setup_controls_help.py`: the hub text, and a docstring note on placeholders.
- `Content/UI/Controls/DA_ControlsHelp.uasset`: rewritten by the script.
- `CLAUDE.md`: a "Keys inside text" bullet.

---

## 2. The six "Condition failed" lines

### They are the engine's own startup smoke tests, not ours

`FAutomationTestFramework::RunSmokeTests()` runs every `[SmokeFilter]` test during engine start-up, just before
"Initializing Engine" (`Engine/Source/Runtime/Launch/Private/LaunchEngineLoop.cpp:4079` for commandlets, `:4376`
otherwise). Engine Core's low-level tests use `CHECK()`, which on failure logs only the bare text "Condition failed",
with no file or line (`Core/Public/Misc/LowLevelTestAdapter.h:130`).

I ran every smoke test with a JSON report (`Automation RunFilter Smoke`): 423 passed and 3 failed, for 6 errors in
all.

| # | Test (source) | Failing check | Expected | Got on this PC |
|---|---|---|---|---|
| 1 | `Core.Logging.StructuredLogFormat` (`Engine/Source/Runtime/Core/Tests/Logging/StructuredLogFormatTest.cpp`) | `:283` `TestLoc(LOCTEXT("IntegerNegative", "{IntegerNegative}"), "-64")` | `-64` | `−64` (U+2212 minus) |
| 2 | same | `:285` `TestLoc(LOCTEXT("Float", "{Float}"), "128.25")` | `128.25` | `128,25` |
| 3 | same | `:286` `TestLoc(LOCTEXT("Double", "{Double}"), "123.456")` | `123.456` | `123,456` |
| 4 | `System.Core.Experimental.UnifiedError.CreateErrorMessage` (`Engine/Source/Runtime/Core/Tests/Experimental/UnifiedError/UnifiedErrorTests.cpp`) | `:489` | `… Error with int -7` | `… Error with int −7` |
| 5 | `System.Core.Experimental.UnifiedError.CreateErrorMessageWithContext` (same file) | `:522` | `… Error with int -7` | `… Error with int −7` |
| 6 | same | `:523` | `[Error with int -7, … extra_info]` | `[Error with int −7, …]` |

### Cause: the OS locale is Swedish

The editor log says `LogInit: Using OS detected locale (sv-SE)`. Swedish number formatting writes negatives with the
Unicode minus sign and uses a decimal comma. These tests compare localized text (`FText`) against English strings.

The checks were confirmed as follows:

- **The number formatting, run directly** (editor Python, `TextLibrary`):
  - sv-SE: -64 → `'−64'` (U+2212), 128.25 → `'128,25'`
  - en-US: `'-64'`, `'128.25'`
- **The smoke tests rerun with `-culture=en-US`:** 426/426 pass, and no "Condition failed" line.
- **The log itself:** the editor prints the tests' `Error with int −7`, and a `-game` run prints `Error with int -7`.
- **Rows 1–3** are the only localized checks in that test that format a negative or fractional number.
- **Rows 4–6** are the only checks in the two failing UnifiedError tests that expect "-7".

Rows 1–3 are therefore matched by reasoning, not by a reported line (the adapter records none); the counts match
exactly.

Our code isn't involved. None of our tests are smoke tests, and the lines are logged before the Startech module's
tests run. They also appear in logs from before this task (`Startech-backup-2026.11.22-*.log`).

### Does it matter for a packaged build? No.

- **Shipping and Test:** UBT compiles dev tests only outside those configurations (`UEBuildTarget.cs:6315`;
  `WITH_DEV_AUTOMATION_TESTS` / `WITH_TESTS` = 0), and the project's `*.Target.cs` don't force them on. The tests
  aren't in the binary.
- **Development:** the tests do run at start-up. But the game falls back to `en` because the project has no sv-SE
  localization (`LogTextLocalizationManager: No localization for 'sv-SE' exists, so 'en' will be used for the locale`),
  so they pass. The `-game` logs from this task contain the tests' output and no "Condition failed" line.
- **Either way:** a failing smoke test only writes to the log. It doesn't stop start-up or change behaviour.

### Fix: none needed on our side, and nothing changed

The failures are in engine Core tests, and the fix would belong in engine code (making the tests culture-invariant),
which I didn't touch. To get a clean log:

- **Headless runs:** add `-culture=en-US` to the command line (`UnrealEditor-Cmd … -culture=en-US …`). This affects
  only that run.
- **The editor itself:** Editor Preferences → Region & Language → set Language and Region to English. This is a
  per-user editor setting, so it's your call. I didn't change it.

`CLAUDE.md` has a new "Engine log noise" section recording all this, so the lines won't be chased again.

### One related observation

For the same reason, numbers in the editor's PIE and `-Cmd` runs on this PC are formatted the Swedish way (for
example "−" and decimal commas wherever the game uses `FText::AsNumber`). Packaged and `-game` runs use English
formatting, which is what players see. The Controls, Flow and Shop tests I ran today all pass under sv-SE. I didn't
rerun the other suites for this.
