**Spec:** @docs/specs/applications/dia/systems/diaobservation/captures.md
**Status:** Done

## Implementation Patterns

### Types (Tasks 1–2)
- `TriggerSource` enum, `CaptureMetadata` struct, `CaptureRequestStatus` enum, `CaptureResult` struct in `Dia/DiaObservation/Capture/CaptureTypes.h`
- All types in `Dia::Observation::Capture` namespace
- `CaptureMetadata::tag` is `StringCRC`; `context` is `char[128]` (fixed, no heap)

### CaptureManager (Tasks 3–5)
- Class in `Dia/DiaObservation/Capture/CaptureManager.h` + `.cpp`
- Owns fixed array `InFlightCapture mInFlight[kMaxInFlight]`
- `Initialize(ICanvas*)` — stores canvas pointer; does NOT create directories yet
- `Tick()` — iterates in-flight slots, polls `PollFrameCapture`, completes ready captures
- `RequestCapture(metadata)` — validates state (canvas set, session started, slot available), calls `ICanvas::RequestFrameCapture()`, stores token + metadata + frame number + scenario step
- Calls `SessionManager::Instance()` directly for directory/frame/scenario (singleton pattern matches existing DiaObservation code)

### PNG encoding (Task 4)
- Copy `lodepng.h` + `lodepng.cpp` into `Dia/DiaObservation/Capture/ThirdParty/` (avoids bimg dependency)
- Define `LODEPNG_NO_COMPILE_DECODER` to only compile encoder (reduces code size)
- `ICanvas` delivers RGBA8 — no channel swap needed in CaptureManager
- `lodepng_encode32` → PNG bytes → `fwrite` to file → `free` PNG buffer

### Observation record (Task 5)
- Use `DiaCore/Json/` to build the record JSON object
- Write via `ObservationFileSink` — same pattern as existing trace/metric records
- Fields: `record_type`, `schema_version`, `timestamp_unix_nano`, `frame_number`, `filename`, `tag`, `trigger`, `scenario_step`, `context`

### vcxproj integration (Task 2)
- Add new files to `DiaObservation.vcxproj` + `.vcxproj.filters` under `Capture/` filter
- lodepng ThirdParty files in a `Capture/ThirdParty` filter

### GoogleTest (Task 6)
- Test `CaptureManager` with a mock `ICanvas` that returns predictable `FrameCaptureResult` data
- Verify: file naming, directory creation, failure on ring-full, record emission
- Use `DiaObservation/Testing/` fixtures for session directory setup

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Define `CaptureTypes.h` (TriggerSource, CaptureMetadata, CaptureRequestStatus, CaptureResult) | Compiles | Done | haiku | Pure types |
| 2 | Add `Capture/` files + lodepng ThirdParty to DiaObservation vcxproj + filters | Builds | Done | haiku | Mechanical — copy lodepng, add to project |
| 3 | Implement `CaptureManager` core: Initialize, RequestCapture, Tick poll loop, slot management | Unit test — mock ICanvas, request accepted, slot tracked | Done | sonnet | Core state machine |
| 4 | Implement PNG write path: lodepng encode (RGBA8 from ICanvas) + file write to session captures dir | Unit test — encode known buffer, verify PNG header bytes | Done | sonnet | File I/O + encode |
| 5 | Implement observation record emission (JSON-line to ObservationFileSink) | Unit test — verify record fields in mock sink | Done | sonnet | JSON construction + sink integration |
| 6 | GoogleTest suite: full flow with mock ICanvas (request → tick → complete → file exists + record emitted) | `dia run googletest --filter="CaptureManager*"` | Done | sonnet | 7 tests, all pass |
| 7 | Wire `CaptureManager` into `SessionManager` lifecycle (create on Start, tick in Tick, cleanup on Stop) | `dia run cluichetest` — no crash, captures dir not created until first request | Done | sonnet | SetCaptureCanvas + GetCaptureManager added to SessionManager |

## Dependencies

- **frame-capture-readback** (DiaBgfx feature) must be Done before Task 3 can be integration-tested against real ICanvas. Unit tests with mock ICanvas can proceed independently.
