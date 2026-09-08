# Feature Spec: Smoke Scenario

## Parent System
@docs/specs/applications/cluichetest/systems/cluichetestscenarios/cluichetestscenarios.md

## Supersedes
@docs/specs/applications/cluichetest/systems/cluichetestscenarios/smoke-test-scenario.md

## Research
@docs/research/e2e_testing/design-decisions.md (Section 6, 16)

## Traceability

| Level | Spec | Decision IDs honored |
|-------|------|---------------------|
| Platform | @docs/specs/platform/Cluiche.md | PD-005, PD-009 |
| Application | @docs/specs/applications/cluichetest/cluichetest.md | AD-001, AD-004, AD-005 |
| System | @docs/specs/applications/cluichetest/systems/cluichetestscenarios/cluichetestscenarios.md | CTS-001, CTS-002, CTS-003 |
| Cross-system | @docs/specs/applications/dia/systems/diaautomation/diaautomation.md | SD-AUT-003, SD-AUT-005 |
| Cross-system | @docs/specs/applications/dia/systems/diacli/dia-orchestrate.md | (plan format, fixture API) |

## Problem Statement

The existing smoke-test-scenario spec is based on the superseded DiaTestHarness (JSON declarative scenarios). With the new architecture (pytest + DiaAutomation + `dia orchestrate`), the smoke scenario must be rewritten as a pytest function using the `dia_client` fixture. Same intent — prove the app launches, connects, navigates between stages, holds stable, and exits cleanly — but in the new format.

## Acceptance Criteria

| # | Criterion | Verification |
|---|-----------|--------------|
| AC1 | `test_boot_smoke.py` exists at `Tools/orchestrator/scenarios/cluichetest/boot/` | File exists |
| AC2 | Scenario connects and calls `dia.app.report` — asserts app is in `Boot` stage | pytest pass |
| AC3 | Scenario calls `dia.automation.navigate_to("DummyStage")` — asserts stage changes to `DummyStage` | pytest pass |
| AC4 | Scenario holds for 3 seconds — app remains stable (report still shows `DummyStage`, no crash) | pytest pass |
| AC5 | App exits cleanly via fixture teardown (`dia.app.quit` + 5s wait + force-kill) | Process exits 0 |
| AC6 | Implicit `assert_no_log_errors` passes — no ERROR-level entries in DiaObservation session logs | pytest pass |
| AC7 | `dia orchestrate --suite=cluichetest/default` runs this scenario and exits 0 | CLI exit code 0 |
| AC8 | Plan JSON (`plans/cluichetest/default.json`) lists this scenario | Schema check |
| AC9 | Old `smoke-test-scenario.md` marked Superseded | Doc review |

## Design

### Scenario File

```python
# Tools/orchestrator/scenarios/cluichetest/boot/test_boot_smoke.py
import time

def test_smoke_journey(dia_client):
    """Boot -> navigate -> hold stable. Quit handled by fixture teardown."""
    # Verify starting state
    report = dia_client.report()
    assert report["stage"] == "Boot"

    # Navigate to DummyStage
    dia_client.navigate_to("DummyStage")
    report = dia_client.report()
    assert report["stage"] == "DummyStage"

    # Hold stable 3 seconds — app should not crash or regress
    time.sleep(3.0)
    report = dia_client.report()
    assert report["stage"] == "DummyStage"
```

### Plan JSON

```json
{
    "app": "cluichetest",
    "port": 9002,
    "config": "Debug",
    "scenarios": [
        "scenarios/cluichetest/boot/test_boot_smoke.py"
    ]
}
```

Located at `Tools/orchestrator/plans/cluichetest/default.json`. Port 9002 matches DebugServerHostModule config in the manifest.

### Prerequisites

This scenario requires the full E2E stack to be implemented:
1. Transition guards (DiaApplicationFlow)
2. Baseline commands (DiaApplicationFlow)
3. DiaAutomation system + AutomationModule added to CluicheTest manifest
4. `dia orchestrate` CLI + pytest plugin

