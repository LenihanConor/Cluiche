# Plan: build-trigger

**Spec:** @docs/specs/features/dia/diapipelineeditor/build-trigger.md  
**Status:** Done  
**Started:** 2026-04-27  
**Last Updated:** 2026-04-27

## Implementation Patterns

### PipelineBuildManager (C++)

- `Initialize(PipelineLogTailer*, const char* repoRoot)` — stores tailer pointer and repo root path
- `Start(config, target, stages, force)` — builds command line, calls `CreateProcessA`, stores `PROCESS_INFORMATION`
- `Cancel()` — `TerminateProcess(mProcessHandle, 1)`, `CloseHandle`
- `Update()` — polls `GetExitCodeProcess`; when process exits, logs non-zero exit code via `DIA_LOG_WARNING`
- `Shutdown()` — if process running, cancel it, close handles
- Command line: `<repoRoot>/Dia/DiaCLI/.venv/Scripts/python.exe -m dia_cli pipeline --config <cfg> --target <tgt> [--stage <s>] [--force]`
- Working directory: `<repoRoot>` (repo root, since pipeline.toml is at repo root)
- `IsBuildRunning()` / `GetLastExitCode()` for state queries

### pipeline.toml parsing for target list

- `pipeline-get-targets` reads `<repoRoot>/pipeline.toml` using jsoncpp... no, TOML isn't JSON.
- Simpler approach: read `pipeline.toml` with basic line parsing — scan for `[targets.X]` headers and extract target names. No full TOML parser needed; just string matching.
- Returns JSON array of target names: `["googletest", "cluichetest", "cluicheeditor"]`

### DiaAPI commands (via WebUIBridge request handlers)

Registered in `PipelineEditorPlugin::OnLoad`:
- `pipeline.start` — RequestHandler: extracts config/target/stages/force from JSON, calls `PipelineBuildManager::Start`, returns `{"ok": true/false, "error": "..."}`
- `pipeline.cancel` — RequestHandler: calls `PipelineBuildManager::Cancel`, returns `{"ok": true}`
- `pipeline.get-targets` — RequestHandler: reads pipeline.toml, returns `{"targets": ["googletest", ...]}`

### PipelineToolbar (React)

- Target dropdown, config dropdown (Debug/Release), force checkbox, Build/Cancel button
- On mount: sends `pipeline.get-targets` request to populate target dropdown
- Build click: sends `pipeline.start` request with selected config/target/force
- Cancel click: sends `pipeline.cancel` request
- Button state: shows "Cancel" when `buildRunning`, "Build" otherwise
- JS → C++ via `sendToPlugin(type, data)` through `window.parent.postMessage`

### JS → C++ bridge pattern (request-response)

The existing `sendToPlugin` is fire-and-forget (EventHandler). For request-response, JS needs to:
1. Generate a `reqId`
2. Send via `postMessage({__diaFromFrame: true, payload: {type, data, reqId}})`
3. Listen for `DiaEditor_onResponse({reqId, result})` callback

This matches the WebUIBridge RequestHandler pattern. The `useBridgeRequest` hook will handle this.

## Tasks

| # | Task | Status | Notes |
|---|------|--------|-------|
| 1 | Create PipelineBuildManager.h/.cpp | Done | Win32 CreateProcess, TerminateProcess, GetExitCodeProcess |
| 2 | Add pipeline.toml target parser | Done | Internal/PipelineTargetParser.h — header-only line scanner |
| 3 | Wire DiaAPI commands in PipelineEditorPlugin | Done | 3 request handlers: pipeline.start, pipeline.cancel, pipeline.get-targets |
| 4 | Create PipelineToolbar.tsx React component | Done | Target/config dropdowns, force checkbox, Build/Cancel button |
| 5 | Create useBridgeRequest hook for request-response | Done | reqId generation, Promise-based, 10s timeout |
| 6 | Update PipelinePanel to include toolbar | Done | Toolbar always visible above content area |
| 7 | Update vcxproj files | Done | PipelineBuildManager + PipelineTargetParser added |
| 8 | Build + test | Done | C++ 27/27 pass, JS 62/62 pass, vite build OK |

## Session Notes

### 2026-04-27
- Created plan from approved spec
- Commands use WebUIBridge request handlers (no separate CommandDispatcher)
- pipeline.toml at repo root has targets: googletest, cluichetest, cluicheeditor
- DiaCLI venv at Dia/DiaCLI/.venv/Scripts/python.exe
- Working dir for subprocess should be repo root (pipeline.toml lives there)
