# Research Summary — E2E Testing

**Session folder:** docs/research/e2e_testing/
**Date:** 2026-05-05

## One-Line Answer

A phased e2e testing solution combining a lightweight Python/pytest harness (JSON scenarios, WebSocket to DiaDebugServer) for orchestration and breadth, with engine-side multi-stage test levels for deep internal validation of systems like DiaRigidBody2D.

## Journey

1. **Explored:** The game-engine e2e testing landscape and discovered the Cluiche codebase already has ~70% of required infrastructure — DiaDebugServer (WebSocket), DiaAPI commands, FrameStream input capture, StateSerializer, MetricsCollector, and structured logging. The main gap is orchestration and scenario definition.
2. **Ideated:** 10 candidates spanning minimal (checkpoint assertions, days) to maximal (full engine test module, months), covering external harnesses, YAML/JSON scenarios, input replay, engine-native test levels, editor CDP automation, and CI gating.
3. **Evaluated:** Multi-Stage Test Levels scored highest (4.10) for engine value and Cluiche fit; Hybrid Harness scored second (4.05) for flexibility and breadth. Analysis showed they're complementary — levels for depth, harness for orchestration.
4. **Chose:** Combined approach (10 + 5), phased delivery (harness first, test levels second), JSON scenarios, DiaRigidBody2D as first target then editor.

## Chosen Work Item

**Name:** E2E Testing Solution (Hybrid Harness + Multi-Stage Test Levels)
**Home module:** External tooling (`Tools/e2e/`) + CluicheTest test levels + existing DiaDebugServer
**Suggested spec type:** System (with two child feature specs: Harness, Test Levels)
**Estimated size:** M (1–3 weeks per phase)

## Key Insights from Exploration

- **DiaDebugServer is the backbone** — WebSocket + protobuf protocol already supports commands, state subscriptions, metrics, and log streaming. No new IPC needed.
- **JSON over YAML** — jsoncpp already in engine, zero Python deps, AI generates valid JSON reliably, no indentation/type gotchas.
- **Test levels are phases** — PD-002 (ProcessingUnit/Phase/Module) means test scenarios map naturally to Phases within test Levels. No architectural invention needed.
- **No screenshots for v1** — State dumps, metrics, logs, and crash detection cover the critical failures. Visual regression is a polish-tier concern.
- **Python deps must stay minimal** — stdlib + pytest + websockets only. No heavy frameworks, no Playwright until editor UI stabilises.
- **Output goes to `Cluiche/out/`** — PD-009 compliance; test results in `Cluiche/out/<AppName>/test-results/`.

## Discarded Candidates

| Candidate | Why discarded |
|-----------|--------------|
| YAML Scenarios | JSON chosen instead — fewer deps, no edge cases, engine already has jsoncpp |
| DiaTestHarness Module | Over-scoped monolith; test levels achieve same with lighter touch |
| DiaCLI Extension alone | Too limited for 100+ scenarios; harness wraps it anyway |
| Record/Replay | Future regression layer, not primary; brittle to changes |
| Minimal Checkpoint | Outgrown immediately; no scenario driving |
| CDP for Editor | Deferred until editor UI stabilises |
| CI Gate | Deployment concern, built on top of harness later |

## References

- docs/research/e2e_testing/explore.md
- docs/research/e2e_testing/ideate.md
- docs/research/e2e_testing/evaluate.md
- docs/research/e2e_testing/choose.md
