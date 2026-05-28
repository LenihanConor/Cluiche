# Feature Spec: Frame Capture Readback

## Parent System
@docs/specs/systems/dia/render-backend.md

## Status
`Done`

## Summary

Adds async GPU framebuffer readback to the rendering pipeline. `ICanvas` gets a `RequestFrameCapture` / `PollFrameCapture` interface pair; `DiaBgfx::Canvas` implements it using `bgfx::requestScreenShot()` and its existing `CallbackHandler::screenShot()` callback, with a fixed-depth ring buffer for in-flight capture results.

This is a pure capability — no file I/O, no observation, no trigger policy. Consumers (DiaObservation Captures pillar, automation, debug tools) call the interface and receive pixels.

## Problem

There is no way to read rendered pixels back from the GPU. The DiaObservation Captures pillar, visual regression testing, and debug screenshot tooling all need this primitive. bgfx provides `requestScreenShot` + callback but it's not wired into our `ICanvas` abstraction.

## Goals

- Expose frame capture as a first-class `ICanvas` capability
- Async (non-blocking) — capture request doesn't stall the render pipeline
- Clean data lifetime contract — pointer valid until ring slot recycled
- Single implementation path regardless of who requests the capture

## Non-Goals

- File I/O / PNG encoding (DiaObservation's responsibility)
- Resolution override / sub-region crop (future feature if needed)
- Format conversion (return whatever bgfx delivers)
- Comparison / diffing logic
- Trigger policy (who calls it and when)

## Acceptance Criteria

| # | Criterion |
|---|-----------|
| AC1 | `ICanvas` exposes `RequestFrameCapture()` returning a `FrameCaptureToken` |
| AC2 | `ICanvas` exposes `PollFrameCapture(token)` returning a `FrameCaptureResult` (status, data ptr, width, height, pitch, format) |
| AC3 | DiaBgfx `Canvas` implements using `bgfx::requestScreenShot()` + `CallbackHandler::screenShot()` |
| AC4 | Ring buffer depth of 4 (compile-time constant, easy to change) |
| AC5 | `FrameCaptureResult::data` pointer valid until that ring slot is reused by a subsequent request |
| AC6 | When ring is full, `RequestFrameCapture()` returns an invalid token (caller can check) |
| AC7 | Captures at current canvas resolution; data delivered as RGBA8 (backend-native BGRA swapped in callback — consumers never see backend format) |
| AC8 | Non-blocking — `RequestFrameCapture` and `PollFrameCapture` do not call `bgfx::frame()` or stall |

## Design

### Types (DiaGraphics)

```cpp
namespace Dia::Graphics
{
    struct FrameCaptureToken
    {
        unsigned int id;        // Ring slot index + generation counter
        bool IsValid() const;
    };

    struct FrameCaptureResult
    {
        enum class Status : unsigned char
        {
            kPending,       // Readback not yet complete
            kReady,         // Data available
            kFailed,        // Capture failed
            kInvalidToken   // Token was never issued or slot recycled
        };

        Status          status;
        const void*     data;       // Pixel buffer (nullptr unless kReady)
        unsigned int    width;
        unsigned int    height;
        unsigned int    pitch;      // Bytes per row
        unsigned int    format;     // bgfx::TextureFormat::Enum cast to uint
    };
}
```

### ICanvas additions

```cpp
virtual FrameCaptureToken RequestFrameCapture() { return FrameCaptureToken{0}; } // no-op default
virtual FrameCaptureResult PollFrameCapture(const FrameCaptureToken& token) { return {}; }
```

Default implementations return invalid/empty so existing `ICanvas` implementers aren't broken.

### DiaBgfx Implementation

- `CallbackHandler` gets a pointer to a `FrameCaptureRingBuffer` (set during `Canvas::DeferredInit`)
- `CallbackHandler::screenShot()` copies pixel data into the ring slot identified by the filePath token encoding
- `Canvas::RequestFrameCapture()` — claims next ring slot, encodes slot index into filePath string, calls `bgfx::requestScreenShot(BGFX_INVALID_HANDLE, encodedPath)`
- `Canvas::PollFrameCapture(token)` — checks ring slot status, returns result

### Ring Buffer

```cpp
static constexpr unsigned int kRingDepth = 4;

struct RingSlot
{
    enum class State : unsigned char { kFree, kPending, kReady };

    State           state;
    unsigned int    generation;  // Increments each reuse — validates tokens
    unsigned int    width;
    unsigned int    height;
    unsigned int    pitch;
    unsigned char*  pixelData;   // Heap-allocated RGBA8 buffer (malloc/free)
    unsigned int    pixelDataSize;
};
```

Pixel buffer is heap-allocated (sized to `width * height * 4` on first use per slot, reallocated if canvas size changes). The `screenShot()` callback performs BGRA→RGBA channel swap during copy — consumers always receive RGBA8 regardless of backend.

The `filePath` string passed to `bgfx::requestScreenShot` encodes the slot index (e.g., `"__capture_slot_2"`) so `screenShot()` callback knows where to write.

## Binding Decisions

**PD-004 (no STL containers):** Ring buffer uses `DynamicArrayC` for pixel storage. No `std::vector`.

No other parent decisions constrain this feature.

## Open Design Questions

1. ~~**Pixel buffer sizing**~~ — **Resolved:** heap-allocated (`malloc`/`free`). Sized per-slot to `width*height*4`; reallocated if canvas size changes.

2. **Thread safety of screenShot callback** — bgfx may call `screenShot()` from its render thread while `PollFrameCapture` is called from the API thread. Need a lightweight synchronization (per-slot atomic state flag, or mutex on the ring). Recommendation: per-slot `std::atomic<State>` — no mutex needed since only one writer (callback) and one reader (poll) per slot at a time.
