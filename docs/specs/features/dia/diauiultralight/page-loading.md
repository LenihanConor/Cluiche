# Feature Spec: Page Loading

## Parent System
@docs/specs/systems/dia/diauiultralight.md

## Summary

Load an HTML page into the Ultralight `View` from a `Dia::Core::FilePath`, blocking until loading completes. Support clearing the current page via `UnloadPage`.

## Goals

- Convert a `Dia::Core::FilePath` to a valid `file://` URL (backslashes normalised to forward slashes)
- Store pending `BoundMethod` entries before loading so `OnDOMReady` can register them
- Block in `LoadPage` until `View::is_loading()` returns false, pumping the renderer each millisecond
- `IsPageLoaded()` returns the correct state at all times
- `UnloadPage()` clears the loaded flag without destroying the View

## Tasks

| # | Task | Implementation |
|---|---|---|
| 1 | Store bindings | Copy `Page::GetBoundMenthods()` into `mPendingBindings` before calling `LoadURL` |
| 2 | Build URL | `FilePath::Resolve()` → prepend `"file:///"` → replace `\\` with `/` |
| 3 | Load | `mView->LoadURL(url)` |
| 4 | Pump until done | `while (mView->is_loading()) { mRenderer->Update(); sleep_for(1ms); }` then `mRenderer->Render()` |
| 5 | Set flag | `mIsPageLoaded = true` in outer `UISystem::LoadPage` after impl returns |
| 6 | Unload | `mIsPageLoaded = false`; pending bindings cleared on next `LoadPage` |

## Binding Decisions Compliance

| Decision | How this feature honours it |
|---|---|
| UL-003 OnDOMReady for binding | Bindings stored here, consumed in `OnDOMReady` feature |
| UL-004 file:// URL scheme | `file:///` prefix used; `DiaFileSystem` resolves from that |

## AI Review Questions

| # | Question | Answer |
|---|---|---|
| 1 | What if the file doesn't exist? | `OnFailLoading` fires → `DIA_ASSERT(0, ...)` with URL and error description |
| 2 | Is the busy-wait safe? | Yes — same pattern as Awesomium; 1 ms sleep avoids busy spinning; no timeout needed for local files |
| 3 | What if `LoadPage` is called while a page is already loaded? | `mPendingBindings` is cleared and the new URL is loaded; Ultralight navigates the existing View |
| 4 | Thread safety of `LoadPage`? | Guarded by `mSystemMutex` in the outer `UISystem`; Ultralight requires single-thread usage |

## Status

`Approved`
