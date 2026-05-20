# Feature Spec: Real-Time Validation

## Parent System
@docs/specs/systems/dia/diaapplicationfloweditor.md

## Traceability

| Level | Spec | Decision IDs honored |
|-------|------|---------------------|
| Platform | @docs/specs/platform/Cluiche.md | PD-001, PD-007 |
| Application | @docs/specs/applications/dia.md | AD-003 |
| System | @docs/specs/systems/dia/diaapplicationfloweditor.md | ED-004, ED-007, ED-013 |
| System (upstream) | @docs/specs/systems/dia/diaapplicationflow.md | SD-014 |

## Purpose

Continuously validate the in-memory ManifestDocument against the same rules the runtime uses (SD-014). Errors and warnings are surfaced immediately (debounced 500ms) in a ValidationBar at the footer of the editor — no manual validate button (ED-013). Errors block save; warnings allow save with confirmation.

## Acceptance Criteria

1. **Always-on** — Validation runs automatically after every edit, debounced 500ms (ED-013). No manual trigger.
2. **Same rules as runtime** — Validates the same constraints DiaApplicationFlow checks at load time (SD-014).
3. **Error/warning classification** — Issues classified as Error (blocks save, blocks runtime load) or Warning (informational, allows save with confirmation).
4. **ValidationBar** — Footer bar showing: error count (red), warning count (amber). Click expands to full issue list.
5. **Issue list** — Each issue shows: severity icon, rule ID, human-readable message, location (which PU/module/stream is involved).
6. **Click-to-navigate** — Clicking an issue navigates to the relevant element (selects PU/module/stream in appropriate tab/inspector).
7. **Validation rules** — At minimum:
   - Dependency cycle detection (Error)
   - Orphaned modules (module in no stage) (Warning)
   - Missing/undeclared stream refs in reads/writes — `UNKNOWN_STREAM_IN_READS`, `UNKNOWN_STREAM_IN_WRITES` (Error)
   - Orphan reader/writer streams — `ORPHAN_READER_STREAM`, `ORPHAN_WRITER_STREAM` (Warning)
   - Missing payload type — `PAYLOAD_TYPE_MISSING` (Error)
   - Stage coverage gaps (module references non-existent stage) (Error)
   - Duplicate instance IDs within a PU (Error)
   - `initial_stage` not in stages array (Error)
   - Stream from/to referencing non-existent PU (Error)
8. **Debounce** — Multiple rapid edits within 500ms trigger only one validation pass.

## Design

### Validation Engine

```cpp
namespace Dia::ApplicationFlow::Editor {
    enum class IssueSeverity { Error, Warning };

    struct ValidationIssue {
        IssueSeverity severity;
        const char* ruleId;         // e.g., "DEPENDENCY_CYCLE", "ORPHAN_MODULE"
        const char* message;        // human-readable
        const char* location;       // e.g., "SimPU/DummyLevel" or "stream:InputToSim"
    };

    struct ValidationResult {
        Dia::Core::Containers::DynamicArrayC<ValidationIssue, 64> issues;
        unsigned int errorCount;
        unsigned int warningCount;
        bool hasErrors() const;
    };

    class ManifestValidator {
    public:
        ValidationResult Validate(const ManifestDocument& doc);
    };
}
```

### Validation Rules

| Rule ID | Severity | Check |
|---------|----------|-------|
| `DEPENDENCY_CYCLE` | Error | Topological sort of module deps within each PU — cycle detected |
| `ORPHAN_MODULE` | Warning | Module whose stages list doesn't intersect any declared stage |
| `UNKNOWN_STREAM_IN_READS` | Error | Module reads a stream ID not in manifest.streams |
| `UNKNOWN_STREAM_IN_WRITES` | Error | Module writes a stream ID not in manifest.streams |
| `ORPHAN_READER_STREAM` | Warning | Stream declared but no module reads it |
| `ORPHAN_WRITER_STREAM` | Warning | Stream declared but no module writes it |
| `PAYLOAD_TYPE_MISSING` | Error | Stream has empty payload type |
| `STAGE_REF_INVALID` | Error | Module references a stage not in manifest.stages array |
| `DUPLICATE_INSTANCE_ID` | Error | Two modules in same PU share instance_id |
| `INITIAL_STAGE_INVALID` | Error | initial_stage not in stages array |
| `STREAM_PU_INVALID` | Error | Stream from/to references non-existent PU |
| `STREAM_SELF_LOOP` | Error | Stream from and to are the same PU |

### Debounce Integration

The frontend maintains a debounce timer. On any model change event:
1. Reset 500ms timer
2. When timer fires: call `editor.validation.run()`
3. Backend runs `ManifestValidator::Validate()` on current model
4. Result sent to frontend: `onValidationComplete({ issues, errorCount, warningCount })`
5. ValidationBar updates

### ValidationBar Component

```
ValidationBar (footer, always visible)
├── ErrorCount (red badge + number, or hidden if 0)
├── WarningCount (amber badge + number, or hidden if 0)
├── "All clear" text (when 0 issues)
└── (expanded) IssueList
    └── IssueRow[] (clickable)
        ├── SeverityIcon (red X or amber !)
        ├── RuleId (monospace)
        ├── Message
        └── Location (clickable → navigate)
```

### Save Integration

Save flow (from Manifest Load/Save feature) calls validation before proceeding:
- If `errorCount > 0` → save blocked, ValidationBar highlights
- If only warnings → confirmation dialog: "N warnings. Save anyway?"
- If clean → save proceeds

## Tasks

| # | Task | Test | Status | Notes |
|---|------|------|--------|-------|
| 1 | `ManifestValidator` class with all rule implementations | Unit test: each rule with pass/fail cases | Todo | Core logic |
| 2 | Debounce integration on model change | Integration test: rapid edits → single validation | Todo | |
| 3 | CEF message handler for validation trigger/result | Integration test: round-trip | Todo | |
| 4 | React `ValidationBar` component | Manual: shows counts, expands to list | Todo | |
| 5 | Click-to-navigate from issue to element | Manual: click navigates to correct inspector | Todo | |
| 6 | Save-blocking integration | Unit test: save rejected when errors exist | Todo | Integrates with Manifest Load/Save |

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| PD-001 | StringCRC for all IDs | Validation uses CRC matching for ID lookups. |
| PD-007 | C++20 required | Validator code uses C++20. |
| AD-003 | Namespace Dia::\<Module\>:: | In `Dia::ApplicationFlow::Editor::`. |
| ED-004/ED-013 | Validation always-on, debounced 500ms, no button | Runs automatically on every edit. No manual trigger. |
| ED-007 | React + CEF frontend | ValidationBar in React. Validator in C++ backend. |
| SD-014 | Full validation at load | Same rules as runtime validation. Editor surfaces same errors. |

## Open Questions

None.

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Performance | Should validation run on a background thread for large manifests? | No — manifests are small (< 100 modules). Validation is O(N) with cycle detection O(V+E). Sub-millisecond for realistic sizes. Main thread is fine. |
| 2 | Extensibility | Should validation rules be pluggable (register new rules)? | No — fixed rule set matching runtime. If runtime adds rules, we add them here. No plugin architecture needed. |
| 3 | Suppression | Should users be able to suppress specific warnings? | No — warnings are informational and few. Suppression adds complexity without real value for this editor. |
| 4 | Initial load | Should validation run immediately on file load? | Yes — same as SD-014. Load triggers validation. Results visible in ValidationBar from the start. |

## Status

`Approved` — 2026-05-19
