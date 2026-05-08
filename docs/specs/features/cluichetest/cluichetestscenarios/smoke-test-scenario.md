# Feature Spec: Smoke Test Scenario

## Parent System
@docs/specs/systems/cluichetest/cluichetestscenarios.md

## Summary

The first DiaTestHarness scenario for CluicheTest. Validates that the application launches, connects via WebSocket, transitions through MainLoadPhase into MainFEPhase (stable running state), holds stable for a duration without crashing or regressing, and exits cleanly via quit command. Serves as the gate in CluicheTest's test plan — all subsequent scenarios are skipped if smoke fails.

## Goals

- Prove the full harness pipeline works end-to-end with a real application
- Validate CluicheTest's happy-path lifecycle (launch → load → run → exit)
- Establish the gate scenario that protects deeper test runs from broken infrastructure
- Provide a template for future CluicheTest scenarios

## Non-Goals

- Assert on specific game behavior beyond lifecycle
- Validate rendering output or visual correctness
- Test multiple stages or stage transitions
- Performance benchmarking (just "didn't crash for N seconds")

## Acceptance Criteria

| # | Criterion | Test |
|---|-----------|------|
| AC1 | Scenario JSON is valid against harness schema | Schema validation passes at preflight |
| AC2 | CluicheTest launches and WebSocket connects within 10s | Connection established, handshake received |
| AC3 | App reaches MainFEPhase (via state-on-connect or transition event) | Phase confirmed via observation |
| AC4 | App remains stable in MainFEPhase for 3 seconds without crash or phase regression | No CRASH/TIMEOUT during hold period |
| AC5 | Quit command triggers clean shutdown with exit code 0 | Process exits 0 within teardown ladder |
| AC6 | Result file written to `Cluiche/out/CluicheTest/test-results/smoke_test.json` | File exists with PASS status |
| AC7 | Scenario serves as gate in CluicheTest test plan | plan.json references `smoke/` directory as gate |
| AC8 | Semantic preflight validates phase names against handshake topology | `MainFEPhase` confirmed as valid phase before waiting |

## Tasks

| # | Task | AC | Description |
|---|------|----|-------------|
| 1 | Write smoke_test.json | AC1, AC2, AC3, AC4, AC5, AC8 | Declarative scenario: connect, wait for MainFEPhase, hold stable 3s, quit |
| 2 | Write plan.json | AC7 | Test plan with smoke/ as gate, empty on_pass (no deeper scenarios yet) |
| 3 | Run end-to-end | AC6 | Execute via `dia test e2e --app=cluichetest`, verify PASS result file |

## Data Models

### smoke_test.json

```json
{
  "name": "smoke_test",
  "description": "Verify CluicheTest launches, reaches MainFEPhase, holds stable, exits cleanly",
  "app": "cluichetest",
  "config": "Debug",
  "timeout_seconds": 30,
  "steps": [
    {
      "action": "wait_for_connection",
      "timeout_seconds": 10
    },
    {
      "action": "wait_for_phase",
      "phase": "DummyProject::MainFEPhase",
      "timeout_seconds": 10
    },
    {
      "action": "hold_stable",
      "duration_seconds": 3
    }
  ],
  "teardown": {
    "action": "command",
    "command": "quit"
  }
}
```

### plan.json

```json
{
  "name": "cluichetest_default",
  "description": "Default test plan for CluicheTest",
  "app": "cluichetest",
  "port": 8090,
  "stages": [
    {
      "gate": "smoke/",
      "on_pass": []
    }
  ]
}
```

## Directory Structure

```
Tools/e2e/scenarios/cluichetest/
  plan.json
  smoke/
    smoke_test.json
```

## Files Modified

| File | Change |
|------|--------|
| `Tools/e2e/scenarios/cluichetest/smoke/smoke_test.json` | New |
| `Tools/e2e/scenarios/cluichetest/plan.json` | New |

## Dependencies

