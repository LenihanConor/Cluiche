# Research: Explore — SFML → SDL3 Swap (Window + Input)

**Session date:** 2026-05-26
**Folder:** docs/research/sfml_sdl3_swap/

## Problem Space Overview

SFML's window and input layer (`sf::Window`, `sf::Event`) is a well-understood, thin abstraction over the OS. In the Cluiche codebase it lives entirely inside DiaSFML — only two files (`Window.cpp`, `InputSource.cpp`) include any SFML headers. DiaWindow and DiaInput define the abstract interfaces (`IWindow`, `IInputSource`); DiaSFML is the concrete backend that implements them. The rendering backend has already been replaced with DiaBgfx, so the only remaining SFML dependency is window creation and event polling.

The driver for this swap is multi-platform support: Android, iOS, Linux, and Windows. SFML's window backend on Android and iOS is incomplete and not maintained at the same level as its Windows/Linux ports. SDL3 (released stable 2024) was designed from the ground up for exactly this target matrix and is the de-facto cross-platform windowing/input library for native C++ game engines. It also offers native touch, gamepad, and sensor APIs that are absent from SFML.

The swap is well-bounded: replace DiaSFML's two `.cpp` files (and the `.h` files that expose `sf::` types) with a new DiaSdl module (or rename DiaSFML) that implements the same `IWindow` + `IInputSource` contracts using `SDL_Window`, `SDL_Event`, and related SDL3 APIs. The rest of the engine — DiaWindow, DiaInput, DiaBgfx, DiaApplicationFlow — is untouched.

## Existing Approaches

- **Direct replacement** — create DiaSdl as a sibling to DiaSFML, implement Window and InputSource against SDL3, update the wiring in the application module that selects the backend.
- **Abstraction layer stays, backend swaps** — the current design already has this; IWindow and IInputSource are the abstraction layer. DiaSdl just replaces DiaSFML without changing the contracts.
- **Platform-conditional compilation** — keep DiaSFML for Windows editor path, add DiaSdl for game targets. Useful if Win32WndProcChain (used by the debug ImGui backend) is hard to replicate in SDL3.
- **SDL3 + bgfx integration pattern** — bgfx already has an SDL3 integration example; SDL_Window exposes a native handle that bgfx can consume via `bgfx::PlatformData`, the same way the SFML window currently works.
- **Thin event-translation table** — SDL3 events are a tagged union (SDL_Event) very similar in shape to Dia::Input::Event; a 1:1 translation table is straightforward.

## Design Axes

| Axis | Options | Notes |
|------|---------|-------|
| Module strategy | Replace DiaSFML in-place vs. new DiaSdl module | New module preserves git history of DiaSFML; easier to review |
| Win32WndProcChain | Drop it / port to SDL3 / keep DiaSFML for editor | SDL3 has `SDL_SetWindowsMessageHook`; can replicate the DIA_DEBUG hook |
| Touch input | Ignore now / map to mouse events / new EType entries | SDL3 SDL_EVENT_FINGER_* events have no Dia equivalent today |
| Gamepad | Keep existing joystick path / migrate to SDL3 gamepad API | SDL3 gamepad API is richer than SFML joystick; Dia::Input already has ConsoleGamepad types |
| Platform-conditional module | Single DiaSdl for all platforms / platform-specific subclasses | SDL3 is already cross-platform; no subclassing needed |
| IWindow.Style / SetActive | Map directly / deprecate SetActive (OpenGL concept) | SetActive was an OpenGL concept; bgfx doesn't need it — can stub or remove |

## Known Tradeoffs

- SDL3 is a C library; SFML is C++. Wrapping SDL3 is slightly more verbose but perfectly fine.
- SDL3 init/quit is global (`SDL_Init` / `SDL_Quit`) — must be managed at the ProcessingUnit level, not per-window.
- `SDL_Window` returns a native handle via `SDL_GetPointerProperty(SDL_GetWindowProperties(win), SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL)` — more verbose than SFML's `getNativeHandle()` but identical capability.
- SDL3 event loop uses `SDL_PollEvent` which is familiar; the tagged-union shape of `SDL_Event` maps cleanly to `Dia::Input::Event`.
- `IWindow::SetActive(bool)` was an OpenGL-era API; SDL3 has no equivalent. Stubbing it to return `true` is safe given bgfx owns the render context.
- Win32WndProcChain relies on SFML's WndProc being hookable. SDL3 exposes `SDL_SetWindowsMessageHook` (Windows only, DIA_DEBUG guard) which provides an equivalent interception point.
- SDL3's audio, networking, and threading are available but **not needed** — this swap is window+input only.

## Known Pitfalls (C++ / game engine context)

