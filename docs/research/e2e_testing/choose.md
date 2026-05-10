# Research: Choice — E2E Testing

**Date:** 2026-05-05
**Chosen candidate:** Combined (Candidate 10: Multi-Stage Test Levels + Candidate 5: Hybrid Harness)

## Rationale

The combined approach gives both depth and breadth. The hybrid harness (DiaCLI orchestration + JSON scenarios + pytest validation) provides the external orchestration layer — launching apps, sequencing tests, collecting results, gating CI. Engine-side test levels provide full internal access for systems like physics and animation where observable state alone isn't enough.

User chose phased delivery: harness first (reusable for both game and editor), test levels second (deep engine validation). JSON chosen over YAML for scenario definitions due to existing jsoncpp integration, zero Python dependencies, AI reliability, and strict predictability.

## What Was Ruled Out

| Candidate | Reason not chosen |
|-----------|------------------|
| 1. Pytest + WebSocket only | Subsumed by Candidate 5's pytest layer |
| 2. YAML Scenarios only | JSON chosen instead; YAML adds pyyaml dep and has edge cases |
| 3. DiaTestHarness Module | Over-engineered single module; test levels (10) achieve the same without a monolithic module |
| 4. DiaCLI Extension only | Too limited long-term; harness wraps DiaCLI anyway |
| 6. Record/Replay | Future addition for regression; not primary solution |
| 7. Minimal Checkpoint | Too limited; outgrown immediately |
| 8. CDP for Editor | Future consideration once editor UI stabilises |
| 9. CI Gate | Deployment layer, not a test solution; built on top of harness later |

## Pre-Spec Commitments

- **Phased delivery:** Phase 1 = Hybrid harness (Candidate 5). Phase 2 = Multi-stage test levels (Candidate 10).
- **Scenario format:** JSON (not YAML). Leverages existing jsoncpp in engine, zero extra Python deps, AI-authorable.
- **First target systems:** DiaRigidBody2D (game), then CluicheEditor.
- **Orchestration via DiaCLI:** `dia test e2e` as the entry point.
- **Communication:** WebSocket via existing DiaDebugServer — no new IPC mechanism.
- **Output:** Results written to `Cluiche/out/<AppName>/test-results/` per PD-009.
- **Python dependencies:** Minimal — stdlib + pytest + websockets (or equivalent). No heavy frameworks.
- **No screenshots for v1:** Observation via state dumps, metrics, logs, crash detection.

## Next Step

Run /spec-system with this candidate as input.
Suggested parent system: DiaApplicationFlow (testing infrastructure) or new top-level system "E2E Testing"
Phase 1 spec: Feature spec for the Hybrid Harness
Phase 2 spec: Feature spec for Multi-Stage Test Levels
