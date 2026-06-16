# Feature Spec: Replace DiaSFML with DiaSDL (Window + Input)

**Parent system:** DiaWindow / DiaInput (Dia application)
**Research:** @docs/research/sfml_sdl3_swap/summary.md
**Status:** `Approved`
**Plan:** replace-diasfml-with-sdl3.plan.md

---

## Summary

Replace the `DiaSFML` window and input backend with a new `DiaSDL` module that implements the same `IWindow` + `IInputSource` contracts using SDL3. This removes the last SFML dependency from the game path and enables CluicheTest and future games to compile and run on Windows, Linux, Android, and iOS from a single code path.

The editor (CluicheEditor) continues to use `DiaWindow::Win32Window` and is unaffected.

---

## Goals

- Four-platform window + input support: Windows, Linux, Android, iOS
- Zero functional regressions on Windows
- `DiaSFML` module deleted; no parallel backends maintained
- `IWindow` interface cleaned up (remove `SetActive(bool)`)
- Dead DiaInput event types removed

## Non-Goals

- Touch input (`kTouchBegan` / `kTouchMoved` / `kTouchEnded`) — deferred to a follow-on feature
- Platform entry-point bootstrap (`SDL_main` / `SDL_UIKitRunApp`) — deferred until mobile build environments exist
- Audio — not implemented anywhere; out of scope
- CluicheEditor window migration — editor is Windows-only by design; `Win32Window` stays

---

## Acceptance Criteria

| # | Criterion |
|---|-----------|
| AC-00 | SDL3 added as git submodule under `External/SDL3/`; `dia env setup` builds SDL3 from source; `dia env verify` confirms SDL3 present |
| AC-01 | `Dia/DiaSDL/` module created with `Window.h/.cpp`, `WindowFactory.h/.cpp`, `InputSource.h/.cpp`; namespace `Dia::SDL` |
| AC-02 | `DiaSDL::Window` implements `Dia::Window::IWindow`; `DiaSDL::InputSource` implements `Dia::Input::IInputSource` |
| AC-03 | `Win32WndProcChain.h/.cpp` moved from `Dia/DiaSFML/` to `Dia/DiaBgfx/`; `BgfxImGuiBackend` updated to include from new path |
| AC-04 | `IWindow::SetActive(bool)` removed from `DiaWindow::IWindow`; all implementations (`Win32Window`, old DiaSFML stub) updated |
| AC-05 | Dead `EType` entries grepped for consumers; if none found, `kMouseWheelMoved`, `kMouseEntered`, `kMouseLeft` removed from `Dia::Input::Event::EType` |
| AC-06 | `SystemHandle.h` extended with Linux (`void*`, covers X11 `Window` and Wayland `wl_surface*`) and iOS (`void*` / `UIView*`) typedefs under correct `#ifdef` guards |
| AC-07 | `SDL_Init(SDL_INIT_VIDEO)` called in `DiaSDL::WindowFactory::Create` (first window creation); `SDL_Quit()` called in `DiaSDL::WindowFactory::Destroy` (last window destroyed) — mirrors the SFML pattern; no SDLModule required |
| AC-08 | SDL3 → `Dia::Input::EKey` translation implemented as a lookup table (not a cast) |
| AC-09 | All existing event types mapped: `kClosed`, `kResized`, `kKeyPressed`, `kKeyReleased`, `kTextEntered`, `kMouseMoved`, `kMouseButtonPressed`, `kMouseButtonReleased`, `kJoystickConnected`, `kJoystickDisconnected`, `kJoystickMoved`, `kJoystickButtonPressed`, `kJoystickButtonReleased` |
| AC-10 | `Dia/DiaSFML/` module deleted; `.vcxproj` and `.vcxproj.filters` removed; all references updated |
| AC-11 | `DiaSDL.vcxproj` and `.vcxproj.filters` created; follows `Directory.Build.props` layout rules (sharedlibs output path) |
| AC-12 | `dia.sdl.architecture.module.md` created in `Dia/DiaSDL/` |
| AC-13 | All GoogleTests pass on Windows (`dia run googletest`) |
| AC-14 | CluicheTest runs on Windows with no visual/input regressions (`dia run cluichetest`) |