The scenario itself is trivial — it's the integration proof that the stack works.

### Out of Scope

- Multiple stages / deep navigation testing (future TestStages system, item #6)
- Performance assertions / metric thresholds (item #8)
- Checkpoint validation (no checkpoints registered in Boot/DummyStage yet)
- Visual regression
- Multiple test functions in this file (single journey is sufficient for smoke)

## Files Touched

| File | Change |
|------|--------|
| `Tools/orchestrator/scenarios/cluichetest/boot/test_boot_smoke.py` | New — smoke scenario |
| `Tools/orchestrator/plans/cluichetest/default.json` | New — default plan |
| `docs/specs/features/cluichetest/cluichetestscenarios/smoke-test-scenario.md` | Mark Superseded |
| `docs/specs/systems/cluichetest/cluichetestscenarios.md` | Update feature row |

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Create `Tools/orchestrator/scenarios/cluichetest/boot/test_boot_smoke.py` | File exists, valid Python | Todo | haiku | |
| 2 | Create `Tools/orchestrator/plans/cluichetest/default.json` | Valid JSON, references scenario | Todo | haiku | |
| 3 | Run `dia orchestrate --suite=cluichetest/default` end-to-end | Exit 0, all assertions pass | Todo | sonnet | Integration gate — requires full stack |
| 4 | Mark old `smoke-test-scenario.md` Superseded. Update `cluichetestscenarios.md` feature table. Commit. | Doc only | Todo | haiku | |

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| PD-005 | x64 only | `dia launch cluichetest` runs x64 build. |
| PD-009 | Output under Cluiche/out/ | DiaObservation session logs (read by `assert_no_log_errors`) are in `Cluiche/out/CluicheTest/sessions/`. pytest output goes to stdout. |
| AD-001 (CT) | Three PUs (Main/Render/Sim) | Smoke validates MainPU lifecycle via stage transitions. Doesn't assert on Render/Sim PUs directly. |
| AD-004 (CT) | Test levels included | DummyStage is the target stage — exists as a test level. |
| AD-005 (CT) | App is testbed not product | Smoke scenario is engine validation, consistent with testbed purpose. |
| CTS-001 | Scenarios describe CluicheTest behavior only | References CluicheTest-specific stage names (Boot, DummyStage). |
| CTS-002 | Smoke is gate for all other scenarios | This IS the smoke/gate scenario. Plan lists it first. |
| CTS-003 | Thresholds account for Debug overhead | No timing assertions beyond "3s stable" — conservative, accommodates Debug. |
| SD-AUT-003 | Navigation hold re-arms after each transition | Scenario navigates once (Boot -> DummyStage). Each navigation requires explicit `navigate_to`. |
| SD-AUT-005 | Consistent {success, data/error} envelope | `dia_client` helpers parse the envelope. Scenario receives `data` directly. |

## Open Questions

None.

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Stage names | Is "Boot" the correct stage name in CluicheTest's manifest? | Yes. v3 manifest at `Cluiche/Assets/CluicheTest/Global/Misc/ApplicationFlow/cluiche_main.diaapp` has `stages: [{name: "Boot", ...}, {name: "DummyStage", ...}]` with `initial_stage: "Boot"`. |
| 2 | DummyStage modules | Does DummyStage include DiaDebugServer module (needed for WebSocket to remain active after navigating away from Boot)? | Yes. `DebugServerHostModule` has `stages: ["all"]` — active across all stages. WebSocket remains available after navigation. |
| 3 | AutomationModule | Does CluicheTest need AutomationModule added to its manifest for this scenario to work? | Yes. AutomationModule doesn't exist yet. It will be added as a `stages: ["all"]` module on MainPU as part of the DiaAutomation implementation (item #3). This is a prerequisite, not part of this scenario's tasks. |

## Status

`Approved` (2026-05-21) — Steps 1–5 complete.
