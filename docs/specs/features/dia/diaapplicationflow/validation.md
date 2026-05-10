# Feature Spec: Validation

## Parent System
@docs/specs/systems/dia/diaapplicationflow.md

## Traceability

| Level | Spec | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaApplicationFlow | @docs/specs/systems/dia/diaapplicationflow.md |
| Feature | Validation | (this file) |

## Problem Statement

Config-as-source-of-truth requires that invalid manifests are caught immediately at load time — never at runtime. A validation pass on the parsed ApplicationManifest must detect all structural errors (missing dependencies, cycles, orphaned streams, unknown types) before any modules are created.

## Acceptance Criteria

1. `ManifestValidator` runs on a parsed ApplicationManifest and returns a list of errors/warnings
2. Errors block Application::Start() — application does not run with invalid manifest
3. Warnings are logged but do not block startup
4. Validation checks:
   - All `initial_stage` and `auto_stages` entries exist in `stages` array
   - All module `stages` entries exist in `stages` array (or are `"all"`)
   - All module `dependencies` reference instance_ids that exist in the same PU
   - Every declared dependency appears at an EARLIER index in the `modules` array than the dependent module (array order is startup order — see `config-format.md`)
   - No circular dependencies within a PU
   - All module `type_id` values exist in the TypeRegistry
   - All `reads`/`writes` stream IDs exist in the `streams` array
   - Stream `from`/`to` PU instance_ids exist in `processing_units`
   - No duplicate instance_ids within a PU (modules)
   - No duplicate instance_ids across PUs (PU names)
   - No duplicate stream IDs
   - Single writer per stream unless `multi_writer: true`
   - Every non-"all" module belongs to at least one declared stage
5. Warning checks:
   - Orphaned streams (declared but no readers or no writers)
   - Modules with no dependencies (not necessarily wrong, but worth flagging)
   - Stages with zero stage-specific modules (only "all" modules active)
6. Validation result is a structured list usable by editor validation bar
7. Each error/warning includes: severity, code, message, and reference (which entity is invalid)

## Public API

```cpp
namespace Dia::ApplicationFlow {

    enum class ValidationSeverity { kError, kWarning };

    struct ValidationEntry {
        ValidationSeverity severity;
        StringCRC code;          // e.g. "CYCLE_DETECTED", "UNKNOWN_TYPE"
        const char* message;
        StringCRC entityId;      // which module/stream/PU/stage is the problem
    };

    class ManifestValidator {
    public:
        ManifestValidator(const TypeRegistry& registry);

        void Validate(const ApplicationManifest& manifest);

        bool HasErrors() const;
        bool HasWarnings() const;
        const DynamicArrayC<ValidationEntry, 64>& GetResults() const;
    };
}
```

## Validation Codes

| Code | Severity | Description |
|------|----------|-------------|
| UNKNOWN_STAGE | Error | initial_stage, auto_stages, or module stage not in stages array |
| UNKNOWN_DEPENDENCY | Error | Module dependency references non-existent instance_id in same PU |
| DEPENDENCY_ORDER | Error | Module depends on an instance_id that appears LATER in the `modules` array (array order is startup order) |
| CYCLE_DETECTED | Error | Circular dependency chain within a PU |
| UNKNOWN_TYPE | Error | Module type_id not found in TypeRegistry |
| UNKNOWN_STREAM | Error | reads/writes references non-existent stream ID |
| UNKNOWN_PU | Error | Stream from/to references non-existent PU |
| DUPLICATE_MODULE_ID | Error | Two modules in same PU share instance_id |
| DUPLICATE_PU_ID | Error | Two PUs share instance_id |
| DUPLICATE_STREAM_ID | Error | Two streams share ID |
| MULTI_WRITER_VIOLATION | Error | Multiple writers on single-writer stream |
| ORPHAN_MODULE | Error | Non-"all" module not in any declared stage |
| ORPHAN_STREAM | Warning | Stream has no readers or no writers |
| NO_DEPENDENCIES | Warning | Module declares zero dependencies |
| EMPTY_STAGE | Warning | Stage has only "all" modules, no stage-specific |

## Cycle Detection Algorithm

Topological sort (Kahn's algorithm) per PU:
1. Build dependency graph for modules within the PU
2. Compute in-degree for each module
3. Process zero-in-degree modules iteratively
4. If any modules remain unprocessed → cycle exists
5. Report all modules in the cycle

## Files Touched

| File | Action |
|------|--------|
| `Dia/DiaApplicationFlow/Manifest/ManifestValidator.h` | Rewrite — v2 validation |
| `Dia/DiaApplicationFlow/Manifest/ManifestValidator.cpp` | Rewrite |
| `Dia/DiaApplicationFlow/DiaApplicationFlow.vcxproj` | Update |
| `Dia/DiaApplicationFlow/DiaApplicationFlow.vcxproj.filters` | Update |

## Dependencies

- **Config Format v2** (this system) — consumes ApplicationManifest
- **Registration** (this system) — queries TypeRegistry for type existence
- **DiaCore/CRC/StringCRC** — validation codes and entity IDs
- **DiaCore/Containers** — DynamicArrayC for results, HashTableC for graph building
- **DiaLogger** — log errors/warnings during validation

## Binding Decisions Compliance

| ID | Source | Decision | Compliance |
|----|--------|----------|------------|
| PD-001 | Platform | StringCRC for all IDs | Compliant — validation codes and entity references are StringCRC |
| PD-004 | Platform | No STL in public APIs | Compliant — DynamicArrayC for results |
| PD-007 | Platform | C++20 | Compliant |
| AD-003 | Dia App | Namespace Dia::\<Module\>:: | Compliant — Dia::ApplicationFlow:: |
| SD-001 | DiaAppFlow | Config is sole source of truth | Compliant — validates the config that IS the source of truth |
| SD-014 | DiaAppFlow | Full validation at load, fail-fast | Compliant — this feature implements that decision |
| SD-017 | DiaAppFlow | Clean break | Compliant — v1 validator rewritten |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Scope | Should validation check stream type consistency (writer type matches reader type)? | Not at manifest level — type is a string in config. Type matching happens at compile time when StreamWriter<T>/StreamReader<T> are instantiated. Manifest validation checks structural correctness only. |
| 2 | Performance | Is validation fast enough to run on every editor edit (debounced 500ms per ED-004)? | Yes. Topological sort is O(V+E) — with <100 modules per PU, this is sub-millisecond. |
| 3 | Editor | Should ManifestValidator be usable from the editor (DiaApplicationFlowEditor)? | Yes. It takes ApplicationManifest and TypeRegistry — both available in editor context. Same validation code, same errors surfaced. |
| 4 | Ordering | Must validation run before or after manifest merging? | After. Validation operates on the final merged manifest. Individual stage .diaapp files are not validated independently. |

## Open Questions

None.

## Status

`Approved` — 2026-05-09