---

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| T-00 | Add SDL3 git submodule under `External/SDL3/`; add `dia env setup` build step for SDL3 via CMake; update `DiaEnv` env manifest | `dia env verify` reports SDL3 present | Todo | sonnet | Must land before T-05; follow bgfx acquisition pattern |
| T-01 | Move `Win32WndProcChain.h/.cpp` from `Dia/DiaSFML/` → `Dia/DiaBgfx/Imgui/` via PowerShell script; update `.vcxproj`/`.vcxproj.filters` for both modules; update `BgfxImGuiBackend` include path | Build passes, ImGui still works | Todo | sonnet | Prep commit; must land before DiaSFML is touched |
| T-02 | Remove `IWindow::SetActive(bool)` from `DiaWindow::IWindow`; stub-remove from `Win32Window`; stub-remove from DiaSFML `Window` | Build passes | Todo | haiku | AC-04 |
| T-03 | Grep `kMouseWheelMoved`, `kMouseEntered`, `kMouseLeft` for consumers; remove dead `EType` entries if none | Build passes, no compile errors | Todo | haiku | AC-05; if consumers found, flag and defer |
| T-04 | Add Linux + iOS `SystemHandle` typedefs to `SystemHandle.h` | Build passes on Windows; typedefs present | Todo | haiku | AC-06 |
| T-05 | Create `Dia/DiaSDL/` folder, `.vcxproj`, `.vcxproj.filters`, `dia.sdl.architecture.module.md` | Project loads in VS | Todo | haiku | AC-11/AC-12 |
| T-06 | Implement `DiaSDL::InputSource` — `SDL_PollEvent` loop, event translation, `EKey` lookup table | Existing input GoogleTests pass | Todo | sonnet | AC-02/AC-08/AC-09 |
| T-07 | Implement `DiaSDL::Window` + `DiaSDL::WindowFactory` — `SDL_Window` creation, `IWindow` methods, `GetSystemHandle()` native handle extraction | Window opens on Windows | Todo | sonnet | AC-01/AC-02 |
| T-08 | Wire `DiaSDL::WindowFactory` into CluicheTest in place of `DiaSFML::WindowFactory`; verify SDL_Init/Quit fires correctly via WindowFactory | CluicheTest runs end-to-end | Todo | sonnet | AC-07 |
| T-09 | Delete `Dia/DiaSFML/`; remove `.vcxproj`/`.vcxproj.filters`; update solution and all references | Full solution builds | Todo | sonnet | AC-10 |
| T-10 | Run full verification: `dia run googletest` + `dia run cluichetest`; update `dia.sdl.architecture.module.md` | All tests green, CluicheTest visual check | Todo | haiku | AC-13/AC-14 |

---

## Traceability

| Level | Spec | ID |
|-------|------|----|
| Platform | @docs/specs/platform/Cluiche.md | — |
| Application | @docs/specs/applications/dia/dia.md | — |
| System | DiaWindow / DiaInput (TBD system specs in `dia.md`) | — |
| Feature | This spec | — |

---

## Binding Decisions Compliance

| Decision | Summary | Compliance |
|----------|---------|------------|
| PD-001 StringCRC | Use StringCRC for all identifiers | Compliant — SDL3 is a C API; no string IDs leak into Dia public surface. Window titles use `String64`. |
| PD-002 ProcessingUnit/Phase/Module | App structure via PU/Phase/Module | Compliant — `SDL_Init`/`SDL_Quit` placed in a Module (AC-07); not in Window ctor/dtor |
| PD-003 Component-based entities | IComponent/IComponentObject for entities | Compliant — DiaSDL is a platform adapter, not a game entity system; not applicable |
| PD-004 No STL in public APIs | Dia containers only in public APIs | Compliant — SDL3 is a C API; DiaSDL public headers use `DiaCore` types only (`String64`, `BitArray8`). `std::function` callbacks in `NativeWindow.h` are pre-existing (not introduced by this feature). |
| PD-005 x64 Windows only | x64 only (current constraint) | Compliant on Windows; this feature *expands* the platform matrix — PD-005 will need updating to reflect Windows/Linux/Android/iOS as the new target set once DiaSDL ships |
| PD-006 VS project files | Visual Studio project files are source of truth | Compliant — `DiaSDL.vcxproj` + `.vcxproj.filters` created; `DiaSFML.vcxproj` deleted; solution updated |
| PD-007 C++20 required | `/std:c++20` for all projects | Compliant — SDL3 C headers compile cleanly under C++20; `DiaSDL.vcxproj` inherits standard from `Directory.Build.props` |
| PD-008 Directory.Build.props owns OutDir/IntDir | No per-project output path overrides | Compliant — `DiaSDL.vcxproj` is a static library type; inherits `bin/sharedlibs/` path automatically |
| PD-009 Generated output under `Cluiche/out/` | Non-binary output under `Cluiche/out/<App>/` | Compliant — DiaSDL generates no build-time output artefacts |
| PD-010 `.diagame` routes loading | Project discovery via `.diagame` typed imports | Not applicable — DiaSDL is a platform module, not a game content loader |
| AD-001 Module YAML frontmatter | Each module has `dia.*.architecture.module.md` | Compliant — `dia.sdl.architecture.module.md` created in T-05 (AC-12) |
| AD-002 No STL in public APIs | Reinforces PD-004 | Compliant — see PD-004 |
| AD-003 Namespace `Dia::<Module>::` | Namespace convention | Compliant — namespace `Dia::SDL` |
| AD-004 ProcessingUnit/Phase/Module | Reinforces PD-002 | Compliant — see PD-002 |

