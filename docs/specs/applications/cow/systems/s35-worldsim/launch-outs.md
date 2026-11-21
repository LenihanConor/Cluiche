# Feature Spec: S-3.5 Launch-outs

**Parent:** @docs/specs/applications/cow/systems/s35-worldsim/s35-worldsim.md
**Builds on:** @docs/specs/applications/cow/systems/s35-worldsim/stage-and-run-mode.md (Feature 1), @docs/specs/applications/cow/systems/f3-worldassets/world-assets-module-and-bundle-load.md (session hint)
**Mockup:** @docs/specs/applications/cow/systems/s35-worldsim/mockups/s35-world-sim.html

**Gameplay Domains:** debug

## Summary

Two buttons, **Open in S-2.5** and **Open in S-3**, that take the current world to the testbed built for a deep look at one layer. S-3.5 shows every layer together; to understand or tune weather you go to S-2.5, ecology to S-3, with the same world. The hand-off is the F-3 **session hint** (a small file naming the bundle) followed by a stage transition; the target stage reads the hint at start and loads that bundle in place of its own configured world.

The mechanism already exists on the target side (S-2.5 and S-3 read and consume the hint, F-3 writes it). This feature adds the buttons, the rules for when they are allowed, and the order of the steps so a launch-out never half-happens.

## Problem

Going from "something looks wrong in the whole world" to "let me tune just the weather on this world" otherwise means remembering the world, leaving the stage, picking another from the Boot menu and loading it by hand. Stage transitions carry no arguments, and a world that exists only as live tuning cannot be named at all. A launch-out has to hand over a world that can actually be reloaded, and has to say clearly when it cannot.

## Goals

1. **One click to the same world.** The target stage opens with the bundle that was loaded here.
2. **Only hand over what can be reloaded.** A world with unsaved changes cannot be named by a bundle, so the launch-out is refused with the reason and the route to fix it.
3. **All or nothing.** The hint and the transition happen together; a failure leaves no hint behind.
4. **No new mechanism.** The F-3 hint and the existing phase transition, nothing else.

## Non-Goals

- Carrying live state: tuning, the clock, selections, the current layer or the timeline do not travel (only the bundle id does).
- "Open in S-3.5" buttons in S-2.5 and S-3 (a natural follow-up, built on the same service, not specced here).
- Launching from Regression mode (a regression run owns the world).
- Passing arguments through stage transitions (rejected in the system spec, Resolved Decision 1).
- Launching a stage that is not in the build.

## Architecture

A small `LaunchOutService` inside `S35WorldSimModule` (Feature 1), driven by the S35 card.

```
S35 card buttons → cow.s35.launch_out <s25|s3>
  → LaunchOutService.Check()          allowed? (guard rules, one function shared with the status flags)
  → WorldAssetsModule hint            cow.world.session.hint <bundle>   (writes out/CoW/session/next_bundle.json)
  → Application::QueuePhaseTransition(target stage)    (thread-safe; executes at the frame boundary)
  → on failure: delete the hint just written, report the reason
Target stage start: F-3's initial-bundle picker reads and deletes the hint (already specced)
```

### Targets

Configured in the stage's `.diastage`, not hard-coded:

```json
"s35": { "launchOuts": { "S-2.5": "S25WeatherPatterns", "S-3": "S3Ecology" } }
```

Each value is the stage transition id listed in `cow_main.diaapp`'s Boot transitions (AD-004). A target absent from the manifest (a testbed not yet built) disables its button with "S-2.5 is not available in this build".

### Guard rules

`LaunchOutService.Check(target)` returns `{enabled, reason}`; the command and the button use the same result.

| Condition | Result |
|---|---|
| A bundle is loaded and every part is saved (F-3 status: not `custom`, no unsaved parts) | enabled |
| The status is `custom` (live pack or terrain change since the load), or any part has unsaved changes | disabled: "this world has unsaved changes; save it as a bundle first (Author step 6)", naming the parts |
| No bundle is loaded (`none`) | disabled: "no bundle is loaded" |
| A bundle load is running, or the load state is `partial` or `failed` | disabled with the load state |
| The mode is Regression | disabled: "a regression run owns the world" |
| The target is not in the manifest | disabled as above |

The unsaved-parts text is F-3's own `reason` string, so the button, the command and the rail in Author mode say the same thing.

### The sequence

1. `Check(target)`; refuse with the reason if not enabled.
2. Write the hint: `cow.world.session.hint <bundleId>` (F-3), which records `{bundle, from_stage: "S35WorldSim"}`.
3. `QueuePhaseTransition(target)`. The transition runs at the frame boundary, like any stage change.
4. If step 3 reports failure, **delete the hint** and return the error; the stage stays as it was.
5. The target stage starts; its initial-bundle picker reads the hint if it is no more than 60 seconds old, loads the bundle in place of its `.diastage` bundle, then deletes the file (F-3). A hint that is never consumed expires by that age rule and is deleted by the next reader.

### What the target does with the bundle

The target applies its own rules to the world it receives:
- **S-2.5** checks the world with its shape prechecks; a failed precheck is reported there as "invalid world", never as a weather failure.
- **S-3** loads the bundle in its current mode.
- Both start with the clock paused after the load (A2 EC-03).

