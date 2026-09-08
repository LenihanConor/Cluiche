# Feature Spec: Manifest Load/Save

## Parent System
@docs/specs/applications/dia/systems/diaapplicationfloweditor/diaapplicationfloweditor.md

## Traceability

| Level | Spec | Decision IDs honored |
|-------|------|---------------------|
| Platform | @docs/specs/platform/Cluiche.md | PD-001, PD-004, PD-006, PD-007, PD-010 |
| Application | @docs/specs/applications/dia/dia.md | AD-001, AD-003 |
| System | @docs/specs/applications/dia/systems/diaapplicationfloweditor/diaapplicationfloweditor.md | ED-007, ED-013 |
| System (upstream) | @docs/specs/applications/dia/systems/diaapplicationflow/diaapplicationflow.md | SD-001, SD-014, SD-018 |

## Purpose

Provide the core file I/O layer for the DiaApplicationFlowEditor: loading `.diaapp` v2 manifest files into an in-memory model, tracking modifications (dirty state), saving back to disk with backup and validation, and reporting load errors gracefully. This feature is the foundation that all other editor features depend on — nothing can be visualized, edited, or validated without a loaded manifest.

## Acceptance Criteria

1. **Load** — Load a `.diaapp` v2 JSON file and populate the in-memory editor model. All PUs, modules, streams, stages, and their properties are accessible after load.
2. **Dirty tracking** — Track whether the in-memory model has been modified since last save/load. Expose `isDirty` state to the UI for visual indicator (e.g., `*` in title).
3. **Backup on save** — Before overwriting the `.diaapp` file, create a `.diaapp.bak` copy of the previous version in the same directory.
4. **Validation blocks save** — Run the full validation rule set before save. Errors (not warnings) block the save and surface in UI. Warnings allow save with confirmation.
5. **Canonical formatting** — Save JSON in canonical style (consistent indentation, sorted keys) to enable meaningful diffs.
6. **Load error reporting** — Malformed JSON, unknown `version` field, or schema violations are reported to the UI as structured errors (not crashes). The editor remains usable (empty state or previous file retained).

## Design

### In-Memory Model

The editor maintains a `ManifestDocument` that mirrors the `.diaapp` v2 JSON schema:

```cpp
namespace Dia::ApplicationFlow::Editor {
    struct StreamDef {
        Dia::Core::StringCRC id;
        const char* type;       // "FrameStream<T>" / "EventStream<T>"
        Dia::Core::StringCRC fromPU;
        Dia::Core::StringCRC toPU;
        unsigned int capacity;
        unsigned int maxReaders;
    };

    struct ModuleDef {
        Dia::Core::StringCRC instanceId;
        Dia::Core::StringCRC typeId;
        // stages: "all" or list of stage names
        Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 16> stages;
        Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 16> dependencies;
        Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 8> reads;
        Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 8> writes;
        unsigned int startTimeoutMs;
        unsigned int stopTimeoutMs;
    };

    struct ProcessingUnitDef {
        Dia::Core::StringCRC instanceId;
        unsigned int frequencyHz;
        bool dedicatedThread;
        Dia::Core::Containers::DynamicArrayC<ModuleDef, 32> modules;
    };

    struct ManifestDocument {
        unsigned int version;   // must be 2
        Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 8> stages;
        Dia::Core::StringCRC initialStage;
        Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 8> autoStages;
        Dia::Core::Containers::DynamicArrayC<StreamDef, 16> streams;
        Dia::Core::Containers::DynamicArrayC<ProcessingUnitDef, 8> processingUnits;

        bool isDirty;
        const char* filePath;   // absolute path of loaded file
    };
}
```

### Load Flow

1. User selects `.diaapp` file (via file dialog or recent-files)
2. C++ backend reads file to buffer (via `ISerializer::ReadFileToBuffer` pattern)
3. Parse JSON (jsoncpp)
4. Validate `version == 2` — reject otherwise with structured error
5. Deserialize into `ManifestDocument`
6. Run validation rules (same as SD-014 — dependency cycles, orphans, stream refs)
7. If validation passes: populate model, set `isDirty = false`, notify React UI
8. If validation warns: populate model, surface warnings
9. If parse/schema fails: report error to UI, retain previous state (or empty state if first load)

### Save Flow

