# Plan: DiaPipelineEditor

**Spec:** @docs/specs/systems/dia/diapipelineeditor.md  
**Status:** Done  
**Started:** 2026-04-27  
**Last Updated:** 2026-04-27 (sub-step-visibility added)

## Implementation Order

Features are implemented in dependency order per the system spec:

| # | Feature | Spec | Plan | Status |
|---|---------|------|------|--------|
| 1 | ndjson-tailer | @docs/specs/features/dia/diapipelineeditor/ndjson-tailer.md | @docs/specs/features/dia/diapipelineeditor/ndjson-tailer.plan.md | Done |
| 2 | pipeline-panel-ui | @docs/specs/features/dia/diapipelineeditor/pipeline-panel-ui.md | @docs/specs/features/dia/diapipelineeditor/pipeline-panel-ui.plan.md | Done |
| 3 | build-trigger | @docs/specs/features/dia/diapipelineeditor/build-trigger.md | @docs/specs/features/dia/diapipelineeditor/build-trigger.plan.md | Done |
| 4 | run-history | @docs/specs/features/dia/diapipelineeditor/run-history.md | @docs/specs/features/dia/diapipelineeditor/run-history.plan.md | Done |
| 5 | sub-step-visibility | @docs/specs/features/dia/diapipelineeditor/sub-step-visibility.md | — | Done |

## Shared Setup (prerequisite for feature 1)

The DiaPipelineEditor module does not exist yet. Before the first feature can start, the project scaffolding must be created:

- `Dia/DiaPipelineEditor/DiaPipelineEditor.vcxproj` + `.vcxproj.filters` (static library, references DiaCore, DiaLogger, DiaEditor)
- Add project to `Cluiche/Cluiche.sln`
- Add `dia.diapipelineeditor` to `Dia/dia.root.architecture.module.md` dependent_modules
- Plugin shell: `PipelineEditorPlugin.h/.cpp` implementing `IEditorPlugin` with `REGISTER_EDITOR_PLUGIN` macro

Reference pattern: `Dia/DiaApplicationEditor/` (same project type, same plugin structure).

## Session Notes

### 2026-04-27
- Created system plan and ndjson-tailer feature plan
- Explored codebase for implementation patterns (Observer, DiaLogger, Json, DynamicArrayC, StringCRC, FilePath, IEditorPlugin, WebUIBridge)
- Key finding: FilePath uses alias-based paths — tailer will use Win32 file I/O directly for polling (GetFileAttributesEx for size, fopen/fseek for reads)
- Key finding: Logger macro is DIA_LOG_WARNING (not DIA_LOG_WARN)
