# Feature Spec: Pixel Buffer Readback

## Parent System
@docs/specs/systems/dia/diauiultralight.md

## Summary

After each frame, lock the Ultralight `BitmapSurface`, copy pixels to a fixed 16 MB staging buffer, and expose them to callers via `UIDataBuffer::CreateFromPreallocatedBuffer`. Clears the surface's dirty bounds after copying so the next frame only copies when content changes.

## Goals

- Provide pixel data to `FetchUIDataBuffer` with no heap allocation per frame
- Match the `UIDataBuffer` contract: caller receives a non-owning buffer pointer
- Guard against oversized bitmaps with a `DIA_ASSERT`
- Clear dirty bounds after each copy so the surface correctly tracks changes

## Tasks

| # | Task | Implementation |
|---|---|---|
| 1 | Call `Renderer::Render()` | Done in `Update()` each frame; ensures surface is up to date |
| 2 | Get surface | `static_cast<BitmapSurface*>(mView->surface())` |
| 3 | Get bitmap | `surface->bitmap()` |
| 4 | Lock pixels | `bitmap->LockPixels()` |
| 5 | Assert size | `DIA_ASSERT(size <= sBufferSize, ...)` |
| 6 | memcpy | Copy `size` bytes to `mStagingBuffer` |
| 7 | Unlock | `bitmap->UnlockPixels()` |
| 8 | Clear dirty | `surface->ClearDirtyBounds()` |
| 9 | Expose | `outBuffer.CreateFromPreallocatedBuffer(w, h, mStagingBuffer, size, false)` |

## Staging Buffer

- Size: `16 * 1024 * 1024` bytes (16 MB) — covers up to 2048×2048 BGRA32
- Location: `mStagingBuffer` — member of `UISystemImpl`, stack-allocated at construction
- Lifetime: owned by `UISystemImpl`; `UIDataBuffer` holds a non-owning pointer (`deleteWhenDone = false`)

## Pixel Format

Ultralight CPU renderer produces **premultiplied BGRA 32-bit** (4 bytes per pixel). This matches the Awesomium surface format, so existing rendering code in `MainUIModule` requires no changes.

## Binding Decisions Compliance

| Decision | How this feature honours it |
|---|---|
| UL-001 CPU renderer | `View::surface()` only valid for CPU renderer; `is_accelerated = false` ensures this |
| UL-005 16 MB staging buffer | Fixed buffer size matches Awesomium precedent |

## AI Review Questions

| # | Question | Answer |
|---|---|---|
| 1 | What if the window is resized beyond 2048×2048? | `DIA_ASSERT` fires; buffer size constant `sBufferSize` needs increasing |
| 2 | Should we only copy when dirty? | Possible optimisation (check `!surface->dirty_bounds().IsEmpty()`); not done yet to keep parity with Awesomium behaviour |
| 3 | Is `bitmap->LockPixels()` thread-safe? | No — Ultralight is single-threaded; the outer `mSystemMutex` ensures `FetchUIDataBuffer` is not called concurrently |
| 4 | Why `false` for `deleteWhenDone`? | Staging buffer lifetime is `UISystemImpl`; `UIDataBuffer` must not free memory it doesn't own |

## Status

`Approved`
