# Handoff: DiaApplicationEditor — v1 → v2 Manifest Migration

**Written by:** Claude (stream-bundle implementation session, 2026-05-18)
**For:** The Claude instance redesigning DiaApplicationEditor  
**Status:** DiaStream F1 is complete; this migration is the prerequisite for final v1 cleanup

---

## Read this before designing anything

This document is your full context. Do not ask the user to re-explain the stream bundle, the v1/v2 split, or the engine architecture — it is all here.

---

## What happened and why this migration exists

The `DiaApplicationFlow` module has been refactored (F1 of the DiaStream bundle). The old Phase-based architecture is gone. The new system uses Stages, Streams declared in the manifest, and a redesigned `ApplicationManifestV2`. The old v1 manifest classes (`ApplicationManifest`, `ApplicationManifestLoader`, etc.) were **excluded from the `DiaApplicationFlow` library build** because they depend on deleted v1 classes (`ApplicationProcessingUnit`, `ApplicationPhase`, `ApplicationModule`).

The files still exist on disk but will be permanently deleted once `DiaApplicationEditor` and `DiaGame` no longer need them. Until then, the `DiaApplicationFlow.lib` does not export v1 manifest symbols, so `DiaApplicationEditor` and `DiaGame` will fail to link if built.

One test file was excluded from the build as a result:
- `Cluiche/Tests/GoogleTests/DiaApplicationEditor/TestManifestSerializer.cpp` — marked with `TODO: re-enable after DiaApplicationEditor migrates to v2 manifest`

---

## V1 files on disk (to be deleted after this migration)

All in `Dia/DiaApplicationFlow/`:

| File | What it does |
|------|-------------|
| `Manifest/ApplicationManifest.h/.cpp` | V1 IR: `ProcessingUnitEntry` with `phases`, `transitions`, `initialPhase`. Phase-based, not Stage-based. |
| `Manifest/ApplicationManifestLoader.h/.cpp` | Loads v1 `.diaapp` JSON → `ApplicationManifest`. Deep dependency on `ApplicationProcessingUnit`, `ApplicationPhase`. |
| `Manifest/ManifestComposer.h/.cpp` | Merges multiple v1 `.diaapp` fragments into one `ApplicationManifest`. |
| `Manifest/ManifestValidator.h/.cpp` | Validates a v1 `ApplicationManifest`. |
| `Manifest/JsonApplicationManifestSerializer.h/.cpp` | Serializes `ApplicationManifest` to/from JSON. Used for save in the editor. |
| `Manifest/TypedImport.h` | V1 typed import mechanism for manifest composition. |
| `TypeRegistry/ApplicationTypeRegistry.h/.cpp` | V1 factory registry for `ProcessingUnit`, `Phase`, `Module` types. |
| `TypeRegistry/RegistrationMacros.h` | V1 registration macros. |

---

## V2 replacements (already built and shipping)

All in `Dia/DiaApplicationFlow/`:

| V1 | V2 equivalent |
|----|--------------|
| `ApplicationManifest` | `ApplicationManifestV2` in `Manifest/ApplicationManifestV2.h` |
| `ApplicationManifestLoader` | `ApplicationManifestLoaderV2` in `Manifest/ApplicationManifestLoaderV2.h/.cpp` |
| `ManifestComposer` | `ManifestComposerV2` in `Manifest/ManifestComposerV2.h/.cpp` |
| `ManifestValidator` | `ManifestValidatorV2` in `Manifest/ManifestValidatorV2.h/.cpp` |
| `JsonApplicationManifestSerializer` | No direct v2 equivalent yet — the v2 manifest is saved by writing JSON directly; you may need to create one |
| `ApplicationTypeRegistry` | `TypeRegistry` in `TypeRegistry.h/.cpp` (v2, already exists) |
| Phase concept | **Gone.** Replaced by `stages` + `modules` with a `stages: ["StageName"]` membership list. No `transitions` array. |

---

## The key structural difference: v1 vs v2 manifest IR

**V1 `ApplicationManifest` shape:**
```
ProcessingUnitEntry
  typeId, instanceId, frequencyHz, dedicatedThread, root, initialPhase, config
  phases[]         ← PhaseEntry { typeId, instanceId, config }
  transitions[]    ← PhaseTransition { fromPhase, toPhase }
  modules[]        ← ModuleEntry { typeId, instanceId, phaseIds[], dependencies[], config }
imports[]          ← TypedImport (for manifest composition)
metadata           ← editor-only JSON blob
```

**V2 `ApplicationManifestV2` shape:**
```
stages[]           ← StageDeclaration { name, manifestPath }
initialStage       ← StringCRC
autoStages[]       ← StringCRC[]
streams[]          ← StreamDeclaration { id, kind, payloadType, fromPU, toPU, multiWriter, capacity, maxReaders }
processingUnits[]  ← ProcessingUnitDeclaration
  instanceId, frequencyHz, dedicatedThread
  modules[]        ← ModuleDeclaration
    instanceId, typeId
    stages[]       ← StringCRC[] (which stages this module is active in; "all" = always)
    dependencies[] ← StringCRC[]
    reads[]        ← StringCRC[] (stream IDs this module reads)
    writes[]       ← StringCRC[] (stream IDs this module writes)
    configJson     ← String256 (raw JSON blob)
    startTimeoutMs, stopTimeoutMs
```

The most important conceptual change: **there are no phases, no transitions, no initialPhase**. Stage membership is declared per-module with `stages: ["Boot", "DummyStage"]` or `stages: ["all"]`. The framework diffs module membership between stages and starts/stops accordingly.

---

## What DiaApplicationEditor currently does (1735 lines, `DiaApplicationEditor.cpp`)