---

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | AC-07 SDL lifecycle | Should `SDL_Init`/`SDL_Quit` live in a new `SDLModule` inside DiaSDL, or be the responsibility of the application's existing KernelModule? | `SDL_Init`/`SDL_Quit` live in `DiaSDL::WindowFactory::Create`/`Destroy` — mirrors the SFML pattern where no global init exists; no SDLModule needed. |
| 2 | AC-06 SystemHandle Linux | What is the correct Linux `SystemHandle` type — `unsigned long` (X11 `Window`) or `void*` (Wayland `wl_surface*`)? SDL3 supports both. | `void*` — covers both X11 and Wayland; matches Android precedent in `SystemHandle.h`. Comment in the typedef will note both use cases. |
| 3 | T-06 EKey lookup table | Should the SDL3 → EKey lookup table live in DiaSDL (private) or be exposed as a utility in DiaInput for future backends? | Private to DiaSDL — DiaSDL is the only SDL3 backend; DiaInput must not depend on SDL3 headers. |
| 4 | AC-05 Dead EType removal | If `kMouseWheelMoved`, `kMouseEntered`, `kMouseLeft` have no consumers, should they be removed from the enum or kept as reserved/future entries? | Remove them — dead enum values with no producer or consumer are noise; re-add with a real mapping when actually needed. |
| 5 | T-09 DiaSFML deletion | DiaSFML is referenced in `render-backend.md` (non-responsibilities section) and the Dia app spec (Platform Dependencies). Should those references be updated in this feature or in a follow-on doc-cleanup? | Update in this feature — stale references to a deleted module are confusing; patch `render-backend.md` and `dia.md` in the same PR as the deletion (T-09). |
| 6 | Win32WndProcChain in DiaBgfx | After the move (T-01), should `Win32WndProcChain` be placed in `Dia/DiaBgfx/` root or in `Dia/DiaBgfx/Imgui/` alongside `BgfxImGuiBackend`? | `Dia/DiaBgfx/Imgui/` — it is an ImGui input hook and its only consumer is `BgfxImGuiBackend`; moved via PowerShell script. |
| 7 | SDL3 dependency acquisition | How is SDL3 acquired — git submodule under `External/SDL3/`, prebuilt `.lib` checked in, or `dia env setup` downloads it? | Git submodule under `External/SDL3/` — matches SFML's pattern; `dia env setup` orchestrates the build step; consistent with PD-006. |
| 8 | DiaSDL + DiaBgfx coupling | `DiaSDL::Window::GetSystemHandle()` must return an `SDL_Window`-derived native handle for bgfx to consume. On Linux this requires `SDL_GetPointerProperty` with an X11/Wayland-specific key. Should this platform-specific handle extraction live in DiaSDL or be performed by DiaBgfx at attach time? | Extraction in `DiaSDL::Window::GetSystemHandle()` — DiaBgfx must not know about SDL3; `GetSystemHandle()` hides all platform detail behind `SystemHandle`. Consistent with how SFML's `getNativeHandle()` worked. |

---

## Open Questions

- None currently blocking spec approval.
