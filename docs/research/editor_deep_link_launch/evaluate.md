# Research: Evaluate — Editor Deep-Link Launch

**Input:** docs/research/editor_deep_link_launch/ideate.md

## Scoring Criteria

- **Engine Value (0.25):** improves Dia reusability or capability.
- **Game Value (0.20):** improves CluicheTest/CoW as a demo or testbed (here: how much faster a developer or AI gets from "slow" to "seeing the cause").
- **Implementation Cost (0.25):** inverse of effort; 5 = very cheap, 1 = very expensive.
- **Risk (0.15):** inverse of uncertainty; 5 = well understood, 1 = highly uncertain.
- **Cluiche Fit (0.15):** alignment with module structure and PD-001..PD-010, plus the standing rules (UI/business separation, editor actions via DiaAPI, no new singletons, PU boundaries are streams).

## Scores

| Candidate | Engine (0.25) | Game (0.20) | Cost (0.25) | Risk (0.15) | Fit (0.15) | Total |
|-----------|---------------|-------------|-------------|-------------|------------|-------|
| 5. Headless data queries via CLI | 4 | 4 | 5 | 5 | 5 | **4.55** |
| 2. Game discovery record | 4 | 4 | 5 | 4 | 5 | **4.40** |
| 1. Typed view request + `dia view` CLI | 4 | 4 | 4 | 4 | 5 | **4.15** |
| 3. View-state contract (`describe` + validate) | 4 | 4 | 4 | 3 | 4 | **3.85** |
| 9. In-game "Inspect in Editor" trigger | 3 | 5 | 4 | 3 | 4 | **3.80** |
| 4. Headless plugin host + web front end | 5 | 4 | 2 | 3 | 4 | **3.60** |
| 6. Editor focus mode (fallback host) | 3 | 3 | 4 | 4 | 4 | **3.55** |
| 10. `cluiche://` links + presets | 3 | 3 | 4 | 3 | 4 | **3.40** |
| 8. `editor.*` DiaAPI commands on running editor | 4 | 2 | 3 | 3 | 5 | **3.35** |
| 7. Native CEF viewer exe | 3 | 3 | 2 | 3 | 3 | **2.75** |

Scores favour cheap, low-risk pieces, as expected. Read the table with the structure from ideation in mind: 1, 2, 3 and 5 are not competitors, they are the shared foundation and are all worth building. The scores do not decide the **host**, which is the real fork (4 vs 6 vs 7).

## Top 3 Candidates

### Rank 1: Headless data queries via CLI (score: 4.55)
**Why:** `dia query perf.window --game cow` returns the numbers a view would show, as JSON, with no window and no host. It uses DebugServer's JSON wire from Python, so it is cheap and well understood, and it serves the AI-diagnosis case directly: an agent reads data first and only asks a human to open a view when needed. It adds a scriptable surface for every existing and future DebugServer channel at once.
**Watch out for:** It only covers channels the DebugServer already exposes; the Perf Inspector channels (`perf.window` etc.) do not exist until the profiling amendment lands. It does not satisfy "show me the screen" on its own.

### Rank 2: Game discovery record (score: 4.40)
**Why:** Smallest piece with the largest effect on "right game, connected": the game publishes pid, port, `.diagame` and session id under `Cluiche/out/<App>/sessions/` and every tool resolves `--game cow` from it. It also fixes the stale-port / orphan `CluicheTest.exe` class of failure and benefits Candidates 1, 4, 5, 6 and 9.
**Watch out for:** Stale-record cleanup (crash leaves a file; use pid liveness plus start time), and the DebugServer listens on all interfaces today, so the record must carry a per-launch token if the host is to authenticate.

### Rank 3: Typed view request + `dia view` CLI (score: 4.15)
**Why:** The single scriptable entry point everything else plugs into (hosts, buttons, links, presets), and the place where the "UI separated from business logic" rule is enforced: one request model, thin front ends. Host choice stays an internal detail, so `dia view entity` and `dia view perf` look the same to callers regardless of which host runs them.
**Watch out for:** Scope creep into a plugin-state framework; keep the request schema minimal (plugin, game, connect target, state JSON) and let Candidate 3 own per-plugin parameters.

## Host decision (the real fork)

| Host | Covers all plugins | Startup weight | Cost | Main risk |
|------|--------------------|----------------|------|-----------|
| 4. Headless plugin host + web front end | Yes (C++ half runs in host; no plugin uses model/view/loader) | Light (no CEF, no editor model) | M-L | Unverified: response path in the shell relay, static serving from DiaWebSocket, bridge impl effort |
| 6. Editor focus mode | Yes | Heavy (editor + CEF) | S | Not "as little as possible"; still the editor process |
| 7. Native CEF viewer | Yes | Medium | M | Duplicates editor bootstrap unless extracted first |

Candidate 4 scores slightly above 6 overall (3.60 vs 3.55) only because of Engine Value and Fit; on cost and risk 6 wins. That is the intended shape: build 6 first as the cheap guaranteed fallback if the first spike of 4 fails, and keep `dia view` host-agnostic so the switch is a manifest setting per plugin.

## Recommendation

Build the foundation first as one work item, because every other candidate depends on it and none of it is contested: **typed view request + `dia view` (1), game discovery (2), headless data queries (5), and the view-state contract (3) in a light form**. Treat the **headless plugin host (4)** as the second work item and the real user-facing goal, gated by a short spike on the three open points (response path, static serving, bridge implementation), with **editor focus mode (6)** as the pre-agreed fallback so the work is never blocked. The in-game trigger (9) and links/presets (10) follow once a view can be launched reliably. This respects PD-010 (targets resolve from `.diagame`, not `.diaapp` paths), PD-002 (the host's request intake is a Module fed by a stream, not a free thread), PD-004 (public request types use Dia containers; JSON stays behind the Dia Json wrapper), and the layer rule (the host and request model are generic and belong in Dia; call sites stay in applications). Candidate 7 should be dropped, and Candidate 8 deferred until someone needs scripted control of the full editor.
