# Research: Choice — DiaApplicationFlow / CluicheTest Simplification

**Date:** 2026-05-08
**Chosen candidate:** Config-driven PU/Stage/Module architecture (phased clean rewrite)

## Rationale

The current DiaApplicationFlow framework has a clean mental model (PU=thread, Phase=time, Module=object, Stream=comms) but the implementation obscures it with: 3 module-access patterns, 3 communication patterns, redundant manifest+code declarations, ~40 lines of macro boilerplate per type, nested StartData classes, and Phase doing structural plumbing disguised as a lifecycle concept.

Through structured discussion, all alternatives collapsed into a single composite approach that preserves the concepts that work (PU as thread owner, Module as behavioral unit) and replaces the sources of confusion (Phase → config-declared Stage, MessageBus/Observer → unified streams, multiple access patterns → ModuleRef<T> only, redundant code registration → config is sole source of truth).

The user confirmed: PD-002 is open to change, config is truth, clean break (no compatibility shim), app-wide stage transitions, framework-managed lifecycle with timeout/rollback.

## What Was Ruled Out

| Candidate | Reason not chosen |
|-----------|------------------|
| Wildcard: reactive activation rules | Too implicit — trades clarity for flexibility. Simple stage model covers 95% of use cases |
| Module-only architecture (remove PU entirely) | XL scope, loses explicit thread ownership which is valuable for game engines |
| Visual graph as source of truth | XL scope, editor-dependent, overkill for data that's well-served by JSON |
| Incremental cleanup (ModuleRef only, no structural change) | Solves symptoms not root cause — Phase complexity remains |
| Compatibility shim / gradual migration | Adds 2-3 weeks of shim code and leaves two systems in the codebase temporarily |
| DiaStateMachine for stage transitions | Over-engineered — one CRC + one function is all that's needed |
| Shared modules across PUs | Not a real case — streams handle cross-PU communication |
| PU-independent stage transitions | Stages are an app concept, not a PU concept — app-wide is correct |

## Pre-Spec Commitments

### Core Model
- **PU** = thread + owns modules. Rarely subclassed.
- **Stage** = config-declared named group of modules. Framework manages transitions (stop/start diff).
- **Module** = behavioral unit. DoStart (returns kLoading/kReady/kFailed), DoUpdate, DoStop (returns kStopping/kDone).
- **Stream** = framework-owned inter-PU channel. Declared at app level in config. StreamReader<T>/StreamWriter<T> handles.
- **ModuleRef<T>** = sole module access pattern.
- **Registration** = one-line macro per class.

### Lifecycle Decisions
- Transitions execute at start of next frame (async, queued)
- App-wide stage transition — all PUs transition together
- PU startup order = array order in config
- Modules have configurable start/stop timeouts
- Failure: assert in debug, rollback in release, shutdown on boot failure
- Shutdown is framework-level (RequestShutdown), not a stage
- Hot reload = re-enter current stage
- No stage parameters — game state lives in retained modules
- "all" keyword for infrastructure modules (always active)
- Render during transition: retained LoadingScreen module handles it

### Config & Format
- Config is sole source of truth (code never declares structure)
- Single merged .diaapp per app (not split per PU)
- .diaapp v2 schema: stages at root level, streams section, module stage membership
- .diagame unchanged, .diastage unchanged (may grow in future)
- Full validation at manifest load time, fail-fast

### Debug & Automation
- IApplicationInspectable interface (framework exposes state)
- DebugServerModule adapts to DiaAPI over WebSocket
- DiaAPI commands: get_app_state, get_active_modules, transition_to, wait_stage_ready
- TransitionTo callable from code, UI, or external automation (same entry point)

### Process
- Clean break — no shim, rewrite modules in one pass
- Old specs marked Superseded, new specs written fresh
- ~40 files superseded, ~3 system specs + ~18 feature specs to write
- CluicheTest will be broken during migration phase (~3 weeks)

### Editor
- Application Flow panel in CluicheEditor
- Graph view (PUs as nodes, streams as edges)
- Stage timeline (module presence across stages)
- Module inspector (deps, timeouts, streams, stage membership, provenance)
- Stream inspector (type, from/to, readers/writers)
- Live mode slot (show runtime state from IApplicationInspectable)
- Validation bar (deps, orphans, stream connectivity, stage coverage)

## Next Step

Run /spec-system to create `diaapplication-v2` system spec. This research summary provides the full context.

Suggested spec structure:
- System spec: `docs/specs/systems/dia/diaapplicationflow.md`
- Feature specs: `docs/specs/features/dia/diaapplication-v2/` (one per phase or per concern)
- System spec: `docs/specs/systems/dia/diaapplicationfloweditor.md`
- System spec: `docs/specs/systems/cluichetest/applicationflow.md`
