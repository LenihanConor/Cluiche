**Spec:** @docs/specs/applications/dia/systems/diabgfx/frame-capture-readback.md
**Status:** Done

## Implementation Patterns

### Types (Task 1–2)
- `FrameCaptureToken` and `FrameCaptureResult` defined in a new header `Dia/DiaGraphics/Interface/FrameCapture.h`
- `FrameCaptureResult::Status` uses `CLASSEDENUM` if it needs string conversion, otherwise plain `enum class`
- Token contains slot index (lower bits) + generation (upper bits) packed into a single `unsigned int`

### ICanvas extension (Task 3)
- Virtual methods with default no-op implementations (non-breaking change)
- Include `FrameCapture.h` from `ICanvas.h`

### Ring buffer (Task 4)
- Internal to DiaBgfx — not exposed in DiaGraphics interface
- Fixed array of `RingSlot` structs, `kRingDepth = 4`
- Pixel storage: heap-allocated via `malloc`/`free` (`unsigned char*` per slot, sized to `width*height*4`; reallocated if canvas size changes)
- Per-slot `std::atomic<State>` for thread safety between bgfx render thread (callback writer) and API thread (poll reader) — no mutex needed (single writer, single reader per slot)
- Generation counter prevents stale token access

### Callback wiring (Task 5)
- `CallbackHandler` gains a `FrameCaptureRingBuffer*` member (set once during `DeferredInit`)
- `screenShot()` override: decode slot index from filePath, BGRA→RGBA swap during copy into slot, flip state to `kReady`
- The swap is done here so consumers always see RGBA8 — bgfx backend format is fully contained within DiaBgfx
- filePath encoding: `"__capture_0"` through `"__capture_3"`

### Canvas integration (Task 6)
- `Canvas` owns a `FrameCaptureRingBuffer` instance (heap-allocated, created in `DeferredInit`)
- `RequestFrameCapture()`: find next free slot → mark pending → `bgfx::requestScreenShot(BGFX_INVALID_HANDLE, slotPath)` → return token
- `PollFrameCapture(token)`: validate generation → read slot state → return result

### GoogleTest (Task 7)
- Unit test the ring buffer logic (slot allocation, generation, overflow, token validation) in isolation
- Mock the bgfx callback path (test that screenShot populates slot correctly)

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Define `FrameCaptureToken` + `FrameCaptureResult` in `Dia/DiaGraphics/Interface/FrameCapture.h` | Compiles | Done | haiku | Pure types, no logic |
| 2 | Add `FrameCapture.h` to DiaGraphics vcxproj + filters | Builds | Done | haiku | Mechanical |
| 3 | Add `RequestFrameCapture()` / `PollFrameCapture()` virtuals to `ICanvas` with default no-op | Builds (no break) | Done | haiku | Non-breaking interface extension |
| 4 | Implement `FrameCaptureRingBuffer` class in DiaBgfx (internal header) | Unit test ring logic | Done | sonnet | Core data structure — slot alloc, generation, overflow, atomic state |
| 5 | Wire `CallbackHandler::screenShot()` to write into ring buffer | Unit test callback path | Done | sonnet | Thread boundary — atomic state transitions |
| 6 | Implement `Canvas::RequestFrameCapture()` + `PollFrameCapture()` | Integration — request + poll returns kReady after frame advance | Done | sonnet | Ties ring buffer + callback + bgfx API together |
| 7 | GoogleTest suite for ring buffer + token validation | `dia run googletest --filter="FrameCaptureRing*"` | Done | sonnet | 10 tests, all pass |
