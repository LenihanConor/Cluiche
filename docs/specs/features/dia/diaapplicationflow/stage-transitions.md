# Feature Spec: Per-Stage Transitions (Manifest v3)

## Parent System
@docs/specs/systems/dia/diaapplicationflow.md

## Builds On
@docs/specs/features/dia/diaapplicationflow/stage-system.md
@docs/specs/features/dia/diaapplicationflow/config-format.md
@docs/specs/features/dia/diaapplicationflow/validation.md

## Traceability

| Level | Spec | Decision IDs honored |
|-------|------|---------------------|
| Platform | @docs/specs/platform/Cluiche.md | PD-001, PD-007 |
| Application | @docs/specs/applications/dia.md | AD-001, AD-003 |
| System | @docs/specs/systems/dia/diaapplicationflow.md | SD-001, SD-002, SD-005, SD-014, SD-017, **SD-019 (new — adds this version bump)** |

## Purpose

The current `.diaapp` v2 stage model can only express **linear** application flows. `stages` is an ordered string array; `auto_stages` lists which entries auto-advance to "the next index in `stages`." There is no way to declare a branching graph — no way to say "from `Boot` you can go to `DummyStage` or `StupidStage`," and no way to come back.

A normal flow — Boot acting as a level-select hub (`Boot → {DummyStage, StupidStage}`, with each level returning to Boot) — is unrepresentable today.

This feature reshapes the stage portion of the manifest to encode transitions explicitly, **per stage**:

- Each stage object declares its own `transitions: [stageName, …]` — the allowed successors.
- Each stage object declares its own `auto_advance: bool` — whether the runtime queues a transition automatically on entry.
- `auto_advance: true` is only legal when `transitions.length == 1` (zero would have nothing to advance to; multi would be ambiguous).
- The top-level `auto_stages` array is removed; its information is now per-stage.

