**Spec:** @docs/specs/applications/dia/systems/diadeferredstep/step-chain.md
**Status:** In Progress

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | AD-001 sign-off for new headers | n/a | Done | n/a | Given 2026-11-21 (spec header) |
| 2 | Merged spec tasks 2+3 (inseparable: chain depends on stage/config/listener): move `IStepStage`, `StepChainConfig` (+loader), `IStepChainListener`, `StepChain` (capacity template, default 8), `Testing/FakeStepStage.h` into `Dia/DiaDeferredStep`; move the CoW chain/config/fake tests to `Tests/GoogleTests/DiaDeferredStep` with names changed only | Moved tests pass (AC 1); capacity 2/8/16 test (AC 2); `StepChainApplied` fields (AC 3) | Done | sonnet | Dia side only; CoW originals stay until task 3 so CoW keeps building |
| 3 | Cut CoW over: delete `Shared/WorldStep` chain/stage/config/listener, `WorldStepModule` + remaining CoW tests/rigs use Dia types, update specs/plans text | CoW WorldStep scoped suite; AC 5 | Pending | sonnet | Hard cutover, one commit |
| 4 | Module doc public headers, `dia check deps`, gap audit, full run | `dia check deps`; one full sharded run | Pending | sonnet | |
