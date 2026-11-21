# Feature Spec: Step Chain (generic ordered stages on one channel)

**Parent:** @docs/specs/applications/dia/systems/diadeferredstep/diadeferredstep.md (Feature 6)
**Status:** `Approved` (2026-11-21; AD-001 sign-off for the new `DiaDeferredStep` headers given the same day)
**Origin:** Draoi layer audit (@docs/specs/applications/draoi/draoi.md). CoW's `Shared/WorldStep/` was audited as pure mechanism with no world knowledge.
**Depends on:** Features 1 and 3 (scheduler, `IParallelExecutor`, save participant). Reopens a system whose Features 1 to 5 are Done: this adds Feature 6 and does not change Features 1 to 5.

## Summary
Move CoW's `WorldStepChain`, `IWorldStepStage`, `WorldStepConfig` and `IWorldStepListener` into `DiaDeferredStep` under generic names. A chain runs an ordered list of stages inside **one** deferred step: `Capture` each in order on the sim thread, `Run` each in order on a worker, `Commit` each in order on the sim thread. Nothing in it knows what a stage simulates. CoW's `WorldStepModule` stays in CoW and becomes a thin user of the Dia chain.

## Goals
- Any game can order several simulations (weather, then ecology) on one channel without writing its own chain.
- Behaviour is byte-identical to today's CoW chain: same phase machine, ordering, re-entrancy guard, pending-removal rules, save/restore behaviour. The existing CoW chain tests move with the code and pass unchanged apart from names.
- Stage capacity is configurable, not a hard-coded 8.

## Non-goals
- A module, manifest wiring, key source or cadence. Those are the application's (`WorldStepModule` stays in CoW: it is "this game's clock").
- Generalising stage dependency beyond a declared order.
- Aliases for the old names (hard cutover).

## Decisions (interview 2026-11-21, all recommended)
| # | Decision |
|---|----------|
| 1 | Lives **inside `DiaDeferredStep`** as Feature 6, not a new package (same system, no second consumer for a split). |
| 2 | Generic names (below). `applyAfterDays` becomes `applyAfterSteps`; `stepDay` becomes `stepKey`. The application maps keys to days. |
| 3 | **Configurable capacity**, fixed-size (no sim-thread allocation): template parameter or constructor capacity, default 8. |
| 4 | **Hard cutover**: delete the CoW originals, update CoW code, tests and specs in the same change. No aliases. |
| 5 | `WorldStepModule` stays in CoW. Only chain, stage interface, config and listener move. |

## Rename map
| CoW today | Dia |
|---|---|
| `CoW::WorldStep::IWorldStepStage` | `Dia::DeferredStep::IStepStage` |
| `WorldStepChain` | `StepChain` |
| `WorldStepConfig`, `LoadWorldStepConfig` | `StepChainConfig`, `LoadStepChainConfig` |
| `IWorldStepListener`, `WorldStepApplied` | `IStepChainListener`, `StepChainApplied` |
| `WorldStepApplied::stepDay` | `StepChainApplied::stepKey` |
| `WorldStepConfig::applyAfterDays` | `StepChainConfig::applyAfterSteps` |
| `kMaxStages = 8` | capacity parameter, default 8 |
| `Testing/FakeWorldStepStage.h` | `Testing/FakeStepStage.h` |

The threading contract on `IStepStage` (Capture/Commit/serialise on the sim PU, Run on one worker and never concurrent with Capture/Commit, readiness flag not written during Run) is carried over verbatim; it already speaks only of "SimPU" and "worker", which are Dia concepts.

## Acceptance Criteria
1. Every existing CoW chain, config and listener test passes after the move with only names changed (same assertions).
2. The chain with capacity 2, 8 and 16 each registers exactly that many stages and rejects the next.
3. `StepChainApplied` carries `applyKey`, `stepKey` and the committed stage count.
4. No file under `Dia/` includes anything from `CoW` or mentions world, day, weather or ecology in code identifiers.
5. `CoW/Shared/WorldStep/` contains no chain/stage/config/listener types after the cutover; `WorldStepModule` builds against the Dia types and its E2E / existing tests pass.
6. `dia check deps` is clean and the module doc for `DiaDeferredStep` lists the new public headers.

## Tasks
| # | Task | Test | Model | Notes |
|---|------|------|-------|-------|
| 1 | AD-001 sign-off for the new headers in `DiaDeferredStep` | n/a | n/a | Escalation rule: Dia placement |
| 2 | Move `IStepStage`, `StepChainConfig` (+ loader), `IStepChainListener`; capacity parameter | Config load tests moved, capacity tests (AC 2) | sonnet | Pure move plus rename |
| 3 | Move `StepChain` and `FakeStepStage` | Chain tests moved (AC 1) | sonnet | Mechanical |
| 4 | Cut CoW over: `WorldStepModule`, stages, listeners, tests, specs/plans text | CoW scoped suite; AC 5 | sonnet | Hard cutover, one commit |
| 5 | Module doc, deps check, gap audit | `dia check deps`; one full run at the end | haiku | |

## Open Design Questions
1. **Capacity as template or runtime?** A template keeps storage inline and allocation-free but puts capacity in the type; a constructor argument keeps one type. Pick when the first non-default consumer appears; default is a template with default 8.
2. **Should the listener key also carry a stage list?** Today it carries only a count; the module's history use does not need names. Leave out unless a second consumer asks.