Manifest version bumps from 2 to 3. **Hard cutover, no backward compatibility** — the loader rejects v2 with `kVersionMismatch` (per the project's existing pattern, SD-017). All ~5 source `.diaapp` files in the repo are migrated by hand in the same change.

## Acceptance Criteria

1. **Version bump** — Top-level `version` becomes `3`. The loader rejects manifests with `version != 3`, returning `LoadResult::kVersionMismatch` and logging the mismatch. There is no v2 compatibility path.

2. **Stage objects** — `stages` is a JSON array of **objects**, not strings. Each object has:
   - `name: string` (required) — the stage identifier.
   - `transitions: [string]` (required, may be empty) — declared allowed successor stages. An empty array means terminal (no outgoing transitions).
   - `auto_advance: boolean` (required) — when true, on entry the runtime queues `TransitionTo(transitions[0])`.
   - `manifestPath: string` (optional) — retained from v2; references a `.diastage` file. Behaviour unchanged.
   String-form stage entries (the v2 shape) are no longer accepted; the loader logs a warning and skips them, but in practice this is caught by the `version` check first.

3. **`auto_stages` removed** — The top-level `auto_stages` array is **not** part of the v3 schema. The loader does not read it. If present, it is silently ignored (no warning needed — it's a leftover from v2 and the version check will already have failed). The `ApplicationManifestV3.autoStages` field is removed from the C++ struct.

4. **`initial_stage` unchanged** — Stays a top-level string referencing a `stage.name`.

5. **Per-stage in-memory model** — `StageDeclaration` gains:
   ```cpp
   Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 8> transitions;
   bool autoAdvance = false;
   ```
   Capacity 8 is generous for realistic branching (most hubs branch to 2–4 stages; 8 leaves headroom).

6. **Manifest struct rename** — `ApplicationManifestV2` is renamed to `ApplicationManifestV3` (file, struct, loader class). The old loader class `ApplicationManifestLoaderV2` is renamed `ApplicationManifestLoaderV3`. Any callers (`Application.cpp`, the editor plugin, tests) are updated. Plain text rename — no v2 type retained anywhere.

7. **New validator rules** — `ManifestValidatorV3` gains four new rules:
   | Code | Severity | Trigger |
   |------|----------|---------|
   | `TRANSITION_TARGET_INVALID` | Error | A stage's `transitions` entry names a stage not declared in `stages[].name` |
   | `AUTO_ADVANCE_AMBIGUOUS` | Error | A stage has `auto_advance: true` but `transitions.length != 1` |
   | `TRANSITION_SELF_LOOP` | Warning | A stage's `transitions` array contains its own name |
   | `STAGE_UNREACHABLE` | Warning | A non-initial stage is not the target of any other stage's `transitions` |
   The existing `UNKNOWN_STAGE` rule narrows: it still fires for `initial_stage` and module stage refs, but the `autoStages` arm is removed (the underlying field is gone).

8. **Runtime auto-advance change** — In `Application.cpp::ApplyPendingTransition` (or wherever auto-advance currently fires), the post-entry block changes from "look up `newStage` index in the array, advance to `array[index+1]`" to: "find the `StageDeclaration` for `newStage`; if `autoAdvance == true`, call `TransitionTo(decl.transitions[0])`." If `autoAdvance` is true and `transitions.length == 0` (defensive — should be caught by validator), log a warning and do not advance.

9. **Source manifest migration** — The five source-of-truth `.diaapp` files are migrated to v3 by hand in this change:
   - `Cluiche/Assets/CluicheTest/Global/Misc/ApplicationFlow/cluiche_main.diaapp`
   - `Cluiche/Assets/Stages/DummyStage/Misc/ApplicationFlow/dummy_stage.diaapp`
   - `Cluiche/CluicheEditor/Data/editor.diaapp`
   - `Cluiche/CluicheTest/ApplicationFlow/example_app.diaapp` (if it ships from source — verify)
   - `Cluiche/CluicheTest/ApplicationFlow/test_manifest.diaapp` (if it ships from source — verify)
   All contents under `bin/` and `.claude/worktrees/` are build artifacts or worktree copies and are not migrated by hand; they regenerate from source.

10. **CluicheTest works end-to-end** — `dia run cluichetest` boots, transitions through `Boot → DummyStage` (today's flow), and shuts down cleanly using v3 manifests. No regressions in existing GoogleTest suites for `ApplicationManifestLoaderV3` and `ManifestValidatorV3`.

11. **Editor (downstream)** — The editor's JS payload, types, and `StageConfiguration` sidebar must adapt to v3. **Out of scope for this feature spec** — handled in [stages-tab.md](../diaapplicationfloweditor/stages-tab.md)'s revision and in the editor's task #29 (Step 8b). This spec is the engine-side change only.

## Design

### Schema (JSON)

```json
{
    "version": 3,
    "stages": [
        {
            "name": "Boot",
            "transitions": ["DummyStage", "StupidStage"],
            "auto_advance": false
        },
        {
            "name": "DummyStage",
            "transitions": ["Boot"],
            "auto_advance": false
        },
        {
            "name": "StupidStage",
            "transitions": ["Boot"],
            "auto_advance": false
        }
    ],
    "initial_stage": "Boot",
    "streams": [...],
    "processing_units": [...]
}
```

`auto_stages` is gone. Streams, processing units, and modules are unchanged.

### C++ structs

`Dia/DiaApplicationFlow/Manifest/ApplicationManifestV3.h` (renamed from V2):

```cpp
struct StageDeclaration
{
    Dia::Core::StringCRC                                                name;
    Dia::Core::Containers::String256                                    manifestPath;

    // NEW in v3
    Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 8>       transitions;
    bool                                                                autoAdvance = false;
};

struct ApplicationManifestV3
{
    int version = 3;

    Dia::Core::Containers::DynamicArrayC<StageDeclaration, 16>          stages;
    Dia::Core::StringCRC                                                initialStage;
    // autoStages removed — moved to per-stage StageDeclaration::autoAdvance.

    Dia::Core::Containers::DynamicArrayC<StreamDeclaration, 16>         streams;
    Dia::Core::Containers::DynamicArrayC<ProcessingUnitDeclaration, 4>  processingUnits;
};
```

### Loader

`ApplicationManifestLoaderV3::ParseJson` differs from V2's `parseJson` in:

- Version check: expect 3.
- `stages` parsing: each entry MUST be an object. Read `name`, `transitions` (array of strings), `auto_advance` (bool), `manifestPath` (optional). String-form entries log a warning and are skipped — but realistically the version check rejects the file before this matters.
- `auto_stages`: not read. If present in the JSON, ignored.

```cpp
// --- stages ---
if (root.isMember("stages") && root["stages"].isArray())
{
    const Json::Value& stagesJson = root["stages"];
    for (unsigned int i = 0; i < stagesJson.size(); ++i)
    {
        const Json::Value& entry = stagesJson[i];
        if (!entry.isObject() || !entry.isMember("name"))
        {
            DIA_LOG_WARNING("ApplicationFlow",
                "ApplicationManifestLoaderV3 — stages[%u] is not an object with 'name'", i);
            continue;
        }
        StageDeclaration decl;
        decl.name = Dia::Core::StringCRC(entry["name"].asCString());

        if (entry.isMember("manifestPath") && entry["manifestPath"].isString())
            decl.manifestPath = entry["manifestPath"].asCString();

        if (entry.isMember("transitions") && entry["transitions"].isArray())
        {
            const Json::Value& tx = entry["transitions"];
            for (unsigned int t = 0; t < tx.size(); ++t)
                if (tx[t].isString())
                    decl.transitions.Add(Dia::Core::StringCRC(tx[t].asCString()));
        }

        if (entry.isMember("auto_advance") && entry["auto_advance"].isBool())
            decl.autoAdvance = entry["auto_advance"].asBool();

        outManifest.stages.Add(decl);
    }
}
```

### Validator

Existing `CheckStageReferences` is updated:
- Drops the `autoStages` loop (the field is gone).
- Adds a new pass that, for every stage, checks each entry in `transitions` exists in `validStages` → `TRANSITION_TARGET_INVALID`.

New methods:

```cpp
void ManifestValidatorV3::CheckTransitionTargets(const ApplicationManifestV3& manifest);
void ManifestValidatorV3::CheckAutoAdvanceConsistency(const ApplicationManifestV3& manifest);
void ManifestValidatorV3::CheckTransitionSelfLoops(const ApplicationManifestV3& manifest);
void ManifestValidatorV3::CheckStageReachability(const ApplicationManifestV3& manifest);
```

`CheckAutoAdvanceConsistency` rule:
```
for each stage:
    if stage.autoAdvance && stage.transitions.Size() != 1:
        AddError("AUTO_ADVANCE_AMBIGUOUS",
                 "Stage '%s' has auto_advance=true but transitions.length=%u (must be 1)",
                 stage.name);
```

`CheckStageReachability` rule:
```
build set of all referenced stages = ⋃ stage.transitions for each stage
for each stage:
    if stage.name != initialStage && stage.name not in referenced:
        AddWarning("STAGE_UNREACHABLE",
                   "Stage '%s' is not initial and not referenced by any transition",
                   stage.name);
```

### Runtime auto-advance change

`Application.cpp` lines 737–758 (the "Auto-advance: if newStage is an autoStage" block) replaced with:

```cpp
// Auto-advance: per-stage flag in v3.
for (unsigned int s = 0; s < mManifest.stages.Size(); ++s)
{
    const StageDeclaration& decl = mManifest.stages[s];
    if (decl.name == newStage && decl.autoAdvance)
    {
        if (decl.transitions.Size() == 1)
        {
            DIA_LOG_INFO("Application", "Auto-advancing from stage '%s' to '%s'",
                         newStage.AsChar(), decl.transitions[0].AsChar());
            TransitionTo(decl.transitions[0]);
        }
        else
        {
            DIA_LOG_WARNING("Application",
                "Stage '%s' has auto_advance=true but %u transitions (validator should have caught this)",
                newStage.AsChar(), decl.transitions.Size());
        }
        break;
    }
}
```

### Migration of source manifests

`cluiche_main.diaapp` today:
```json
"stages": ["Boot", "DummyStage"],
"initial_stage": "Boot",
"auto_stages": []
```

After:
```json
"version": 3,
"stages": [
    { "name": "Boot",       "transitions": ["DummyStage"], "auto_advance": false },
    { "name": "DummyStage", "transitions": ["Boot"],       "auto_advance": false }
],
"initial_stage": "Boot"
```

(Note: explicit return path `DummyStage → Boot` matches the user's design intent of stages as a navigable graph, even though today's runtime never does that transition automatically. It documents the allowed flow.)

For each source file the migration is a small mechanical rewrite — same names, just structural. No behaviour change in CluicheTest's current boot flow.

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Rename `ApplicationManifestV2.h/.cpp` → `V3`; struct + manifest class rename; add `transitions[]` + `autoAdvance` to `StageDeclaration`; remove `autoStages` from `ApplicationManifestV3` | `dia pipeline --target cluichetest` builds | Todo | sonnet | Plain rename + field add. No logic change. Touches all callers. |
| 2 | Update `ApplicationManifestLoaderV3::ParseJson`: bump expected version to 3, read stage objects, read `transitions` + `auto_advance`, drop `auto_stages` reading | New GoogleTest: `LoaderV3Test` — round-trip a v3 manifest with branching transitions; reject v2 with `kVersionMismatch` | Todo | sonnet | |
| 3 | Update `CheckStageReferences` to drop the autoStages loop; existing rule still fires for initialStage + module stage refs | Existing tests pass; one updated for v3 shape | Todo | sonnet | |
| 4 | Implement `CheckTransitionTargets` (`TRANSITION_TARGET_INVALID`) + `CheckAutoAdvanceConsistency` (`AUTO_ADVANCE_AMBIGUOUS`) | New GoogleTest cases: each rule fires on synthetic bad manifest, doesn't fire on good | Todo | sonnet | |
| 5 | Implement `CheckTransitionSelfLoops` (`TRANSITION_SELF_LOOP`) + `CheckStageReachability` (`STAGE_UNREACHABLE`) | New GoogleTest cases for both | Todo | sonnet | |
| 6 | Update `Application.cpp` auto-advance block: per-stage `autoAdvance` + `transitions[0]` lookup | Existing CluicheTest auto-advance integration test passes (boot → first stage when manifest has `auto_advance: true`) | Todo | sonnet | |
| 7 | Migrate source `.diaapp` files to v3 by hand (cluiche_main, dummy_stage, editor; verify example_app + test_manifest if they ship from source) | `dia run cluichetest` boots and shuts down cleanly | Todo | haiku | Mechanical JSON edit. List in plan. |
| 8 | Run full pipeline: `dia pipeline --target cluichetest` then `dia run cluichetest`; `dia run googletest --filter="*Manifest*"` | Quoted output of dia commands | Todo | sonnet | Verification gate per CLAUDE.md |
| 9 | Add SD-019 to `docs/specs/systems/dia/diaapplicationflow.md` Decisions table; update Stage System feature row note that v3 supersedes v2's `auto_stages` field | Doc only | Todo | haiku | |
| 10 | Commit with conventional message ("Bump .diaapp to v3 — per-stage transitions + auto_advance") | `git status` clean | Todo | haiku | After tasks 1-9 |

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| PD-001 | StringCRC for all IDs | All stage names, transition targets, and `initial_stage` remain `StringCRC`-keyed in the in-memory model. JSON form is the user-readable string. |
| PD-007 | C++20 required | New struct fields use designated initializers; `DynamicArrayC` is C++20-compatible already. |
| AD-001 | YAML frontmatter module docs | `dia.diaapplicationflow.architecture.module.md` is updated to mention the v3 schema (in the engineering plan task list, doc-only entry). |
| AD-003 | Namespace `Dia::ApplicationFlow::` | All new code stays in this namespace. |
| SD-001 | Config is sole source of truth | Reinforced — transitions are explicit in config, no implicit "next-in-array" inference. |
| SD-002 | Stages replace Phases | Reinforced — stages gain a richer model that subsumes more of what Phases used to express (branching). |
| SD-005 | Transitions queued, execute next frame | Unchanged — auto-advance still goes through the queued `TransitionTo` path. |
| SD-014 | Full validation at load | Reinforced — four new rules tighten the guarantee. |
| SD-017 | Clean break, no backward compatibility | Directly applied — v2 is rejected outright, no shim. |
| **SD-019** | **Manifest version 3 (this spec)** | This spec *introduces* SD-019. The decision text: *"Manifest schema bumps to v3. Stages become objects `{name, transitions[], auto_advance}` replacing v2's parallel `stages` (string[]) + `auto_stages` arrays. Per-stage `auto_advance` is only legal when `transitions.length == 1`. Hard cutover, no v2 compatibility path."* Binding: Yes. |

## Open Questions

None. (The "all" sentinel question for transitions is closed: `transitions` is a list of *target* stage names — the "all" sentinel applies only to `module.stages` membership, not to transitions. A transition target must be a real stage name.)

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Schema shape | Why a per-stage `transitions[]` array rather than a single top-level `transitions: { from: [to, …] }` map? | Per-stage co-locates the data with what it describes — reading a stage object tells you everything about it (name, where it can go, whether it auto-advances). A top-level map fragments that and forces cross-references. The chosen shape also matches how the editor's UI will render (per-row chip-list editor in the sidebar) and how the runtime queries it (find stage by name → read its transitions). No advantage to the map form. |
| 2 | Auto-advance | What if a future flow needs auto-advance with multiple targets (e.g., "auto-pick the first viable one based on some predicate")? | We'd extend the schema then, not now. Today's auto-advance is mechanically deterministic ("queue the one transition"). Predicate-based selection is a real future feature but it's a bigger design (where does the predicate live? how is it evaluated? what state can it read?) and forcing a decision now would either lock us into an inferior design or bloat this spec. AC #5 keeps the door open: when we add it, `auto_advance` becomes one of N strategies, and `AUTO_ADVANCE_AMBIGUOUS` becomes more nuanced. |
| 3 | Backward compatibility | The project has been running on v2 for a while. Are we sure no third party (test fixtures, mod tooling, sample projects) reads `.diaapp` files we don't control? | The `.diaapp` format is engine-internal at this point. All readers live in this repo: `ApplicationManifestLoaderV2`, the editor's JS-side parser, and the GoogleTests fixtures. No public API exposes v2 manifests externally. Hard cutover is safe. SD-017 has already established this pattern for the v1→v2 transition. |
| 4 | Runtime change | If the validator catches `AUTO_ADVANCE_AMBIGUOUS` at load time, can we remove the runtime defensive check (warning + no-op) added in AC #8? | We keep the defensive check. SD-014 says validation runs at load *and* the editor validates redundantly — but the runtime never trusts. A malformed manifest that somehow reached runtime should fail loud (warning log) rather than crash dereferencing `transitions[0]`. The cost is one `if` statement; the benefit is robustness against future skipped-validator code paths. |
| 5 | Migration | Why are there only ~5 source `.diaapp` files when `find` shows >100? | The vast majority of `.diaapp` matches are under `.claude/worktrees/` (parallel agent worktree copies of the repo) and `bin/` (build artifacts copied during the asset pipeline). Source-of-truth files live under `Cluiche/Assets/`, `Cluiche/CluicheEditor/Data/`, and `Cluiche/CluicheTest/ApplicationFlow/`. Worktree copies migrate themselves when they next sync; `bin/` regenerates on the next `dia pipeline`. Only the source files need hand-editing. |
| 6 | Test fixtures | The validator test suite currently builds in-memory manifests with the v2 shape. Do all those tests need rewriting? | Yes — the struct rename to V3 + new fields means every test that constructs a `StageDeclaration` (or `ApplicationManifestV2`) needs the type updated and the new fields populated. Most are mechanical (`autoAdvance = false; transitions = {};`). Existing test names and assertions stay intact. The new validator rules get fresh test cases. |
| 7 | Stage reachability rule | Could `STAGE_UNREACHABLE` be too noisy for in-progress manifests where the developer hasn't wired up transitions yet? | It's a Warning, not an Error — non-blocking. The editor's validation bar surfaces it but doesn't prevent save. For an unfinished manifest (initial_stage + 1 stage with no transitions) it correctly flags every other stage as unreachable, which is the intended signal: "you've added stages but no way to get to them yet." If a developer finds it noisy, suppression UX is a separate feature; we don't pre-design for it. |
| 8 | Self-loop rule severity | Why is `TRANSITION_SELF_LOOP` a Warning and not an Error? | A self-transition is a legitimate "restart this stage" pattern (re-enter the stage, re-run DoStop/DoStart on non-retained modules — this is how SD-012's hot-reload works internally). It's flagged as a Warning so unintended cycles are visible, but allowed because the runtime mechanic exists and may be desirable. If a future consensus emerges that self-loops are always wrong, we promote it to Error. |
| 9 | Renamings | The proposed rename `ApplicationManifestV2 → V3` (file, struct, loader, validator) touches a lot of files. Is the churn worth it vs. keeping the V2 names? | Keeping v2 names while the schema is v3 would create a permanent reader trap — "why is `ApplicationManifestV2` accepting `version: 3`?". The rename is a one-time cost; the names then truthfully match the schema. SD-017's clean-break ethos applies. |

## Status

`Approved` — 2026-05-20

Plan: [stage-transitions.plan.md](stage-transitions.plan.md)