- SDL3 `SDL_Init(SDL_INIT_VIDEO)` must be called once before any window creation; forgetting this on Android/iOS causes silent failures.
- Android/iOS require `SDL_main` entry-point convention or explicit `SDL_SetMainReady()` — the application main() must be adapted per platform.
- SDL3 key codes are `SDL_Keycode` (UTF-32 codepoint-like values), not a dense enum — the translation to `Dia::Input::EKey` requires a lookup table, not a cast.
- `SDL_EVENT_WINDOW_RESIZED` fires on DPI changes too; need to guard or the engine may get spurious resize events.
- Touch events (`SDL_EVENT_FINGER_*`) have no current entry in `Dia::Input::Event::EType` — they will silently fall through the translation unless new types are added.
- SDL3 gamepad rumble, sensors, and battery APIs are bonus features; don't let scope creep pull them in.
- On iOS, the SDL3 window lifecycle is managed by `UIApplicationDelegate`; the ProcessingUnit shutdown must flush the SDL event queue before destroying the window.

## Cluiche-Specific Opportunities

### Relevant Existing Modules

| Module | Relevance |
|--------|-----------|
| DiaSFML | The module being replaced — contains Window.h/cpp, InputSource.h/cpp, Win32WndProcChain.h/cpp |
| DiaWindow | Defines IWindow, IWindow::Settings, SystemHandle — these contracts do not change |
| DiaInput | Defines IInputSource, Event, EKey, EMouseButton, EJoystick, ConsoleGamepad — translation target |
| DiaBgfx | Consumes the native window handle via bgfx::PlatformData::nwh — must still get HWND/ANativeWindow |
| DiaApplicationFlow | Owns ProcessingUnit lifecycle — SDL_Init/SDL_Quit must hook here |

### Platform Decision Constraints

| Decision | Implication for this topic |
|----------|---------------------------|
| PD-001 StringCRC | SDL3 window titles are const char*; no StringCRC conflict |
| PD-002 ProcessingUnit/Phase/Module | SDL_Init/SDL_Quit must live in a Module or Phase, not in Window ctor/dtor |
| PD-004 No STL in public APIs | SDL3's C API returns raw pointers and value types; no STL leakage risk |
| PD-005 x64 Windows only (current) | Goal is to *expand* this; PD-005 will need to be updated as part of this work |
| PD-006 VS project files | DiaSdl will need a new .vcxproj; Android/iOS build integration is a separate concern |
| PD-007 C++20 | SDL3 headers are C99/C11; no conflict with C++20 consumer |
| PD-008 Directory.Build.props | DiaSdl output paths follow the same sharedlibs rule automatically |

### SystemHandle gap

`DiaWindow/SystemHandle.h` already has `#if defined(WIN32)` and `#if defined(ANDROID_OS)` guards — it was designed for this. Linux and iOS `SystemHandle` typedefs are missing and must be added.

### Actual window backend inventory (post-scan)

There are **three** window creation paths in the codebase, not one:

| Path | File | Interface | Used by | Platform |
|------|------|-----------|---------|----------|
| DiaSFML | DiaSFML/Window.cpp | IWindow + IInputSource | CluicheTest (game) | Windows only today |
| Win32Window | DiaWindow/Win32Window.cpp | IWindow | CluicheEditor | Windows-only (intentional) |
| SplashScreenModule | CluicheEditor/Modules/SplashScreenModule.cpp | **none** (raw Win32) | CluicheEditor startup | Windows-only (intentional) |

- **Win32Window** is already a proper IWindow implementation inside DiaWindow — the editor uses `CreateNativeWindow()` / `NativeWindow.h` to get one. It is editor-only and intentionally Windows-only, so it does **not** need to be touched by this swap.
- **SplashScreenModule** is pure raw Win32 GDI, lives in the editor application, and is also intentionally Windows-only. Out of scope.
- The **only path that needs to become cross-platform is DiaSFML** (the game path used by CluicheTest and future game applications).

### Audio

No audio system exists anywhere in the codebase — SFML Audio was never used. There is nothing to lose.

### IWindow::SetActive(bool)

This was an OpenGL surface activation concept. bgfx owns the render context; neither DiaBgfx nor the SDL3 path needs this. It can be removed from IWindow entirely. Win32Window already stubs it (`return true`).

## Open Questions for Ideation

- Should the new module be called **DiaSdl** (clean break) or should DiaSFML be renamed in-place?
- Should touch input be added to `Dia::Input::Event::EType` now, or deferred to a follow-on feature?
- Does `Win32WndProcChain` move into DiaSdl (Windows-only, DIA_DEBUG) or into DiaBgfx directly?
- How does `SDL_SetMainReady()` / platform main interop get handled for Android/iOS — CluicheTest concern or DiaSdl platform adapter concern?
- Should a single DiaSdl module compile on all four platforms with `#ifdef` guards, or use platform-specific subclasses?
