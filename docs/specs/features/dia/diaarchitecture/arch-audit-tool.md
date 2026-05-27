# Feature Spec: arch-audit-tool (C1)

## Traceability

| Level | Parent | This Feature |
|-------|--------|--------------|
| Platform | @docs/specs/platform/Cluiche.md | — |
| Application | @docs/specs/applications/dia.md | — |
| System | @docs/specs/systems/dia/diaarchitecture.md | **arch-audit-tool** |

**Status:** `Approved`
**Research:** docs/research/migrat_cmake/summary.md

---

## Summary

Add `dia check --tool=arch` to DiaCLI. The command reads every `dia.*.architecture.module.md` YAML file, collects `dependencies.forbidden` and the `layer:` field (added by C7), then parses `#include` directives across all `.cpp` and `.h` files in each module directory. It reports two classes of violation:

1. **Forbidden deps** — a module includes a header from a module listed in its `dependencies.forbidden`
2. **Layer ordering violations** — a module at layer N includes a header from a module at layer N+k (upward reach within Core sub-layers, or a domain core reaching into another domain)

This is a prerequisite gate for C3: `dia check --tool=arch` must exit 0 before the full CMake migration begins.

---

## Acceptance Criteria

| ID | Criterion | Verification |
|----|-----------|-------------|
| AC1 | `dia check --tool=arch` runs from repo root and exits 0 on a clean codebase | Run against a known-clean module |
| AC2 | `dia check --tool=arch` exits 1 and prints a violation report when a forbidden include is present | Unit test: introduce a deliberate violation, confirm exit code + output |
| AC3 | Layer ordering violations are reported (module at `core/platform` including from `core/tooling`) | Unit test with synthetic module docs |
| AC4 | `External/` includes, system headers, and intra-module includes are excluded from violation detection | No false positives on DiaCore or DiaMaths |
| AC5 | Output is written to `Cluiche/out/check/arch-violations.txt` in addition to console | File exists after a violating run |
| AC6 | `--module <module-id>` flag scopes the scan to one module only | `dia check --tool=arch --module dia.core` only scans DiaCore |
| AC7 | `dia check --tool=arch --summary` prints violation count only (no file paths) — suitable for CI badge | Output matches `N violations found` pattern |
| AC8 | Tool handles missing `layer:` field gracefully (warn, do not crash) | Run against a module without `layer:` |

---

## Design

### Include resolution strategy

The tool resolves `#include <Dia/SomeModule/Foo.h>` to a module by matching the first path component against the known module directory map built from YAML. It does **not** invoke the compiler — pure text parse.

Exclusion rules (never flagged):
- Path starts with `External/`
- Path starts with a Windows SDK prefix (`windows.h`, `d3d11.h`, etc.)
- Path is within the same module directory (intra-module)
- Path is a standard library header (`<vector>`, `<string>`, etc.)

### Layer ordering table

Built at runtime from YAML `layer:` fields. Ordering:

```
core/foundation < core/platform < core/application
               < core/entity
               < core/assets
               < core/tooling
domain/*/core  depends on core/* only
domain/*/tools depends on core/*, domain/*/core
```

A violation is any include where the *included module's* layer is higher in the ordering than permitted for the *including module's* layer.

### Output format

```
ARCH VIOLATION: dia.core depends on dia.editor
  File: Dia/DiaCore/Foo.cpp:42
  Include: <DiaEditor/Bar.h>
  Rule: core/foundation may not depend on core/tooling

ARCH VIOLATION: dia.rigidbody2d depends on dia.graphics (forbidden)
  File: Dia/DiaRigidBody2D/PhysicsWorld.cpp:17
  Include: <DiaGraphics/ICanvas.h>
  Rule: listed in dependencies.forbidden

Total: 2 violations
```

---

## Tasks

| # | Task | Notes |
|---|------|-------|
| 1 | Add `arch` subcommand to `dia check` in DiaCLI | Registers alongside existing `--tool=sanitizer` and `--tool=cppcheck` |
| 2 | Implement YAML module map builder — reads all `dia.*.architecture.module.md`, extracts `module_id`, `path`, `layer:`, `dependencies.forbidden` | Python, uses `PyYAML` (already in DiaCLI venv) |
| 3 | Implement include parser — walks `.cpp`/`.h` files, extracts `#include <...>` directives, resolves to module IDs | Pure text, no compiler invocation |
| 4 | Implement forbidden-dep checker — cross-references includes against `dependencies.forbidden` | |
| 5 | Implement layer ordering checker — cross-references includes against layer ordering table | Requires C7 `layer:` fields to be present; gracefully warns if missing |
| 6 | Write output to `Cluiche/out/check/arch-violations.txt` + console | |
| 7 | Add `--module` flag for single-module scoping | |
| 8 | Add `--summary` flag for CI badge output | |
| 9 | Add `dia check --tool=arch` to CI pipeline (after C7 is merged) | `dia pipeline --stage static-analysis` invokes it |

---

## Binding Decisions Compliance

| Decision | How Complied |
|----------|-------------|
| PD-006 — VS project files source of truth | Tool reads YAML and source files only — no `.vcxproj` dependency |
| AD-001 — Module YAML frontmatter | Tool consumes existing YAML schema extended by C7 |

---

## AI Review Questions

| # | Question | Answer |
|---|----------|--------|
| 1 | What if a module has no `layer:` yet (C7 incomplete)? | Tool warns per-module and skips layer ordering check for that module; forbidden-dep check still runs |
| 2 | Should the tool report warnings vs errors differently? | Forbidden-dep violations = errors (exit 1). Missing `layer:` = warnings (exit 0). Layer ordering violations = errors (exit 1) once C7 is complete. |
| 3 | How does the tool handle conditional includes (`#ifdef _WIN32 #include ...`)? | Parses all `#include` directives regardless of surrounding preprocessor conditions — simpler and conservative (no false negatives). |
