# Plan: ndjson-tailer

**Spec:** @docs/specs/features/dia/diapipelineeditor/ndjson-tailer.md  
**Status:** Not Started  
**Started:** 2026-04-27  
**Last Updated:** 2026-04-27

## Implementation Patterns

### Project scaffolding
- Static library `.vcxproj` following `DiaApplicationEditor.vcxproj` pattern
- Project references: DiaCore, DiaLogger, DiaEditor (for IEditorPlugin base)
- Include directory: `$(ProjectDir)../` (allows `#include <DiaCore/...>`)
- Preprocessor: `WIN32;_DEBUG;NOMINMAX;/Zc:strictStrings-` matching existing projects

### Struct definitions (PipelineEvent.h)
- `PipelineEvent` and `RunSummary` per system spec
- StringCRC fields constructed via `Dia::Core::StringCRC("OnStageStarted")` etc.
- Display text (`error`, `detail`, `level`) are `const char*` into string pool — no per-event allocation

### String pool
- Simple linear allocator: `char mPool[64 * 1024]` with a write cursor
- `const char* AllocString(const char* src, size_t len)` — copies into pool, null-terminates, returns pointer
- Cleared on new run (reset cursor to 0)
- Lives as private member of `PipelineLogTailer`

### File I/O (Win32)
- `GetFileAttributesExW` for file size check (no file handle needed)
- `fopen` / `fseek` / `fread` / `fclose` each `Poll()` — simple C I/O, no lock held
- File size comparison with `mLastReadPos` to detect truncation
- Read into a line buffer, split on `\n`, only process complete lines (save partial line for next Poll)

### JSON parsing
- `#include <DiaCore/Json/external/json/json.h>` — jsoncpp `Json::Reader` + `Json::Value`
- `NdjsonLineParser.h` (header-only, internal): takes a `const char* line`, returns `PipelineEvent`
- Maps JSON field names to struct fields per `dia.output.v1`:
  - `"event"` → `eventType` (StringCRC)
  - `"system"` → `system` (StringCRC)
  - `"stage"` → `stage` (StringCRC, empty string if absent)
  - `"step"` → `step` (StringCRC, empty string if absent)
  - `"ts"` → `timestampSec` (float)
  - `"durationMs"` → `durationMs` (int, -1 if absent)
  - `"error"` → `error` (pool string, nullptr if absent)
  - `"detail"` → `detail` (pool string, nullptr if absent)
  - `"level"` → `level` (pool string, nullptr if absent)
  - `"schema"` — checked on OnRunStarted only, not stored in struct
  - `"target"`, `"config"`, `"stages"` — OnRunStarted-only fields, used to init RunSummary
- Returns false on parse failure — caller logs warning and skips

### Observer notification
- `Dia::Core::ObserverSubject mObserverSubject`
- `NotifyObservers(int message)` called after processing all new lines in a Poll() cycle
- Message enum: `kNewEvents = 1`, `kRunStarted = 2`, `kRunCompleted = 3`, `kRunInterrupted = 4`

### Logging
- `#include <DiaLogger/DiaLog.h>`
- Channel: `"PipelineEditor"`
- `DIA_LOG_WARNING("PipelineEditor", "Malformed JSON at line %d: %s", lineNum, line)`
- `DIA_LOG_WARNING("PipelineEditor", "Unknown schema: %s", schemaStr)`

### Plugin shell (PipelineEditorPlugin)
- Implements `Dia::Editor::IEditorPlugin`
- `OnLoad`: create PipelineLogTailer, call Initialize() with resolved NDJSON path
- `OnUpdate(deltaTime)`: call `PipelineLogTailer::Poll()`
- `OnUnload`: call Shutdown()
- `REGISTER_EDITOR_PLUGIN(PipelineEditorPlugin, "DiaPipelineEditor")` at end of .cpp

### Unit tests
- Test files added to `Cluiche/Tests/GoogleTests/PipelineEditor/` directory
- Write NDJSON to a temp file, create PipelineLogTailer, call Poll(), assert events
- Mock observer to verify notification callbacks
- For interrupted detection: need injectable time source or `SetCurrentTime(float)` test helper

## Tasks

| # | Task | Status | Notes |
|---|------|--------|-------|
| 1 | Project scaffolding: create DiaPipelineEditor.vcxproj, .vcxproj.filters, add to solution, add to root module doc | Not Started | Reference DiaApplicationEditor.vcxproj for template |
| 2 | PipelineEvent.h: PipelineEvent + RunSummary structs | Not Started | Per system spec definitions |
| 3 | NdjsonLineParser.h: header-only single-line JSON → PipelineEvent parser | Not Started | Internal/ subfolder, jsoncpp |
| 4 | PipelineLogTailer.h/.cpp: Poll(), Initialize(), Shutdown(), string pool, state management | Not Started | Largest task — polling, truncation detection, RunSummary updates, interrupted detection |
| 5 | PipelineEditorPlugin.h/.cpp: IEditorPlugin shell wiring tailer to update loop | Not Started | Thin wrapper; registers with REGISTER_EDITOR_PLUGIN macro |
| 6 | Unit tests: AC1–AC9 coverage | Not Started | GoogleTests/PipelineEditor/ |
| 7 | Build + test: verify all 9 ACs pass | Not Started | |

## Session Notes

### 2026-04-27
- Created plan from approved spec
- Confirmed DiaApplicationEditor as reference plugin pattern
- Key patterns documented: Win32 file I/O (not FilePath aliases), jsoncpp for JSON, ObserverSubject for notifications
- NdjsonLineParser field mapping needs to match dia.output.v1 schema — check cli-output spec for exact JSON field names before implementing task 3
