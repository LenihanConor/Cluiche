# Research: Ideate — E2E Testing

**Input:** docs/research/e2e_testing/explore.md

## Candidates

### Candidate 1: Python Pytest Harness + WebSocket Client

**Home module/system:** External tooling (`Tools/e2e/`) + existing DiaDebugServer
**Size:** M (1–3 weeks)
**Description:** A pytest-based test runner that launches applications via `dia run`, connects to the DebugServer WebSocket, sends commands, subscribes to metrics/logs/state, and asserts outcomes. Scenarios defined as Python test functions using a `GameClient` fixture. Each test function is a scenario: connect, send commands (load level, wait for phase, dump state), assert on returned data. No engine-side changes needed beyond registering a few new DiaAPI commands.
**Primary value:** Tests written in Python are fast to author, easy for AI to generate, and leverage pytest's ecosystem (fixtures, parametrize, parallel, reporting).

---

### Candidate 2: YAML Scenario Files + Generic Test Runner

**Home module/system:** External tooling (`Tools/e2e/`) + existing DiaDebugServer
**Size:** M (1–3 weeks)
**Description:** Scenarios defined in YAML files (steps: load_level, wait_frames, assert_state, check_metrics). A Python runner parses YAML, translates steps to WebSocket commands, and validates results. Supports multi-stage testing via scenario groups (e.g., `rigid_body_2d/` folder with multiple `.yaml` scenarios). AI or humans author YAML without writing code.
**Primary value:** Non-programmers (or AI) can author/modify test scenarios without touching Python — just edit declarative YAML.

---

### Candidate 3: Engine-Side Test Mode (DiaTestHarness Module)

**Home module/system:** New `DiaTestHarness/` module in Dia
**Size:** L (1–2 months)
**Description:** A new Dia module that adds a "test mode" to applications. When launched with `--test-mode`, the application loads test scenario definitions (JSON/binary), executes them internally (input injection, state checkpoints, assertions), and outputs results to `Cluiche/out/<App>/test-results/`. Fully self-contained — no external process needed. Scenarios as data files consumed by the engine.
**Primary value:** Zero external dependencies; tests run at engine speed without IPC overhead; deterministic execution possible with fixed timestep forcing.

---

### Candidate 4: DiaCLI Extension (`dia test e2e`)

**Home module/system:** DiaCLI (`Tools/diacli/`) + light Python harness
**Size:** S (≤1 week)
**Description:** Extend the existing DiaCLI with a `dia test e2e` command that orchestrates everything: builds, deploys, launches app with debug server enabled, runs a folder of test scripts against it, collects results. Minimal — just subprocess management and a thin WebSocket client. Test scripts are simple Python files with a `run(client)` function. Result: pass/fail with logs.
**Primary value:** Fits existing workflow (`dia run` → `dia test e2e`); minimal new infrastructure; can be built in days.

---

### Candidate 5: Hybrid — DiaCLI Orchestration + YAML Scenarios + Pytest Validation

**Home module/system:** DiaCLI (`Tools/diacli/`) + `Tools/e2e/` + existing DiaDebugServer
**Size:** M (1–3 weeks)
**Description:** Combines the best of Candidates 1, 2, and 4. DiaCLI handles orchestration (`dia test e2e --stage rigid_body`). Scenarios defined in YAML (declarative, AI-authorable). Pytest handles execution and assertion (rich reporting, parallelism). A small Python library (`dia_e2e`) provides the WebSocket client, YAML parser, and pytest fixtures. Engine-side: register 3–5 new DiaAPI commands (load_scenario, advance_frames, dump_state, get_metrics, shutdown).
**Primary value:** Each layer does what it's best at — CLI for orchestration, YAML for definition, pytest for execution — and nothing is tightly coupled.

---

### Candidate 6: Record/Replay Input Testing

**Home module/system:** New `DiaInputRecorder/` module or extension of FrameStream
**Size:** M (1–3 weeks)
**Description:** Extend FrameStream<EventData> with serialization (JSON or binary). Add `--record` flag to capture all input during a play session. Add `--replay <file>` to play it back deterministically (with fixed timestep). After replay, dump state and compare against golden file. Good for regression — "this exact sequence of inputs should produce this exact state."
**Primary value:** Catches regressions in deterministic systems (physics, game logic) with zero manual scenario authoring — just play the game once.

---

### Candidate 7: State Checkpoint Assertions (Minimal Viable)

**Home module/system:** 2–3 new DiaAPI commands + existing DebugServer
**Size:** S (≤1 week)
**Description:** The absolute minimum viable e2e test: register `test.dump_state` and `test.assert_state` commands. External script launches game, waits for specific phase, sends dump command, compares JSON output against expected file. No scenario scripting — just "launch level X, wait, check state." Good enough to catch crashes, assert failures, and gross state errors.
**Primary value:** Achievable in 1–2 days; catches the most critical failures (crashes, broken levels) with near-zero investment.

---

### Candidate 8: pytest + Chrome DevTools Protocol for Editor

**Home module/system:** External tooling (`Tools/e2e/`) targeting CluicheEditor's CEF layer
**Size:** M (1–3 weeks)
**Description:** Since CluicheEditor uses CEF (Chromium Embedded Framework), leverage Chrome DevTools Protocol (CDP) to automate the editor UI. Python's `pychrome` or `playwright` connects to CEF's debug port. Can click buttons, fill fields, verify DOM state. Combined with the WebSocket approach for engine-side assertions. Two test channels: CDP for UI, WebSocket for engine state.
**Primary value:** Tests the editor's UI layer properly — button clicks, panel interactions, asset operations — not just the engine underneath.

---

### Candidate 9: Continuous Integration Test Gate

**Home module/system:** GitHub Actions workflow + any of the above test solutions
**Size:** S (≤1 week) — assuming a test harness already exists
**Description:** A GitHub Actions workflow that runs on PR: builds the solution, launches e2e tests (headless or windowed via virtual display), gates merge on pass/fail. Not a testing solution itself but the deployment mechanism. Requires one of the other candidates as the actual test runner.
**Primary value:** Prevents regressions from reaching master; automates what would otherwise be manual "run and check."

---

### Candidate 10: Multi-Stage Test Levels (Engine-Side Scenarios)

**Home module/system:** `Cluiche/CluicheTest/Levels/TestScenarios/` + LevelFactory
**Size:** M (1–3 weeks)
**Description:** Create dedicated test levels in CluicheTest — one per system being tested (e.g., `RigidBody2DTestLevel`, `AnimationTestLevel`). Each level has multiple phases representing scenarios (spawn bodies, apply forces, wait N frames, check positions). Results reported via DebugServer. External runner just loads levels and collects results. The scenarios live as code in the engine, not as external scripts.
**Primary value:** Scenarios have full access to engine internals; can test things that are hard to observe externally; natural fit for physics/animation testing.

## Coverage Map

The candidates span the design axes from explore.md:

- **Orchestration**: Candidates 4, 5, 9 (DiaCLI/CI) vs. 1 (pytest direct) vs. 3 (self-contained)
- **Scenario definition**: Candidate 2 (YAML) vs. 1, 4 (Python code) vs. 3, 10 (engine-side code) vs. 6 (recorded input)
- **Scope**: Candidates 7 (minimal S) → 5 (hybrid M) → 3 (full engine module L)
- **Game vs. Editor**: Candidates 1–7, 10 target game; Candidate 8 targets editor specifically
- **Build vs. Buy**: All use free tools; some extend existing infrastructure, some create new modules

Size distribution: 2× S, 5× M, 1× L (no XL — the problem is well-constrained).
