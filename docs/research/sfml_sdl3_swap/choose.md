# Research: Choice — SFML → SDL3 Swap (Window + Input)

**Date:** 2026-05-26
**Chosen candidate:** C1 — DiaSdl Minimal (module named DiaSDL)

## Rationale

C1 delivers the primary goal — 4-platform window and input support (Windows, Linux, Android, iOS) — with the tightest scope and lowest risk. The SDL3 API maps cleanly to all existing `Dia::Input::Event` types that are actually in use. Touch and platform bootstrap are deliberately deferred until mobile build environments exist and the decisions can be verified against real targets. C6 (Win32WndProcChain relocation) is mandatory prep bundled as the first commit inside this spec.

## What Was Ruled Out

| Candidate | Reason not chosen |
|-----------|------------------|
| C2 DiaSdl + Touch | Touch coordinate space decision (normalised vs pixels) is best made when building a real touch-driven game; deferred to follow-on |
| C3 DiaSdl + Bootstrap | SDL_main / iOS UIKitRunApp wiring can't be tested until mobile toolchains exist; deferred until mobile builds start |
| C4 DiaSdl Full | C2 + C3 risks bundled together; both deferred for the same reasons above |
| C5 In-Place Rename | Rename ripples across registry, docs, build scripts; harder to review and loses the safety net of a parallel fallback during the transition |
| C6 WndProcChain → DiaBgfx | Not ruled out — mandatory, bundled as first commit inside C1 |
| C7 Parallel Backends | Maintains two implementations with no long-term payoff; DiaSFML deleted once DiaSDL is verified on Windows |
| C8 DiaWindow Unification | XL scope, high architectural risk, no proportionate benefit over a clean adapter module |

## Pre-Spec Commitments

- Module name: **DiaSDL** (uppercase SDL), namespace `Dia::SDL`
- **C6 bundled in:** Move `Win32WndProcChain.h/.cpp` from DiaSFML into DiaBgfx as a first commit before DiaSDL is built
- **`IWindow::SetActive(bool)` removed** from `IWindow` interface — it was an OpenGL concept; bgfx doesn't need it. `Win32Window` stub and DiaSFML stub both deleted.
- **Dead `EType` entries removed from DiaInput:** `kMouseWheelMoved`, `kMouseEntered`, `kMouseLeft` were never mapped in DiaSFML and are presumed unconsumed. Verify with a grep before deleting; if any consumer exists, flag and defer removal.
- **DiaSFML deleted** once DiaSDL is wired in and verified on Windows — no parallel backends.
- **`SystemHandle.h` extended** with Linux (`unsigned long` / `xcb_window_t`) and iOS (`void*` / `UIView*`) typedefs under the appropriate `#ifdef` guards.
- **SDL3 `SDL_Init`/`SDL_Quit`** managed at Module level (not Window ctor/dtor) to comply with PD-002.
- **SDL3 key-code → `Dia::Input::EKey`** translation via lookup table, not a cast.
- Touch input, platform bootstrap, and audio are explicitly out of scope.

## Next Step

Run `/spec-feature` with this candidate as input.
Suggested parent system: `dia.sdl` (new system) under application `dia`
Suggested spec path: `docs/specs/systems/dia/diasdl.md` or as a feature under a new DiaSDL system spec
