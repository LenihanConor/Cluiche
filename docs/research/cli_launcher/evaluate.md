# Research: Evaluate — CLI Launcher / Dia Console

**Input:** docs/research/cli_launcher/ideate.md (8 candidates scored; Candidates 5 and 9 were dropped in discussion before scoring — see ideate.md Coverage Map)

## Scoring Criteria

- **Engine Value (0.25):** Does this improve Dia/DiaCLI's reusable tooling capability, not just a one-off convenience?
- **Game Value (0.20):** Does this make CluicheTest (or any game target) easier to build/run/test as a demo/testbed? Low ceiling for all candidates here — this is developer tooling around the engine, not a gameplay or engine capability change, so no candidate scores above 3.
- **Implementation Cost (0.25):** Inverse of effort — 5 = days, 1 = months.
- **Risk (0.15):** Inverse of uncertainty — 5 = well-understood pattern/existing code to extend, 1 = many unknowns.
- **Cluiche Fit (0.15):** Aligns with module structure and PD-001–PD-007. All 8 candidates are pure Python tooling inside `Dia/DiaCLI`, so none conflict with binding platform decisions (see explore.md) — this axis mainly rewards staying close to existing patterns (Click, Rich, precedent of Python tooling already living in this repo) over introducing large new frameworks.

## Scores

| Candidate | Engine (0.25) | Game (0.20) | Cost (0.25) | Risk (0.15) | Fit (0.15) | Total |
|-----------|---------------|-------------|-------------|-------------|------------|-------|
| 1. Trogon-Style Reflected Launcher | 4 | 2 | 5 | 4 | 5 | **4.00** |
| 2. Curated Vertical-Slice Console | 4 | 2 | 3 | 3 | 4 | 3.20 |
| 3. Full Dia Console (seed doc as written) | 5 | 2 | 1 | 2 | 4 | 2.80 |
| 4. Local Web Launcher (native window) | 4 | 2 | 3 | 3 | 4 | 3.20 |
| 6. Live NDJSON Dashboard (viewer only) | 3 | 2 | 5 | 5 | 5 | 3.90 |
| 7. Exe-Only Launcher Panel | 3 | 3 | 5 | 5 | 5 | **4.10** |
| 8. Saved Presets / Favorites | 2 | 2 | 5 | 5 | 5 | 3.65 |
| 10. Reflected Registry + Execution Foundation (no UI) | 4 | 1 | 3 | 4 | 4 | 3.15 |

## Top 3 Candidates

### Rank 1: Exe-Only Launcher Panel (score: 4.10)
**Why:** Directly and minimally answers the stated #1 priority — launching known targets (googletest/cluichetest/cluicheeditor) with the right config/flags without retyping them. It's a thin extension of code that already works (`launch.py`'s `launch_target()`), so cost and risk are both as low as this list gets. Slightly edges out Candidate 1 on Game Value since it's specifically the launch workflow, and ties it on cost/risk/fit.
**Watch out for:** Scope is intentionally narrow — it does nothing for "understand all the CLI commands" (the other two stated priorities). It's a strong first slice, not a destination.

### Rank 2: Trogon-Style Reflected Launcher (score: 4.00)
**Why:** This is the candidate that actually satisfies all three of the user's stated priorities at once (browse everything, run tests, launch exes) at S-size cost, because it reflects the existing Click command tree instead of hand-authoring descriptors — directly exploiting the explore.md finding that Click already carries most of what the seed doc wanted in YAML. PD-004/PD-001 etc. don't bind Python tooling, so there's no platform-constraint friction.
**Watch out for:** Reflection has ceilings — Click's own metadata doesn't know about richer semantic types the seed doc wanted (`target`, `project`, `duration`), so some forms will render as plain text/int fields rather than smart pickers until a thin optional metadata layer is added later (this is exactly the `console/model.py` work in Candidate 10, deferred rather than dropped).

### Rank 3: Live NDJSON Dashboard (viewer only) (score: 3.90)
**Why:** Almost free (Candidate 6 only tails a file format that already exists) and directly satisfies "understand what's happening" for anything already running — no execution/launch risk at all since it never spawns a process, only reads. Highest Risk and Fit scores on the list for that reason.
**Watch out for:** Solves observation, not action — doesn't launch anything or browse commands, so on its own it's a complement to Rank 1/2, not a substitute.

## Recommendation

**Exe-Only Launcher Panel (Candidate 7)** is the strongest single first move: cheapest, lowest-risk, and squarely aimed at the priority the user called "most important," extending code that already works rather than introducing a new architecture layer — consistent with PD-006/PD-008's general platform preference for extending established, centrally-owned mechanisms over parallel ad-hoc ones. But it's worth being explicit that Rank 1 and Rank 2 aren't really competing bets: **Candidate 1 (Trogon-Style Reflected Launcher) is the generalization of Candidate 7** — once a reflected command tree exists, the `run`/`launch` forms it generates *are* the exe-launcher panel, just automatically, for every command instead of three hardcoded targets. A sensible build order is 7 → 1 → 6: ship the narrow win in days, generalize it to the full command tree once the picker pattern is proven, then layer the free NDJSON dashboard on top for live visibility into anything running. Candidates 2/3/4/10 (the seed doc's heavier architecture, and the native-window web UI) remain available later if the reflected-Textual approach hits a real ceiling — but nothing in this evaluation justifies starting there.