The editor:
1. **Loads** a `.diagame` file → calls `ApplicationManifestLoader` to load referenced `.diaapp` fragments → composes into one `ApplicationManifest`.
2. **Validates** the loaded manifest via `ManifestValidator`.
3. **Displays** the manifest as a graph (processing units → phases → modules) in a WebUI.
4. **Edits** the manifest — adds/removes PUs, phases, modules, transitions; rewires module-to-phase membership.
5. **Saves** edits back to the source `.diaapp` file via `JsonApplicationManifestSerializer`, preserving import-origin metadata (`sourceManifestPath` per entry).
6. **Reloads** on file change.

The key complexity: the editor tracks **which source file each entry came from** (`sourceManifestPath` on every entry). When saving, it writes only the entries that belong to the file being saved, leaving others untouched.

---

## What the v2 migration needs to achieve

1. **Load v2 `.diaapp` manifests** — use `ApplicationManifestLoaderV2` + `ManifestComposerV2`.
2. **Display the v2 structure** — stages instead of phases; modules with stage membership instead of phase membership; streams as a first-class entity (the editor should be able to show/edit the `streams[]` array).
3. **Edit v2 manifest** — add/remove PUs, modules, stages; edit module stage membership; add/edit streams with `kind`, `payload_type`, `from`, `to`.
4. **Validate** via `ManifestValidatorV2` — show validation errors to the user.
5. **Save** edits back to source `.diaapp` files — the `sourceManifestPath` tracking still applies (the user may compose from multiple `.diaapp` files).
6. **Re-enable `TestManifestSerializer.cpp`** — against new v2 types and a v2 serializer.

---

## What you need to decide (and write to `decisions.md`)

The user is redesigning the editor. These are the decisions that affect what gets cleaned up vs. built new:

1. **Does the editor get a v2 JSON serializer, or does it write JSON directly?** The v1 had `JsonApplicationManifestSerializer` as a class. V2 could just use `json::Value` construction directly in the editor, or you could build a `JsonApplicationManifestSerializerV2`. The choice affects whether F1 cleanup can simply delete the v1 serializer or needs a new file.

2. **Does the editor stay Phase-aware for v1 backwards compatibility, or is it a hard cut?** The user said "clean break" for the engine (SD-017). If the editor also cuts v1 support, old v1 `.diaapp` files simply won't load — the editor only understands v2. If it needs to open old files, a migration converter would be needed.

3. **What is the `sourceManifestPath` equivalent in v2?** V1 tracked which `.diaapp` file each entry came from. In v2, the `ManifestComposerV2` also composes from multiple files (per `StageDeclaration.manifestPath`). Does the editor need to track per-entry origin in v2, or does it edit one canonical `.diaapp` per application?

4. **Does `DiaGame::GameFileComposer` also get migrated in this refactor, or separately?** It only uses `ManifestComposer` and `ApplicationManifest` for composition — a small surface. Easier if bundled here.

---

## How to write back to the implementation session

Once you have your design decisions, write them to `docs/refactors/editor_manifest_v2/decisions.md` using this format:

```markdown
# DiaApplicationEditor v2 Migration — Decisions

## D1 — [Topic]
[Decision + rationale]

## D2 — ...
```

Then write your implementation plan to `docs/refactors/editor_manifest_v2/plan.md`.

The Claude continuing the DiaStream bundle implementation will read `decisions.md` before resuming. If any of your decisions affect what gets deleted from `Dia/DiaApplicationFlow/Manifest/` (e.g., if you're building a new `JsonApplicationManifestSerializerV2`), note that explicitly so the cleanup task can be updated.

---

## Binding decisions that apply to your work

From the platform/app spec chain — all binding:

| ID | Decision |
|----|----------|
| PD-001 | StringCRC for all IDs |
| PD-004 | No STL containers in public APIs |
| PD-007 | C++20 |
| AD-003 | Namespace `Dia::<Module>::` |
| SD-001 | Config is sole source of truth for structural wiring |
| SD-002 | Phases removed, replaced by Stages |
| SD-006 | Streams framework-owned, declared in config |
| SD-017 | Clean break — no v1 backward compatibility |
| SD-018 | Reserved `$`-prefix streams are framework-auto-created; user `$` prefix forbidden |

The specs live at:
- Platform: `docs/specs/platform/Cluiche.md`
- Application (Dia): `docs/specs/applications/dia.md`
- System: `docs/specs/systems/dia/diaapplicationflow.md`

---

## Definition of done for this migration

- `DiaApplicationEditor` builds and links against `DiaApplicationFlow.lib` (v2 only).
- `DiaGame::GameFileComposer` builds (either migrated or decoupled).
- `TestManifestSerializer.cpp` re-enabled in `GoogleTests.vcxproj` with tests against v2 types.
- The following files deleted from disk:
  - `Dia/DiaApplicationFlow/Manifest/ApplicationManifest.h/.cpp`
  - `Dia/DiaApplicationFlow/Manifest/ApplicationManifestLoader.h/.cpp`
  - `Dia/DiaApplicationFlow/Manifest/ManifestComposer.h/.cpp`
  - `Dia/DiaApplicationFlow/Manifest/ManifestValidator.h/.cpp`
  - `Dia/DiaApplicationFlow/Manifest/JsonApplicationManifestSerializer.h/.cpp`
  - `Dia/DiaApplicationFlow/Manifest/TypedImport.h`
  - `Dia/DiaApplicationFlow/TypeRegistry/ApplicationTypeRegistry.h/.cpp`
  - `Dia/DiaApplicationFlow/TypeRegistry/RegistrationMacros.h`
- `dia pipeline --target googletest` passes with 0 failures.