### Command and status

| Command | Effect |
|---|---|
| `cow.s35.launch_out <s25\|s3>` | Runs the sequence; returns `{ ok, bundle, target }` or an error naming the reason |

`cow.s35.status` (Feature 1) gains `launchOuts: { "S-2.5": {enabled, reason}, "S-3": {enabled, reason} }`, from the same `Check` function.

### Card

The S35 card shows the two buttons in the Run and Author areas. A disabled button shows its reason on hover, with a **Save bundle** shortcut (Author step 6) when the reason is unsaved changes. Clicking an enabled button shows nothing further: the stage changes.

### Logging and observation

- **Channel:** `S35`.
- **Info:** a launch-out (target, bundle id).
- **Warning:** a refused launch-out with its reason; a transition failure and the hint cleanup.
- **Metrics:** launch-outs by target, refusals by reason.
- **Health:** none beyond Feature 1's.

## Acceptance Criteria

1. With a loaded, fully saved bundle, **Open in S-2.5** and **Open in S-3** are enabled and the card shows both in Run and Author modes.
2. With `custom` status or any unsaved part, with no bundle, during a load or after a failed or partial load, in Regression mode, or for a target not in the manifest, the button is disabled with the reasons in the table; the unsaved-parts text equals F-3's refusal text.
3. `cow.s35.launch_out` runs the guard, writes the hint with the bundle id and `from_stage`, then queues the transition to the configured target, in that order.
4. If the transition cannot be queued, the hint written for this launch-out is deleted and the error is returned; nothing else changes.
5. The target stage loads the bundle named in the hint instead of its configured world and the hint file is gone afterwards; a hint older than 60 seconds is ignored and deleted.
6. Launch-out targets come from `s35.launchOuts` in the `.diastage`, not from code.
7. `cow.s35.status.launchOuts` and the command's refusals use one function and always agree.
8. Nothing but the bundle id is carried: the target's clock is paused and its own selection, layer and timeline start fresh.
9. A world that fails S-2.5's prechecks is reported there as an invalid world, not as a weather failure.
10. The visual result matches the launch-out buttons in the mockup.

## Test Strategy

- **GoogleTests (TDD) with fakes** for F-3 status, the hint writer, the transition requester and the manifest's transition list:
  - Every row of the guard table, including that the unsaved text equals F-3's string.
  - The sequence order, and cleanup of the hint when the transition fails.
  - `s35.launchOuts` lookup, a missing target, and `status.launchOuts` agreeing with the command.
- **Hint round trip** (F-3 tests already cover read-once and the 60-second rule); this feature adds a test that the hint it writes parses with `from_stage`.
- **E2E (DiaAutomation):** load a saved bundle in S-3.5; **Open in S-2.5**; assert S-2.5's `cow.world.status` shows the same bundle id and the hint file is gone; repeat for S-3. Where a target stage is not built yet, assert the button is disabled with its reason.
- **Visual gate:** the buttons in the mockup.

## Binding Decisions

| Source | Decision | How this feature complies |
|---|---|---|
| System Resolved Decision 1 | Launch-out hand-off by session hint file; no transition arguments | The hint plus a plain phase transition |
| AD-004 | Boot transitions are the stage list | Targets are Boot transition ids from the manifest |
| AD-005 | One stage per testbed | A launch-out changes stage; modes are not stages |
| F-3 session hint | Written by `WorldAssetsModule`, read once, expires after 60 seconds | Uses `cow.world.session.hint` and relies on the target's initial-bundle picker |
| Editor actions via DiaAPI | Every action is a command | `cow.s35.launch_out` |
| `QueuePhaseTransition` is thread-safe | Use it for transitions requested outside the owning phase | The service uses it |
| System spec | S-3.5 adds no new simulation code | Only a button, a guard and a call |

No conflicts.

## Dependencies

| Dependency | Use | Status |
|---|---|---|
| Feature 1 | Stage, mode machine, S35 card | Approved |
| F-3 Feature 2 | Session hint and initial-bundle picker; status and `reason` strings | Approved, not built |
| S-2.5 and S-3 stage start | Read the hint (already amended 2026-10-19) | Approved, not built |
| DiaApplicationFlow `QueuePhaseTransition` | Stage change | Done |
| `cow_main.diaapp` Boot transitions | Target ids | Done (entries added as testbeds are built) |

## Resolved Decisions

1. **Only a saved bundle can be launched out** (2026-10-20). A hint names a bundle id; live tuning has no name, and handing over a different world than the one on screen would be worse than a refusal. The reason points to Author step 6.
2. **The hint is written first and removed on failure** (2026-10-20), so a successful transition always finds its hint and a failed one leaves no stale file for an unrelated stage to pick up.
3. **Targets are configuration** (2026-10-20), so adding S-4 or renaming a stage is a `.diastage` change.
4. **Only the bundle travels** (2026-10-20). Carrying the layer, selection or timeline would need new state in every target; the deep-dive testbeds have their own tools.

## Open Design Questions

_None remaining._

## Status

`Approved` (2026-10-20). Plan not yet created. Build order is last, after Features 1, 3 and 2.