- **Harness Core** must be implemented first (provides AppLauncher, DiaTestClient, ScenarioRunner, plan loader)
- **DiaDebugServer handshake enhancement** must include current phase (from Harness Core Task 4)
- **CluicheTest must have DiaDebugServer module active** in its DummyStage configuration — confirmed: already included in MainLoadPhase and MainFEPhase
- **DiaAPI quit command** — new command needed for graceful shutdown. Does not exist today; current exit is UI-driven only. Useful for editors and remote debugging beyond testing
- **`hold_stable` step action** — new action type in Harness Core (stay connected N seconds, fail on crash or unexpected phase change)

## Traceability

| Level | Spec | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | CluicheTest | @docs/specs/applications/cluichetest.md |
| System | CluicheTestScenarios | @docs/specs/systems/cluichetest/cluichetestscenarios.md |
| Feature | Smoke Test Scenario | (this spec) |

## Binding Decisions Compliance

### Platform Decisions (PD-)

| ID | Decision | Compliance |
|----|----------|------------|
| PD-001 | StringCRC for IDs | N/A — scenario is JSON, not C++ code |
| PD-002 | PU/Phase/Module architecture | Compliant — scenario validates the PU phase lifecycle, references phases by their StringCRC names |
| PD-003 | Component-based entities | N/A — no component creation |
| PD-004 | No STL in public APIs | N/A — no C++ code |
| PD-005 | x64 only | Compliant — launches x64 build via DiaCLI |
| PD-006 | VS project files are source of truth | N/A — no build changes |
| PD-007 | C++20 | N/A — no C++ code |
| PD-008 | Directory.Build.props owns paths | N/A — no build changes |
| PD-009 | Output under `Cluiche/out/<AppName>/` | Compliant — results to `Cluiche/out/CluicheTest/test-results/` |
| PD-010 | `.diagame` is project root | Compliant — DiaCLI resolves via `.diagame` |

### Application Decisions (CluicheTest AD-)

| ID | Decision | Compliance |
|----|----------|------------|
| AD-001 | Three PUs (Main/Render/Sim) | Compliant — smoke test validates MainPU phase lifecycle; doesn't assert on Render/Sim PUs |
| AD-002 | Levels are code-based | N/A — scenario doesn't create levels |
| AD-003 | Entry point in Main.cpp | Compliant — harness launches the compiled app which uses this entry point |
| AD-004 | Test levels included | Compliant — DummyStage is the level that runs during smoke test |
| AD-005 | App is testbed not product | Compliant — smoke test validates testbed functionality |

### System Decisions (CTS-)

| ID | Decision | Compliance |
|----|----------|------------|
| CTS-001 | Scenarios describe CluicheTest behavior only | Compliant — references CluicheTest-specific phase names |
| CTS-002 | Smoke test is gate for all other scenarios | Compliant — plan.json places smoke/ as gate |
| CTS-003 | Thresholds account for Debug overhead | Compliant — no FPS assertions; only stability (no crash for 3s) |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Phase name | Is "DummyProject::MainFEPhase" the correct StringCRC string for the phase? | Yes — `MainFEPhase.cpp` line 16: `const Dia::Core::StringCRC MainFEPhase::kTypeId("DummyProject::MainFEPhase")` |
| 2 | DiaDebugServer | Is DiaDebugServer already a module in CluicheTest's DummyStage MainPU? | Yes — included in both MainLoadPhase and MainFEPhase. WebSocket available from MainLoadPhase onward |
| 3 | hold_stable | Is `hold_stable` a new step action or reuse of existing? | New action type for Harness Core — "remain connected, no crash or phase change for N seconds." If phase changes or process dies, fail |
| 4 | Quit command | Is there a "quit" command registered in DiaAPI for CluicheTest? | No — doesn't exist today. Adding a DiaAPI quit command as a dependency (useful beyond testing for editors/remote debugging) |
| 5 | First-time setup | Does running this scenario require any one-time setup beyond `dia env setup`? | No — harness uses `dia run` which builds + launches. No separate build step needed |

## Open Questions

None.

## Status

`Approved`
