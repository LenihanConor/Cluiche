# Research: Evaluate — E2E Testing

**Input:** docs/research/e2e_testing/ideate.md

## Scoring Criteria

- **Engine Value (0.25):** Improves Dia module reusability, testability, or capability as shared infrastructure
- **Game Value (0.20):** Improves CluicheTest/CluicheEditor as a demo, testbed, or shippable application
- **Implementation Cost (0.25):** Inverse of effort — 5 = very cheap (days), 1 = very expensive (months)
- **Risk (0.15):** Inverse of uncertainty — 5 = well-understood approach, 1 = highly uncertain outcome
- **Cluiche Fit (0.15):** Aligns with module structure, DiaCLI workflow, and PD-001 through PD-009

## Scores

| Candidate | Engine (0.25) | Game (0.20) | Cost (0.25) | Risk (0.15) | Fit (0.15) | Total |
|-----------|---------------|-------------|-------------|-------------|------------|-------|
| 1. Pytest + WebSocket | 3 | 4 | 4 | 5 | 3 | 3.70 |
| 2. YAML Scenarios | 3 | 4 | 3 | 4 | 3 | 3.35 |
| 3. DiaTestHarness Module | 5 | 4 | 2 | 3 | 5 | 3.70 |
| 4. DiaCLI Extension | 2 | 3 | 5 | 5 | 5 | 3.80 |
| 5. Hybrid (CLI+YAML+pytest) | 4 | 5 | 3 | 4 | 5 | 4.05 |
| 6. Record/Replay | 4 | 3 | 3 | 3 | 4 | 3.40 |
| 7. Minimal Checkpoint | 2 | 2 | 5 | 5 | 4 | 3.45 |
| 8. CDP for Editor | 2 | 4 | 3 | 3 | 3 | 2.95 |
| 9. CI Gate | 2 | 3 | 4 | 4 | 4 | 3.25 |
| 10. Multi-Stage Test Levels | 5 | 4 | 3 | 4 | 5 | 4.10 |

## Top 3 Candidates

### Rank 1: Multi-Stage Test Levels (score: 4.10)

**Why:** Engine-side test levels have the highest combined engine value and Cluiche fit. They slot naturally into the existing LevelFactory/Phase architecture (PD-002), use StringCRC identifiers (PD-001), and give tests full access to engine internals — physics contacts, component state, phase transitions. Because scenarios are levels, they're type-checked at compile time and run at full engine speed with no IPC overhead. Multi-stage testing is built into the design: each level represents a system under test, each phase represents a scenario.

**Watch out for:** Requires recompilation to add/modify scenarios. Harder for AI to author than YAML/Python. Risk of coupling test code to engine internals (tests break when refactoring). Doesn't address editor testing.

---

### Rank 2: Hybrid — DiaCLI + YAML + pytest (score: 4.05)

**Why:** The most flexible and extensible option. DiaCLI orchestration fits the existing workflow (`dia test e2e` alongside `dia run`). YAML scenarios are trivial for AI to generate and humans to review. Pytest gives rich assertions, parametrization, parallel execution, and HTML reporting. The WebSocket communication leverages DiaDebugServer — no new engine modules needed. Covers both game and editor (via different scenario sets). Scales from 3 scenarios to 300.

**Watch out for:** Three layers means more initial setup. YAML interpreter needs designing (what verbs are supported?). Tests can only observe what DebugServer exposes — need to ensure enough commands/data are registered. Slightly over-engineered if you never exceed 20 scenarios.

---

### Rank 3: DiaCLI Extension (score: 3.80)

**Why:** The fastest path to a working e2e test. Fits perfectly into the existing DiaCLI workflow (PD-009 compliance for output). Can be built in 2–3 days with minimal infrastructure. Proves the concept before investing in more complex solutions. Uses the WebSocket connection that already exists. Low risk — well-understood subprocess + WebSocket pattern.

**Watch out for:** Limited expressiveness without pytest. No parametrization, fixtures, or rich reporting. Would need to be replaced or wrapped as scope grows. Doesn't solve scenario definition (scripts are ad-hoc Python files).

## Recommendation

**Candidate 10 (Multi-Stage Test Levels)** narrowly leads because it maximizes engine value and Cluiche fit — test levels are first-class citizens of the architecture, not bolted-on scripts. However, it's strongest for testing engine systems (physics, animation, game logic) where full internal access matters.

**Candidate 5 (Hybrid)** is the better orchestration layer — it handles launch, sequencing, reporting, and scenario definition. It's also the only candidate that naturally extends to editor testing.

The real answer is **10 + 5 combined**: engine-side test levels define the scenarios and report results via DebugServer, while the hybrid harness orchestrates which levels to run, collects results, and gates CI. This gives you:
- Engine-native scenarios with full internal access (Candidate 10)
- AI-authorable YAML for simple assertion-only tests (Candidate 5)
- DiaCLI integration for workflow (Candidate 5)
- pytest reporting and CI gating (Candidate 5)

This combination honors PD-002 (scenarios as Phases within test Levels), PD-001 (StringCRC scenario IDs), and PD-009 (results to `Cluiche/out/`).
