# Feature Spec: Captures (6th Observation Pillar)

## Parent System
@docs/specs/applications/dia/systems/diaobservation/diaobservation.md

## Status
`Done`

## Summary

Adds **Captures** as the 6th DiaObservation pillar (alongside Logs, Traces, Metrics, Health, Profiling). Provides a single entry point — `CaptureManager::RequestCapture(metadata)` — that triggers a GPU framebuffer readback via `ICanvas::RequestFrameCapture`, encodes the result as PNG, and writes it to the session directory. The code path is identical regardless of trigger source (human hotkey, automation command, checkpoint code).

## Problem

There is no structured way to capture rendered frames as part of an observation session. Visual regression testing, debug screenshots, and automation all need frame captures written to the session directory with appropriate metadata (frame number, scenario step, trigger context). Currently the only readback mechanism (`frame-capture-readback`) returns raw pixels — no one encodes, names, or stores them.

## Goals

- Single `CaptureManager` class in `Dia/DiaObservation/Capture/`
- One entry point: `RequestCapture(CaptureMetadata)` — trigger-agnostic
- PNG output written to `<sessionDir>/captures/<frameNumber>_<tag>.png`
- Metadata logged as a `record_type: "capture"` entry in the observation file sink
- Integrates with `SessionManager` for directory + frame count + scenario step
- Polls `ICanvas::PollFrameCapture` each tick to complete in-flight captures
- Exposes a result callback or pollable status so callers know when capture is written

## Non-Goals

- Image comparison / visual regression logic (Python orchestrator's job)
- Resolution override or format conversion (captures at native resolution, whatever `ICanvas` delivers)
- Video capture / continuous recording
- Buffering captures across sessions
- Any UI for capture management

## Acceptance Criteria

| # | Criterion |
|---|-----------|
| AC1 | `CaptureManager` class exists in `Dia/DiaObservation/Capture/` |
| AC2 | `RequestCapture(CaptureMetadata)` is the single trigger entry point |
| AC3 | Metadata struct carries: tag (StringCRC), trigger source (enum: kManual, kAutomation, kCode), optional context string |
| AC4 | PNG file written to `<sessionDir>/captures/<frameNumber>_<tag>.png` |
| AC5 | `record_type: "capture"` JSON-line emitted to observation file sink with frame number, filename, tag, trigger source, scenario step, timestamp |
| AC6 | `CaptureManager::Tick()` polls `ICanvas::PollFrameCapture` for in-flight captures and completes the write when ready |
| AC7 | When ring buffer is full (ICanvas returns invalid token), `RequestCapture` returns a failure status and logs a warning |
| AC8 | `CaptureManager` requires a pointer to `ICanvas` (set at init) and `SessionManager` (for directory/frame/scenario) |
| AC9 | PNG encoding uses lodepng (already available in `External/bimg/3rdparty/lodepng/`) |
| AC10 | Capture directory (`captures/`) created lazily on first capture request |

## Design

### CaptureManager

```cpp
namespace Dia::Observation::Capture
{
    enum class TriggerSource : unsigned char
    {
        kManual,        // Human-initiated (hotkey, debug menu)
        kAutomation,    // Orchestrator / DiaRemoteControl command
        kCode           // Programmatic (checkpoint, test logic)
    };

    struct CaptureMetadata
    {
        Dia::Core::StringCRC tag;           // e.g. "checkpoint_settle", "user_screenshot"
        TriggerSource        trigger;
        char                 context[128];   // Free-form context (e.g. stage name, test id)
    };

    enum class CaptureRequestStatus : unsigned char
    {
        kAccepted,          // Queued for capture
        kRejected_RingFull, // ICanvas ring buffer full
        kRejected_NoCanvas, // No ICanvas configured
        kRejected_NoSession // SessionManager not started
    };

    struct CaptureResult
    {
        enum class Status : unsigned char { kPending, kComplete, kFailed };
        Status status;
        char   filePath[512]; // Populated on kComplete
    };

    class CaptureManager
    {
    public:
        CaptureManager();
        ~CaptureManager();

        void Initialize(Dia::Graphics::ICanvas* canvas);
        void Tick(); // Call once per frame — polls in-flight captures

        CaptureRequestStatus RequestCapture(const CaptureMetadata& metadata);

        // Optional: query last N results
        unsigned int GetPendingCount() const;

    private:
        struct InFlightCapture
        {
            Dia::Graphics::FrameCaptureToken token;
            CaptureMetadata                  metadata;
            uint64_t                         frameNumber;
            Dia::Core::StringCRC             scenarioStep;
        };

        static constexpr unsigned int kMaxInFlight = 4; // Matches ICanvas ring depth

        Dia::Graphics::ICanvas* mCanvas;
        InFlightCapture         mInFlight[kMaxInFlight];
        unsigned int            mInFlightCount;
        bool                    mCapturesDirCreated;

        void CompleteCapture(InFlightCapture& capture, const Dia::Graphics::FrameCaptureResult& result);
        void EmitCaptureRecord(const InFlightCapture& capture, const char* filename);
        bool EnsureCapturesDirectory();
    };
}
```

### File Naming

`<sessionDir>/captures/<frameNumber>_<tag>.png`

Example: `Cluiche/out/cluichetest/sessions/2026-05-28T14-30-00-abc123/captures/000142_checkpoint_settle.png`

Frame number is zero-padded to 6 digits for filesystem sorting.

### Observation Record

```json
{
  "record_type": "capture",
  "schema_version": "1.0",
  "timestamp_unix_nano": 1748442600000000000,
  "frame_number": 142,
  "filename": "000142_checkpoint_settle.png",
  "tag": "checkpoint_settle",
  "trigger": "code",
  "scenario_step": "rigidbody2d_settle",
  "context": "10 circles expected settled"
}
```

### Integration Points

- **SessionManager** — `GetSessionDirectory()`, `GetFrameCount()`, `GetCurrentScenarioStep()`
- **ICanvas** — `RequestFrameCapture()`, `PollFrameCapture()` (delivers RGBA8 — no format conversion needed)
- **ObservationFileSink** — emits the capture record (same sink as logs/traces/metrics)
- **lodepng** — PNG encoding from RGBA8 pixel buffer (direct pass-through, no channel swap)

### Tick Flow

```
CaptureManager::Tick()
  for each in-flight capture:
    result = mCanvas->PollFrameCapture(token)
    if result.status == kReady:
      EnsureCapturesDirectory()
      lodepng_encode32(result.data, width, height) → PNG bytes
      Write PNG to captures/<frameNumber>_<tag>.png
      EmitCaptureRecord(...)
      Free slot
    if result.status == kFailed:
      Log warning, free slot
```

## Binding Decisions

**SD-O01 (all pillars in one DiaObservation module):** Captures lives in `Dia/DiaObservation/Capture/` — consistent with Logs, Traces, Metrics, Health, Profiling all under one module.

**PD-004 (no STL containers):** In-flight array is fixed C-array `[kMaxInFlight]`. PNG encoding output uses lodepng's C API (malloc-based buffer, freed after fwrite).

## Open Design Questions

1. ~~**lodepng linkage**~~ — **Resolved:** Copy `lodepng.h` + `lodepng.cpp` into `Dia/DiaObservation/Capture/ThirdParty/`. Define `LODEPNG_NO_COMPILE_DECODER`. Avoids transitive bimg dependency.

2. ~~**BGRA→RGBA swap**~~ — **Resolved:** Not CaptureManager's concern. `ICanvas::PollFrameCapture` delivers RGBA8 (DiaBgfx does the swap internally in the `screenShot` callback). CaptureManager passes bytes straight to lodepng.
