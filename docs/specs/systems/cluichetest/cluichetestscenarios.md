# System Spec: CluicheTestScenarios

## Parent Application
@docs/specs/applications/cluichetest.md

## Purpose

CluicheTestScenarios owns all DiaTestHarness e2e scenario files for CluicheTest. It defines what to test (smoke, stages, specific behaviors) while the harness infrastructure (Dia/DiaTestHarness) provides the how. Each scenario is a declarative JSON file describing expected CluicheTest behavior; the system also owns the test plan that gates and orders scenario execution.

## Responsibilities

- **Scenario definition** - JSON scenario files describing CluicheTest-specific expected behavior
- **Test plan** - Gate/fan-out ordering for CluicheTest scenarios
- **App-specific assertions** - Phase names, expected metrics thresholds, and behavior specific to CluicheTest's architecture (3 PUs, MainPhase, etc.)

## Non-Responsibilities

- **Harness infrastructure** - Owned by Dia/DiaTestHarness (Harness Core)
- **Engine-side observability** - Owned by DiaDebugServer
- **Deep engine validation stages** - Owned by future CluicheTest TestStages system (actual game stages that exercise engine features)

## Dependencies

| System | Relationship |
|--------|-------------|
| DiaTestHarness (Harness Core) | Provides execution infrastructure — harness runs these scenarios |
| DiaDebugServer | Provides observability — scenarios assert on data from here |
| CluicheTest ApplicationFlow | Defines the phases and modules scenarios validate |

## Directory Structure

```
Tools/e2e/scenarios/cluichetest/
  plan.json              # Test plan (gate/fan-out)
  smoke/
    smoke_test.json      # Gate scenario
  stages/               # Future: deeper test scenarios
    ...
```

## System-Specific Decisions

| ID | CTS-001 | Scenarios describe CluicheTest behavior only | Other apps own their own scenario directories | Proposed | Yes |
|----|---------|----------------------------------------------|----------------------------------------------|----------|-----|
| ID | Decision | Rationale | Status | Binding |
|----|----------|-----------|--------|---------|
| CTS-001 | Scenarios describe CluicheTest behavior only | Other apps own their own scenario directories | Proposed | Yes |
| CTS-002 | Smoke test is the gate for all other scenarios | Must pass before deeper tests run; proves app launches and connects | Proposed | Yes |
| CTS-003 | Threshold values account for Debug build overhead | CluicheTest e2e runs Debug; thresholds set conservatively | Proposed | Yes |

## Features

| Feature | Description | Spec | Status |
|---------|-------------|------|--------|
| Smoke Test Scenario | First scenario: launch, connect, reach MainFEPhase, hold stable 3s, clean exit | [smoke-test-scenario.md](../../features/cluichetest/cluichetestscenarios/smoke-test-scenario.md) | Approved |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Scope | Should this system also own integration-level scenarios (e.g. "load level X, verify Y") or only infrastructure-proving scenarios? | Both — smoke proves infrastructure, future scenarios prove CluicheTest behavior. All CluicheTest scenarios live here |
| 2 | TestStages relationship | How does this relate to the backlog's "CluicheTest TestStages" system? | TestStages defines actual in-engine test stages (game code). This system defines the external scenarios that *validate* those stages via harness. They're complementary |

## Status

`Approved`
