# Research: Evaluate — SFML → SDL3 Swap (Window + Input)

**Input:** docs/research/sfml_sdl3_swap/ideate.md

## Scoring Criteria

- **Engine Value** (0.25): Improves Dia module reusability, capability, or platform reach
- **Game Value** (0.20): Improves CluicheTest as a demo or testbed
- **Implementation Cost** (0.25): Inverse of effort — 5 = very cheap, 1 = very expensive
- **Risk** (0.15): Inverse of uncertainty — 5 = well-understood, 1 = highly uncertain
- **Cluiche Fit** (0.15): Aligns with module structure and PD-001 through PD-007

Weighted total = (Engine×0.25) + (Game×0.20) + (Cost×0.25) + (Risk×0.15) + (Fit×0.15)

## Scores

| Candidate | Engine (0.25) | Game (0.20) | Cost (0.25) | Risk (0.15) | Fit (0.15) | Total |
|-----------|---------------|-------------|-------------|-------------|------------|-------|
| C1 DiaSdl Minimal | 4 | 4 | 4 | 4 | 5 | **4.15** |
| C2 DiaSdl + Touch | 5 | 4 | 3 | 3 | 4 | **3.85** |
| C5 In-Place Rename | 4 | 4 | 3 | 4 | 3 | **3.60** |
| C4 DiaSdl Full | 5 | 4 | 2 | 2 | 4 | **3.45** |
| C6 WndProcChain → DiaBgfx | 2 | 1 | 5 | 5 | 5 | **3.45** |
| C3 DiaSdl + Bootstrap | 4 | 3 | 3 | 3 | 4 | **3.40** |
| C7 Parallel Backends | 3 | 3 | 2 | 3 | 3 | **2.75** |
| C8 DiaWindow Unification | 5 | 2 | 1 | 1 | 4 | **2.65** |

## Top 3 Candidates

### Rank 1: C1 — DiaSdl Minimal (score: 4.15)
**Why:** The most direct path to the goal — replace the 2 DiaSFML source files with a new DiaSdl module implementing the same `IWindow` + `IInputSource` contracts. SDL3 is stable and its API maps cleanly to everything Dia::Input::Event already defines. Scored highest on Cluiche Fit because it follows the exact existing adapter-module pattern (PD-002, PD-004 satisfied without any special effort) and leaves no dead code behind. Touch and bootstrap are deliberately deferred, keeping the PR reviewable in a single session.
**Watch out for:** The SDL3 key-code mapping to `Dia::Input::EKey` requires a lookup table, not a cast — this is the only non-trivial translation work. `SystemHandle.h` also needs Linux and iOS typedefs added before the mobile builds can link.

### Rank 2: C2 — DiaSdl + Touch (score: 3.85)
**Why:** Adds `kTouchBegan` / `kTouchMoved` / `kTouchEnded` to `Dia::Input::Event::EType` alongside C1. Since Android/iOS are on the roadmap and DiaInput is being touched anyway, adding the new event types now costs ~1 day and avoids an API-breaking DiaInput change later. Existing game code is unaffected — unknown event types are simply ignored.
**Watch out for:** Touch coordinate normalisation differs between SDL3 (0.0–1.0 normalised) and the existing integer pixel space used by mouse events. A conscious decision is needed at spec time: normalised floats or pixels for touch?

### Rank 3: C5 — In-Place Rename (score: 3.60)
**Why:** Same implementation outcome as C1 but delivered by renaming DiaSFML in-place. Avoids the brief period where both DiaSFML and DiaSdl modules exist, and git tracks the evolution as a rename rather than a delete+create. Simpler from a project-management angle.
**Watch out for:** A rename ripples through the module registry, architecture doc, build scripts, and any documentation that references `DiaSFML` by name. The "can fall back to DiaSFML if DiaSdl has bugs" safety net disappears immediately. Marginally lower fit score because reviewing a rename-plus-rewrite is harder than reviewing a new-module-plus-delete.

## Recommendation

**C1 (DiaSdl Minimal)** is the clear choice. It delivers the primary goal — 4-platform window and input support — with the highest score on cost and fit, and the lowest risk of any candidate that actually achieves cross-platform capability. The scope is tight enough to spec, implement, and verify in one sprint. PD-002 (ProcessingUnit/Phase/Module) is honoured by placing `SDL_Init`/`SDL_Quit` in a Module; PD-004 (no STL in public APIs) is naturally satisfied since SDL3 is a C API.

C6 (Win32WndProcChain → DiaBgfx) deserves a mention: it scored tied with C4 at 3.45 and is S-sized with near-zero risk. It is best treated as a preparatory commit inside the same spec rather than a separate deliverable — moving the two files before the SDL3 swap makes DiaSdl cleaner and DiaBgfx correctly self-contained.

C2 (touch) is the natural immediate follow-on once DiaSdl is live and a mobile build is being attempted.
