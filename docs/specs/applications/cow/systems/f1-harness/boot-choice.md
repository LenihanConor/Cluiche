# Feature Spec: Boot Choice and Shipping Gating

**Parent:** @docs/specs/applications/cow/systems/f1-harness/f1-harness.md
**Design source:** `Output Docs/GDD/10_PrototypeStrategy/F1_Harness.md` (decision 1, Test 1)
**Depends on:** @docs/specs/applications/cow/systems/f1-harness/cow-app-skeleton.md
**Status:** `Done`

## Summary

CoW boots to the `Boot` stage and its debug menu, where a "Play" entry (the stub `FrontEnd` stage) is just another transition. There is no command-line flag and, by decision, **no shipping-build requirement**: the menu is not compiled out and no second manifest exists. Both were dropped: the flag because DiaCLI has no argument forwarding, shipping gating because it is not wanted for F-1. They can be revisited when a shipping build is planned.

## Goals

1. CoW boots to the Boot menu; selecting the `FrontEnd` entry lands on the stub and returning goes back to Boot.

## Non-Goals

1. Command-line flags and DiaCLI argument forwarding.
2. Shipping builds: no compile-time define, no shipping manifest, no symbol checks.
3. The real front end (G-1).

## Design

The `FrontEnd` stage is listed in `Boot`'s transitions and has a `"Boot"` transition back, exactly as CluicheTest stages do (see the skeleton spec). That is the whole feature; the work is delivered by the skeleton and the promotion.

## Acceptance Criteria

| # | Criterion |
|---|-----------|
| AC1 | CoW launches to the Boot menu, which lists `FrontEnd`; selecting it lands on the stub, and the stub returns to Boot. |

## Tasks

| # | Task | Test | Status | Model |
|---|------|------|--------|-------|
| 1 | Confirm the `FrontEnd` transitions exist in the skeleton and run the launch and return cycle | AC1 | Draft | haiku |

## Binding Decisions

| Source | Constraint |
|--------|------------|
| AD-004 | Dev builds boot to the menu with a Play entry. The `--play` flag and the shipping compile-out halves are superseded (user decision). |

## Open Design Questions

1. When a shipping build is planned, revisit gating (a define, a second manifest, and symbol checks were the design considered).
