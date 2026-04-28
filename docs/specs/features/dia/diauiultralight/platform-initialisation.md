# Feature Spec: Platform Initialisation

## Parent System
@docs/specs/systems/dia/diauiultralight.md

## Summary

Set up the Ultralight `Platform` singleton with all required handlers (`FileSystem`, `FontLoader`, `Logger`, `Config`) and create the `Renderer` and `View`. This is the foundation that all other DiaUIUltralight features depend on.

## Goals

- Initialise Ultralight exactly once per `UISystem` instance
- Provide a `FileSystem` that reads local files via `fopen_s`
- Provide a `Logger` that forwards to `Dia::Core::Log`
- Use `GetPlatformFontLoader()` to avoid embedding font files
- Create a CPU-rendered, transparent `View` at the window's current size

## Tasks

| # | Task | Implementation |
|---|---|---|
| 1 | Configure `Platform` | Set `Config::cache_path`; call `set_config`, `set_file_system`, `set_logger`, `set_font_loader` |
| 2 | Create `Renderer` | Call `Renderer::Create()` after platform is configured; assert non-null |
| 3 | Create `View` | `ViewConfig { is_accelerated=false, is_transparent=true, enable_javascript=true }`; width/height from `IWindow::GetSize()` |
| 4 | Attach listeners | `set_load_listener(this)`, `set_view_listener(this)` on the View |
| 5 | Guard re-init | `if (mIsInitialized) return;` at the top of `Initialize()` |

## Binding Decisions Compliance

| Decision | How this feature honours it |
|---|---|
| PD-001 C++ primary | All initialisation code is C++17 |
| PD-002 Windows primary | Uses `GetPlatformFontLoader()` (Windows system fonts via AppCore) |
| UL-001 CPU renderer | `is_accelerated = false` |
| UL-006 Platform font loader | `GetPlatformFontLoader()` used; no custom font implementation |
| UL-007 Transparent | `is_transparent = true` |

## AI Review Questions

| # | Question | Answer |
|---|---|---|
| 1 | What if `Renderer::Create()` returns null? | `DIA_ASSERT` fires; requires FileSystem and FontLoader to be set first |
| 2 | What if `GetPlatformFontLoader()` returns null? | `DIA_ASSERT` fires; means AppCore.dll is missing or not loaded |
| 3 | Does the cache path need to exist? | Ultralight creates it; `.ultralight_cache` in the working directory |
| 4 | Is `Platform` a singleton — safe to call multiple times? | `Platform::instance()` is safe; `set_*` calls after first `Renderer::Create()` are ignored by Ultralight |

## Status

`Done` - Implemented