1. Check `isDirty` — if not dirty, no-op (UI shouldn't enable save button)
2. Run full validation
3. If errors exist → block save, surface errors via ValidationBar
4. If warnings exist → prompt user for confirmation
5. If file exists on disk → copy to `.diaapp.bak`
6. Serialize `ManifestDocument` to JSON with canonical formatting
7. Write to disk atomically (write to `.tmp`, rename over original)
8. Set `isDirty = false`

### Dirty Tracking

Every mutation to `ManifestDocument` sets `isDirty = true`. The Undo/Redo feature (separate spec) will also update dirty state — undo back to save-point clears dirty. For this feature's scope, any edit marks dirty; only successful save clears it.

### Frontend Communication

The React UI communicates with the C++ backend via CEF message passing:
- `editor.manifest.load(path)` → triggers load flow
- `editor.manifest.save()` → triggers save flow
- `editor.manifest.getState()` → returns current model as JSON for rendering
- Events: `onManifestLoaded`, `onManifestError`, `onDirtyChanged`

### File Format (Canonical Output)

```json
{
    "version": 2,
    "stages": ["Boot", "DummyStage"],
    "initial_stage": "Boot",
    "auto_stages": ["Boot"],
    "streams": [
        {
            "id": "InputToSim",
            "type": "EventStream<InputEvent>",
            "from": "MainPU",
            "to": "SimPU"
        }
    ],
    "processing_units": [
        {
            "instance_id": "MainPU",
            "frequency_hz": 30,
            "dedicated_thread": false,
            "modules": [...]
        }
    ]
}
```

Keys are sorted alphabetically within each object. 4-space indentation. No trailing whitespace. Newline at EOF.

## Tasks

| # | Task | Test | Status | Notes |
|---|------|------|--------|-------|
| 1 | Define `ManifestDocument` and related structs in `Dia/DiaApplicationEditor/` | Unit test: construct, default values correct | Todo | Foundation data model |
| 2 | Implement `ManifestLoader::Load(path)` — JSON parse + schema validation + populate model | Unit test: load valid file, load invalid JSON, load wrong version | Todo | Uses jsoncpp |
| 3 | Implement `ManifestSaver::Save(doc, path)` — validate, backup, serialize, atomic write | Unit test: save creates .bak, save blocks on errors, canonical output | Todo | Atomic write via tmp+rename |
| 4 | Implement dirty tracking on ManifestDocument | Unit test: dirty after mutation, clean after save, clean after load | Todo | Simple bool + notifier |
| 5 | Implement CEF message handlers for load/save/getState | Integration test: message round-trip | Todo | React ↔ C++ bridge |
| 6 | React UI: file open dialog, save button (disabled when clean), error display | Manual test via editor | Todo | Minimal UI — other features add views |

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| PD-001 | StringCRC for all IDs | All instance_id, type_id, stage names, stream IDs stored as StringCRC in the in-memory model. JSON stores human-readable strings; CRC computed on load. |
| PD-004 | No STL in public APIs | ManifestDocument uses DynamicArrayC, not std::vector. Internal implementation may use std::string for JSON buffer. |
| PD-006 | VS project files are source of truth | New files added to DiaApplicationEditor.vcxproj manually. |
| PD-007 | C++20 required | Implementation uses C++20. |
| PD-010 | .diagame is root, .diastage declares stage metadata | Editor loads `.diaapp` files (found via .diagame imports). Does not bypass the file hierarchy. |
| AD-001 | YAML frontmatter module docs | dia.applicationeditor.architecture.module.md updated. |
| AD-003 | Namespace Dia::\<Module\>:: | All code in `Dia::ApplicationFlow::Editor::` namespace. |
| ED-007 | React + CEF frontend | UI layer is React; backend communication via CEF message passing. |
| ED-013 | No explicit Validate button — always-on | Validation runs automatically; save flow uses same validation. No manual validate trigger. |
| SD-001 | Config is sole source of truth | The editor edits config — the manifest file. No hidden runtime state affects the saved output. |
| SD-014 | Full validation at load time | Load runs the same validation rules the runtime uses. Errors surfaced immediately. |
| SD-018 | Reserved `$`-prefix streams | `$`-prefix streams shown read-only in model. Save preserves them. No user creation of `$`-prefix streams. |

## Open Questions

None — all resolved via system spec decisions and AI review answers.

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Load | Should the editor support loading `.diastage` files directly, or only `.diaapp`? | `.diaapp` only. `.diastage` is metadata — if a user wants stage modules, they open the `.diaapp` it points to. Project-level file browsing is a separate concern. |
| 2 | Save | Should atomic write use OS-level rename or a custom two-phase approach? | OS rename (MoveFileEx with MOVEFILE_REPLACE_EXISTING). Atomic on NTFS, single syscall, no failure window. |
| 3 | Model | Should `ManifestDocument` store raw JSON alongside the structured model for round-trip fidelity? | No — structured only. Canonical reformat on save (per system AI Review Q8). Unknown fields dropped. Schema growth = struct growth. |
| 4 | Dirty | Should closing a dirty document prompt "save changes?" or discard silently? | Prompt save/discard/cancel. Standard editor UX. |
| 5 | Backup | Should `.bak` files accumulate (`.bak1`, `.bak2`) or only keep the latest? | Only keep latest `.bak`. Version history is git's job. |
| 6 | Load | What happens if the file is locked by another process during load? | Report structured error to UI ("File is locked by another process"), retain previous state. No retry loop — user retries manually. |

## Status

`Approved` — 2026-05-19
