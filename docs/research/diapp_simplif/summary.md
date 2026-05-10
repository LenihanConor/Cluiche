# Research Summary — DiaApplicationFlow / CluicheTest Simplification

**Session folder:** docs/research/diapp_simplif/
**Date:** 2026-05-08

## One-Line Answer

Replace DiaApplicationFlow's Phase-based architecture with a config-driven PU/Stage/Module model where config is the sole source of truth, stages replace phases, streams unify communication, and ModuleRef<T> is the only access pattern.

## Journey

1. **Explored:** The current framework has a clean mental model (PU=thread, Phase=time, Module=object, Stream=comms) but the implementation obscures it with 3 access patterns, 3 communication patterns, redundant code+config declarations, ~40 lines of registration boilerplate per type, and Phase doing structural plumbing. 11 specific pain points identified, rated by severity.

2. **Ideated:** 10 candidates generated spanning S to XL scope. Through discussion, alternatives collapsed — the user confirmed PD-002 is mutable, config is truth, and clean break is preferred. All decisions made interactively: Phase→Stage, unified streams, ModuleRef only, one-liner registration, app-wide transitions, timeout/rollback error handling, framework-level shutdown, IApplicationInspectable for debug.

3. **Evaluated:** Scored 8 implementation phases. P1 (Framework Core) is the critical path with highest dependency score. Total timeline: 14-16 weeks phased. Key risk: P1 complexity and 3-week CluicheTest blackout during migration.

4. **Chose:** Confirmed the composite approach. No competing alternatives remained — discussion resolved all design axes before evaluation.

## Chosen Work Item

**Name:** Config-driven PU/Stage/Module Architecture
**Home module:** DiaApplicationFlow (rewrite), DiaApplicationEditor (rewrite), CluicheTest ApplicationFlow (new system)
**Suggested spec type:** System (3 system specs + ~18 feature specs)
**Estimated size:** L (14-16 weeks total, phased)

## Key Insights from Exploration

- **Phase earned its removal:** CluicheTest uses phases in a dead-simple boot→run pattern. The real need (different sim loops per stage) maps better to config-declared module groups than Phase subclasses.
- **Shared modules across PUs are not a real case.** Streams handle cross-PU data flow. Each PU owns its modules. No shared tier needed.
- **"all" is the right keyword for infrastructure modules** — distinguishes them visually and semantically from stage-specific game modules.
- **TransitionTo is one entry point regardless of caller** (code, UI, automation). The framework doesn't need to know who triggered it.
- **DoStart/DoStop ARE the load/unload phases** — returning kLoading/kReady gates the transition. No extra concept needed.
- **DiaStateMachine is wrong for this** — stage transitions are one CRC + one function, not behavioral complexity.
- **The editor is a JSON editor with structure** — it reads .diaapp, validates, visualizes, writes back. No runtime dependency for the static view.
- **40 specs need superseding** — this is a redesign, not a refactor. Old specs describe a different system.

## Design Decisions Made

| Topic | Decision |
|-------|----------|
| Phase fate | Config-declared Stages (framework-managed swap) |
| Communication | Unified FrameStream + EventStream |
| Module access | ModuleRef<T> only |
| Registration | One-liner macro (C++20) |
| Init/wiring | Shared resources are modules; deps in config |
| Cross-PU | Streams are the connection; framework-owned |
| Transition timing | Start of next frame |
| PU startup order | Config array order |
| Stage scope | App-wide (all PUs transition together) |
| Shutdown | Framework-level RequestShutdown(), not a stage |
| Module failure | Assert debug, rollback release, shutdown on boot fail |
| Stage parameters | None — game state in retained modules |
| Runtime validation | Full at load, fail-fast |
| Debug exposure | IApplicationInspectable + DebugServerModule adapter |
| E2E hooks | DiaAPI commands + blocking wait_stage_ready |
| Hot reload | Re-enter current stage |
| Render during transition | Retained LoadingScreen module |
| Migration strategy | Clean break, no shim |
| Spec strategy | Supersede old specs, write fresh |

## Discarded Candidates

| Candidate | Why discarded |
|-----------|--------------|
| Reactive activation rules (wildcard) | Too implicit, trades clarity for power that's rarely needed |
| Module-only architecture (remove PU) | XL scope, loses valuable explicit thread ownership |
| Visual graph as source of truth | XL scope, editor-dependent |
| Compatibility shim | Adds weeks of throwaway code |
| DiaStateMachine for transitions | Over-engineered for one-CRC-one-function problem |
| Shared modules across PUs | Not a real case |
| PU-independent stage transitions | Stages are app-level, not PU-level |
| Incremental cleanup only | Symptoms not root cause |

## Implementation Phases

| Phase | Work | Size | Weeks |
|-------|------|------|-------|
| P1 | Framework Core (stages, lifecycle, TransitionTo, timeout/rollback) | M | 1-3 |
| P3 | Module Access (ModuleRef only, one-liner registration) | S | 1-3 |
| P4 | Config Format v2 (schema, validation, merged .diaapp) | S | 3-4 |
| P8 | Test Suite (framework tests, continuous) | M | 3-9 |
| P2 | Streams-in-config (StreamReader/Writer, remove MessageBus) | M | 4-6 |
| P5 | CluicheTest Migration (rewrite all modules) | M | 6-9 |
| P6 | Debug & E2E (IApplicationInspectable, DiaAPI commands) | M | 9-11 |
| P7 | Editor (Application Flow panel) | L | 11-16 |

## Artifacts Produced

- `docs/research/diapp_simplif/editor_mockup.html` — Full HTML mockup of the editor panel

## References

- docs/research/diapp_simplif/explore.md
- docs/research/diapp_simplif/ideate.md
- docs/research/diapp_simplif/evaluate.md
- docs/research/diapp_simplif/choose.md
