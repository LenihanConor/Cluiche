# Feature Spec: Boot Choice and Shipping Gating

**Parent:** @docs/specs/applications/cow/systems/f1-harness/f1-harness.md
**Design source:** `Output Docs/GDD/10_PrototypeStrategy/F1_Harness.md` (decision 1, Test 1)
**Depends on:** @docs/specs/applications/cow/systems/f1-harness/cow-app-skeleton.md
**Status:** `Draft`

## Summary

CoW has two boot paths. A dev build starts at the `Boot` stage and its debug menu, where a "Play" entry (the stub `FrontEnd` stage) is just another transition. A shipping build starts directly at `FrontEnd` and contains no debug menu. There is no command-line flag: choosing the front end in a dev build is a menu click. This differs from the GDD wording ("a `--play` flag or a menu entry"); the flag was dropped because DiaCLI has no argument forwarding and the menu entry satisfies the same branch.

## Goals

1. Dev build: boots to the Boot menu; selecting the `FrontEnd` entry lands on the stub.
2. Shipping build: boots straight to `FrontEnd`; no Boot stage, no `BootMenuModule` registered or linked.

## Non-Goals

1. Command-line flags and DiaCLI argument forwarding.
2. Automated shipping verification (manual inspection only, per the F-1 decision).
3. The real front end (G-1).

## Design

- A compile-time define selects the manifest. `Main.cpp` loads `cow.diagame` in dev builds and `cow.shipping.diagame` when the define is set. The shipping manifest sets `initial_stage: FrontEnd`, declares no `Boot` stage, and omits `BootMenu` and its channel.
- The define also excludes `BootMenuModule`'s registration in the baseline (`DIA_MODULE`) so the module is absent from the binary, not only unused. Because the baseline is a shared library, this needs a define the baseline recognises; no existing shipping define was found, so the nearest candidate is the Release configuration.
- Dev builds may be Debug or Release; only the shipping define removes the menu.

## Acceptance Criteria

| # | Criterion |
|---|-----------|
| AC1 | Dev build launches to the Boot menu, which lists `FrontEnd`; selecting it lands on the stub. |
| AC2 | Shipping build launches directly to the `FrontEnd` stub with no Boot stage. |
| AC3 | A symbol inspection of the shipping binary shows no `BootMenuModule` symbols. |
| AC4 | The shipping manifest contains no `Boot` stage, `BootMenu` module, or `BootMenuNavRequest` channel; it passes `dia validate manifest`. |
| AC5 | CluicheTest's builds and behaviour are unchanged. |

## Tasks

| # | Task | Test | Status | Model |
|---|------|------|--------|-------|
| 1 | Add the `FrontEnd` transition from `Boot` (if not already in the skeleton) and confirm dev path | AC1 | Draft | haiku |
| 2 | Add `cow.shipping.diagame` and shipping `.diaapp` variant; select by define in `Main.cpp` | AC2, AC4 | Draft | sonnet |
| 3 | Gate `BootMenuModule` registration behind the shipping define; add the define to the shipping configuration | AC3, AC5 | Draft | sonnet |
| 4 | Verify: run dev and shipping builds through `dia`, inspect the shipping binary's symbols manually | AC1 to AC5 | Draft | haiku |

## Binding Decisions

| Source | Constraint |
|--------|------------|
| AD-001 | Task 3 edits the baseline; needs sign-off. |
| AD-004 | Dev boots to the menu; shipping compiles it out. The `--play` flag half of AD-004 is superseded by the menu entry (record in cow.md). |
| F-1 decision | Shipping verification is manual inspection only. |

## Open Design Questions

1. Which configuration defines "shipping"? There is no `DIA_SHIPPING` today. Options: a new define on a dedicated configuration, or the existing Release configuration. Needs your decision before task 3.
2. If `BootMenuModule` lives in a shared static library, the linker may still pull it in for other apps; task 3 must confirm the gating removes it from the CoW binary.
