# Feature Spec: imgui-backend

## Parent System
@docs/specs/systems/dia/render-backend.md

**Research:** @docs/research/render_backend_swap/summary.md

## Status
`Done`

## Summary

Implement `Dia::Bgfx::BgfxImGuiBackend`, a concrete `Dia::ImGui::IImGuiBackend` implementer for the bgfx render path. Restores ImGui debug overlays (DiaVisualDebuggerConsole, future debug panels) when running on `Dia::Bgfx::Canvas` — `canvas-parity` ships with ImGui suppressed by design.

The implementation reuses bgfx's reference ImGui integration (`examples/common/imgui/imgui.cpp` from the bgfx upstream repo, MIT-licensed) staged as part of `bgfx-env-setup` into `External/bgfx/examples/common/imgui/`. We wrap it behind `IImGuiBackend` so the rest of the engine sees the same interface as the SFML path. Platform-side input (Win32 message → `ImGuiIO`) reuses `imgui_impl_win32.cpp` from `External/imgui/backends/`, since SFML still owns the window in Phase 1 and we have direct access to the HWND via `IWindow::GetSystemHandle()`.

After this feature, `BGFX_BACKEND=dx11` runs CluicheTest with full ImGui debug console, layer toggles, and metrics rendering — at parity with the SFML path.

## Problem

`canvas-parity` (the previous feature) intentionally suppresses ImGui calls on the bgfx path because there is no bgfx-side `IImGuiBackend` impl yet. While ImGui is debug-only (`#ifdef DIA_DEBUG`), losing the debug console is a real productivity loss for any further DiaBgfx work. The `diasfml-render-removal` feature (next-next) cannot land until ImGui works on bgfx — otherwise debug builds lose all ImGui-driven tooling permanently.

ImGui needs three things from a backend to function:
1. **Renderer** — convert ImGui's draw lists (`ImDrawData`) into GPU draw calls each frame
2. **Platform** — translate OS events (mouse, keyboard, focus, IME) into `ImGuiIO`
3. **Lifecycle** — `Init` / `Shutdown` / `NewFrame` / `Render` glue

For (1), bgfx upstream ships a complete reference implementation at `examples/common/imgui/imgui.cpp` (~600 lines, MIT-licensed). It is the canonical bgfx+ImGui integration — used by every bgfx example — and produces correct output on D3D11/D3D12/Vulkan/GL without per-backend changes. We integrate it as-vendored, not rewritten.

For (2), Phase 1's window is still SFML-owned. Two viable paths:
- (a) **`imgui_impl_win32.cpp`** from `External/imgui/backends/` reads from the HWND directly (mouse pos via `GetCursorPos`, keyboard via Win32 `WM_KEYDOWN` etc.). We feed it the HWND once at init.
- (b) Continue routing through `Dia::SFML::SFMLImGuiBackend::ProcessEvent` (which converts `sf::Event` to `ImGuiIO`) and call ImGui through that path while bgfx renders.

(a) is cleaner because it decouples ImGui input from SFML entirely — when the SDL migration happens, only the renderer side moves. (b) keeps a fragile dual-input path. Going with (a).

## Goals

- Add `Dia::Bgfx::BgfxImGuiBackend` implementing `Dia::ImGui::IImGuiBackend` (`Init` / `Shutdown` / `NewFrame` / `Render`)
- Vendor bgfx's reference ImGui renderer into `Dia/DiaBgfx/Imgui/` (single `.cpp` + `.h`), unmodified except for namespace wrapping if needed
- Use `imgui_impl_win32.cpp` (from `External/imgui/backends/`) as the platform input layer
- `BgfxImGuiBackend::Init` performs three steps:
  1. `ImGui::CreateContext()` (if not already created by the manager)
  2. `ImGui_ImplWin32_Init(hwnd)` — platform layer
  3. `imguiCreate(...)` — bgfx's renderer init (registers fonts, allocates programs, etc.)
- `BgfxImGuiBackend::NewFrame(dt)` calls `ImGui_ImplWin32_NewFrame()` then `ImGui::NewFrame()`
- `BgfxImGuiBackend::Render()` calls `ImGui::Render()` then dispatches `ImGui::GetDrawData()` to bgfx's `imguiRender(...)` against a dedicated bgfx view id
- `BgfxImGuiBackend::Shutdown` tears down in reverse order
- The bgfx Canvas reserves an extra view id (`mImGuiViewId`) drawn last so ImGui appears on top; passed into `BgfxImGuiBackend::Init`
- Wired into the existing `DiaImGuiManager` registration path: at app wire-up, `BgfxImGuiBackend` is constructed and registered via `Dia::ImGui::SetBackend(&backend)` instead of `SFMLImGuiBackend`
- `canvas-parity`'s ImGui-suppression flag is removed — `Canvas::StartFrame`/`EndFrame` resume calling `Dia::ImGui::NewFrame` / `Render`
- Win32 `WndProc` hook: `imgui_impl_win32.cpp` exports `ImGui_ImplWin32_WndProcHandler` that must be called from the SFML-owned WndProc to capture mouse/keyboard. This requires a small extension on `DiaSFML::Window` to expose a `WndProc` chain hook (see Implementation section)
- `bgfx-env-setup` extended (this feature ships a tiny one-line addition to `deps.json`'s bgfx entry's `stage[]` list to also stage `examples/common/imgui/imgui.cpp` and the IconsFontAwesome / IconsKenney font headers it depends on)

## Non-Goals

- **DiaImGui module restructure** — `Dia::ImGui::IImGuiBackend` and `DiaImGuiManager` are unchanged
- **ImGui docking / multi-viewport** — not enabled; would require extra setup. Single ImGui window context only, same as today
- **Custom font loading** — uses ImGui's default font; debug console ergonomics unchanged from SFML path
- **Replacing `SFMLImGuiBackend`** — that class continues to exist until `diasfml-render-removal` deletes it. This feature only adds the bgfx implementer alongside
- **Win32 message routing redesign** — the WndProc chain is a minimal hook; no broad input-system refactor
- **Sandbox / sub-window ImGui rendering** — ImGui renders to the main canvas only
- **ImGui asserts on bgfx caps** — ImGui pixel-perfect rendering on all bgfx backends is bgfx's reference implementation's responsibility; if D3D12 or Vulkan render with subtle differences, that's accepted

## Public Interfaces

### `Dia::Bgfx::BgfxImGuiBackend`

```cpp
// Dia/DiaBgfx/Imgui/BgfxImGuiBackend.h
#pragma once

#ifdef DIA_DEBUG

#include <DiaImGui/IImGuiBackend.h>
#include <DiaWindow/Interface/SystemHandle.h>

namespace Dia
{
    namespace Bgfx
    {
        class BgfxImGuiBackend : public Dia::ImGui::IImGuiBackend
        {
        public:
            BgfxImGuiBackend();
            ~BgfxImGuiBackend() override;

            // Bgfx-specific bring-up (called by app wire-up before Init).
            // viewId: bgfx view to draw ImGui into — typically reserved last
            //         so ImGui appears on top of the game frame.
            // hwnd:   Win32 window handle (from DiaSFML::Window::GetSystemHandle())
            //         used by imgui_impl_win32 for platform input.
            void Configure(unsigned short viewId, Dia::Window::SystemHandle hwnd);

            // IImGuiBackend
            void Init() override;
            void Shutdown() override;
            void NewFrame(float dt) override;
            void Render() override;

            // Win32 message pump hook — call from the SFML-owned WndProc.
            // Returns true if ImGui consumed the message (caller should not
            // forward to game input). Forwards to ImGui_ImplWin32_WndProcHandler.
            bool OnWin32Message(void* hwnd, unsigned int msg,
                                unsigned long long wparam, long long lparam);

        private:
            Dia::Window::SystemHandle mHwnd;
            unsigned short            mViewId;
            bool                      mInitialised;
        };
    }
}

#endif // DIA_DEBUG
```

### `DiaSFML::Window` extension (WndProc hook)

To deliver Win32 messages to `imgui_impl_win32`, the SFML-owned WndProc needs to call `BgfxImGuiBackend::OnWin32Message` before SFML processes the event. SFML 3 lets you subclass the window via `setWndProc` (... actually SFML doesn't expose this). The viable path: use Win32's `SetWindowLongPtr(hwnd, GWLP_WNDPROC, ...)` to install a thunk that calls ImGui first, then forwards to the original SFML WndProc.

This is a small Win32-specific shim:

```cpp
// Dia/DiaSFML/Win32WndProcChain.h (new, DIA_DEBUG only)
namespace Dia { namespace SFML {

class Win32WndProcChain
{
public:
    using PreHandler = bool (*)(void* hwnd, unsigned int msg,
                                unsigned long long wparam, long long lparam,
                                void* user);

    static void Install(Dia::Window::SystemHandle hwnd, PreHandler handler, void* user);
    static void Uninstall(Dia::Window::SystemHandle hwnd);
};

} }
```

The chain pre-handler returns `true` if the message was consumed (skip forwarding). `BgfxImGuiBackend::Configure` installs itself via `Win32WndProcChain::Install` and uninstalls in `Shutdown`. Lives in `DiaSFML` (it's Win32-specific glue tied to the SFML-owned window) but only compiled in `DIA_DEBUG`.

When the SDL migration happens later, SDL provides `SDL_AddEventWatch` for the same purpose; `Win32WndProcChain` is replaced with an SDL equivalent and `BgfxImGuiBackend::OnWin32Message` becomes `BgfxImGuiBackend::OnSDLEvent`. Out of scope here.

## Implementation

### Files introduced

```
Dia/DiaBgfx/Imgui/
├── BgfxImGuiBackend.h                         NEW
├── BgfxImGuiBackend.cpp                       NEW (calls into vendored renderer)
├── BgfxImGuiRenderer.cpp                      NEW (vendored from bgfx examples/common/imgui/imgui.cpp; light namespacing)
├── BgfxImGuiRenderer.h                        NEW (declares imguiCreate/imguiRender/imguiDestroy entry points)

Dia/DiaSFML/Win32WndProcChain.h                NEW (DIA_DEBUG only)
Dia/DiaSFML/Win32WndProcChain.cpp              NEW (DIA_DEBUG only)
```

### Files modified

```
Dia/DiaBgfx/DiaBgfx.vcxproj{,.filters}
   - Add Imgui/ files
   - Link against External/imgui/imgui*.cpp (already on the include path; matches DiaSFML's existing imgui-sfml linkage)

Dia/DiaSFML/DiaSFML.vcxproj{,.filters}
   - Add Win32WndProcChain.h/.cpp under DIA_DEBUG

Dia/DiaBgfx/Canvas.cpp
   - Allocate mImGuiViewId at Initialize time (last view, drawn on top)
   - Remove canvas-parity's ImGui suppression flag
   - Call Dia::ImGui::NewFrame in StartFrame, Dia::ImGui::Render in EndFrame
   - Pass mImGuiViewId to BgfxImGuiBackend during app wire-up

Cluiche/Cluiche/Main.cpp (or kernel module)
   - When BGFX_BACKEND is active:
     * Construct Dia::Bgfx::BgfxImGuiBackend
     * Call backend.Configure(canvas.GetImGuiViewId(), window.GetSystemHandle())
     * Call Dia::ImGui::SetBackend(&backend)
   - When SFML path is active (during canvas-parity coexistence window):
     * Construct Dia::SFML::SFMLImGuiBackend (existing)
     * Call Dia::ImGui::SetBackend(&backend) (existing)

deps.json (extended by bgfx-env-setup, amended here)
   - bgfx entry's stage[] gains:
     { "from": "examples/common/imgui",     "to": "External/bgfx/examples/common/imgui" }
     { "from": "examples/common/nanovg",    "to": "External/bgfx/examples/common/nanovg" }   (transitive dep of imgui.cpp)
   - sentinel_inputs gains "External/bgfx/examples/common/imgui/imgui.cpp"

Dia/DiaBgfx/dia.bgfx.architecture.module.md
   - public_api.entry_points: add BgfxImGuiBackend
```

### Renderer integration (bgfx upstream)

bgfx's `examples/common/imgui/imgui.cpp` exposes:
- `void imguiCreate(float fontSize = 18.0f);`
- `void imguiDestroy();`
- `void imguiBeginFrame(int32_t mx, int32_t my, uint8_t button, int32_t scroll, uint16_t width, uint16_t height, int inputChar = -1, bgfx::ViewId view = 255);`
- `void imguiEndFrame();`

The wiring:

```cpp
void BgfxImGuiBackend::Init()
{
    // 1. ImGui context
    if (::ImGui::GetCurrentContext() == nullptr)
        ::ImGui::CreateContext();

    // 2. Platform input
    ImGui_ImplWin32_Init(reinterpret_cast<HWND>(mHwnd));

    // 3. bgfx renderer
    imguiCreate(/*fontSize*/ 18.0f);

    // 4. Win32 chain hook (DIA_DEBUG only)
    Win32WndProcChain::Install(mHwnd, &BgfxImGuiBackend::WndProcThunk, this);

    mInitialised = true;
}

void BgfxImGuiBackend::NewFrame(float dt)
{
    ImGui_ImplWin32_NewFrame();      // mouse pos, modifier keys, viewport size
    imguiBeginFrame(/*mx*/ ImGui::GetIO().MousePos.x,
                    /*my*/ ImGui::GetIO().MousePos.y,
                    /*button*/ 0,    // bgfx imguiBeginFrame uses MS imgui io as source
                    /*scroll*/ 0,
                    /*width*/  uint16_t(ImGui::GetIO().DisplaySize.x),
                    /*height*/ uint16_t(ImGui::GetIO().DisplaySize.y),
                    /*inputChar*/ -1,
                    /*viewId*/ mViewId);
    // imguiBeginFrame ends with ImGui::NewFrame() internally
}

void BgfxImGuiBackend::Render()
{
    imguiEndFrame();   // ImGui::Render() + bgfx draw calls into mViewId
}

void BgfxImGuiBackend::Shutdown()
{
    Win32WndProcChain::Uninstall(mHwnd);
    imguiDestroy();
    ImGui_ImplWin32_Shutdown();
    // Don't destroy ImGui context — DiaImGuiManager owns it
    mInitialised = false;
}
```

Note: bgfx's `imguiBeginFrame` signature accepts mouse state from the caller, but `imgui_impl_win32` already pumps it into `ImGui::GetIO()`. The wiring above reads from `ImGui::GetIO()` directly, which works because bgfx's `imguiBeginFrame` uses those values redundantly. In practice we may need to call `ImGui::NewFrame()` ourselves and skip `imguiBeginFrame`'s internal NewFrame; final wiring is finalised in implementation. The interface contract (`Init`/`NewFrame`/`Render`/`Shutdown`) stays the same regardless.

### WndProc chain detail

bgfx's `examples/common/imgui/imgui.cpp` does not handle Win32 input on its own — it expects the host app to use `imgui_impl_win32.cpp`. The WndProc chain in DiaSFML hooks into SFML's window:

```cpp
// Dia/DiaSFML/Win32WndProcChain.cpp (DIA_DEBUG only)
static WNDPROC g_originalWndProc = nullptr;
static Win32WndProcChain::PreHandler g_preHandler = nullptr;
static void* g_preUser = nullptr;

LRESULT CALLBACK ChainProc(HWND hwnd, UINT msg, WPARAM w, LPARAM l)
{
    if (g_preHandler && g_preHandler(hwnd, msg, w, l, g_preUser))
        return 0;  // consumed
    return CallWindowProc(g_originalWndProc, hwnd, msg, w, l);
}

void Win32WndProcChain::Install(SystemHandle hwnd, PreHandler h, void* user)
{
    g_preHandler = h;
    g_preUser = user;
    g_originalWndProc = (WNDPROC)SetWindowLongPtr(
        (HWND)hwnd, GWLP_WNDPROC, (LONG_PTR)ChainProc);
}

void Win32WndProcChain::Uninstall(SystemHandle hwnd)
{
    if (g_originalWndProc)
        SetWindowLongPtr((HWND)hwnd, GWLP_WNDPROC, (LONG_PTR)g_originalWndProc);
    g_originalWndProc = nullptr;
    g_preHandler = nullptr;
    g_preUser = nullptr;
}
```

`BgfxImGuiBackend::WndProcThunk` (a static method) calls `ImGui_ImplWin32_WndProcHandler(hwnd, msg, w, l)` and returns whatever that handler indicates.

### Tradeoff summary

This feature trades:
- **Vendoring bgfx's reference imgui renderer** (~600 LOC of upstream code under `External/bgfx/examples/common/imgui/`) — accepted because rewriting it is a poor use of time when the upstream impl is well-tested and MIT-licensed
- **A small Win32-specific shim in DiaSFML** (`Win32WndProcChain.cpp`, ~30 LOC) — accepted because the SDL migration replaces this anyway; until then it's the cleanest input route
- **Phase 1 retains a Win32 dependency in DiaSFML** — was true already (HWND retrieval); this just adds a hook

## Files Introduced / Modified

| File | Change |
|------|--------|
| `Dia/DiaBgfx/Imgui/BgfxImGuiBackend.h / .cpp` | NEW — IImGuiBackend impl + Win32 message thunk |
| `Dia/DiaBgfx/Imgui/BgfxImGuiRenderer.h / .cpp` | NEW — vendored from bgfx upstream `examples/common/imgui/imgui.cpp` |
| `Dia/DiaBgfx/DiaBgfx.vcxproj{,.filters}` | Add Imgui/ files |
| `Dia/DiaSFML/Win32WndProcChain.h / .cpp` | NEW (DIA_DEBUG only) — Win32 WndProc chain shim |
| `Dia/DiaSFML/DiaSFML.vcxproj{,.filters}` | Add Win32WndProcChain under DIA_DEBUG |
| `Dia/DiaBgfx/Canvas.cpp` | Reserve ImGui view id, remove suppression flag, resume ImGui calls |
| `Cluiche/Cluiche/Main.cpp` | Backend selection: SFMLImGuiBackend vs BgfxImGuiBackend based on BGFX_BACKEND |
| `deps.json` (root) | Extend bgfx entry's `stage[]` to include `examples/common/imgui/` and `examples/common/nanovg/` |
| `Dia/DiaBgfx/dia.bgfx.architecture.module.md` | Add BgfxImGuiBackend to entry_points |

## Dependencies

| Dependency | Type | Notes |
|------------|------|-------|
| `bgfx-env-setup` (Approved) | Hard | Stages bgfx upstream's `examples/common/imgui/` (this feature amends `deps.json` to do so) |
| `canvas-parity` (Approved) | Hard | `Bgfx::Canvas` exists; ImGui view id reservation hooks in here |
| `debug-console` (Approved, DiaVisualDebugger) | Soft | The debug console consumes ImGui; this feature unblocks it on bgfx |
| `diasfml-render-removal` | Reverse | That feature deletes `SFMLImGuiBackend` after this feature confirms ImGui works on bgfx |

## Acceptance Criteria

1. `Dia::Bgfx::BgfxImGuiBackend` implements `Dia::ImGui::IImGuiBackend`'s four pure-virtual methods
2. `BgfxImGuiBackend` is `#ifdef DIA_DEBUG`-guarded; not present in Release builds
3. bgfx's reference ImGui renderer is staged into `External/bgfx/examples/common/imgui/imgui.cpp` by `dia env setup --dep bgfx` (verified by amending the bgfx entry's `stage[]` and `sentinel_inputs`)
4. `imgui_impl_win32.cpp` from `External/imgui/backends/` is added to `DiaBgfx.vcxproj` (DIA_DEBUG only)
5. `Dia::SFML::Win32WndProcChain` exists, installs a Win32 WndProc thunk via `SetWindowLongPtr`, and uninstalls cleanly on shutdown (no crash on app close)
6. With `BGFX_BACKEND=dx11`, `dia run cluichetest` shows the DiaVisualDebuggerConsole (ImGui window) at parity with the SFML path: layer toggles work, metrics display, log tail scrolls
7. ImGui mouse hover / click / keyboard input is responsive (verified by clicking a layer toggle in the visual debugger console)
8. ImGui drawn on top of game frame (after sprite + debug + UI overlay views), via the dedicated `mImGuiViewId` reserved by `Canvas::Initialize`
9. `Bgfx::Canvas`'s ImGui-suppression flag (added in `canvas-parity`) is removed; `StartFrame` / `EndFrame` resume calling `Dia::ImGui::NewFrame` / `Render`
10. App wire-up (`Main.cpp`) selects the correct backend based on `BGFX_BACKEND`: `BgfxImGuiBackend` for bgfx, `SFMLImGuiBackend` for SFML (preserved during the `canvas-parity` ↔ `diasfml-render-removal` coexistence window)
11. ImGui consumes Win32 messages it cares about (mouse, keyboard) and **does not** double-deliver them to the game input layer (verified: pressing a key while the ImGui window has focus does not trigger game input)
12. ImGui input falls back to the game when no ImGui window is focused (verified: WASD movement works when not hovering the debug console)
13. `dia run googletest` is green; no test changes required (ImGui is a runtime overlay, not unit-tested)
14. Build clean under `/std:c++20` for both Debug (with ImGui) and Release (without — guard not violated)
15. `dia.bgfx.architecture.module.md` lists `BgfxImGuiBackend` in `public_api.entry_points`
16. Frame time impact: enabling DiaVisualDebuggerConsole adds ≤1 ms to median frame time on dx11 (matches SFML baseline impact)

## Traceability

| Level | Spec | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System (primary) | RenderBackend | @docs/specs/systems/dia/render-backend.md |

## Binding Decisions Compliance

| ID | Source | Decision | Compliance |
|----|--------|----------|------------|
| PD-001 | Platform | StringCRC for all entity/component IDs | N/A — ImGui backend has no identifier surface |
| PD-002 | Platform | ProcessingUnit/Phase/Module architecture | Compliant — backend lives inside Render PU lifecycle, mirrors SFMLImGuiBackend |
| PD-003 | Platform | Component-based entities | N/A |
| PD-004 | Platform | No STL containers in public APIs | Compliant — `BgfxImGuiBackend` public surface uses `unsigned short`, `Window::SystemHandle`, raw types; no STL. Vendored renderer (`External/bgfx/examples/common/imgui/imgui.cpp`) uses STL internally — acceptable per the existing pattern (DiaSFML's imgui-sfml integration follows the same precedent for vendored UI code) |
| PD-005 | Platform | x64 only | Compliant — Win32 thunk is x64-clean (`SetWindowLongPtr`, not `SetWindowLong`) |
| PD-006 | Platform | Visual Studio project files are source of truth | Compliant — vendored files added to `.vcxproj{,.filters}` manually |
| PD-007 | Platform | C++20 required | Compliant — code under `/std:c++20`; vendored bgfx imgui renderer compiles cleanly under C++20 |
| PD-008 | Platform | `Directory.Build.props` owns OutDir/IntDir | Compliant |
| PD-009 | Platform | Generated output under `Cluiche/out/<AppName>/` | N/A — no generated output |
| PD-010 | Platform | `.diagame` typed imports | N/A |
| AD-001 | Dia App | Module YAML frontmatter | Compliant — `dia.bgfx.architecture.module.md` updated |
| AD-002 | Dia App | No STL in public APIs | Reinforces PD-004 |
| AD-003 | Dia App | Namespace `Dia::<Module>::` | Compliant — `Dia::Bgfx::BgfxImGuiBackend`; `Dia::SFML::Win32WndProcChain` |
| AD-004 | Dia App | ProcessingUnit/Phase/Module | Reinforces PD-002 |
| AD-005 | Dia App | Component-based entities | N/A |
| RB-002 | RenderBackend | Two-phase delivery | Compliant — Phase 1 |
| RB-004 | RenderBackend | Canvas implements ICanvas only | Reinforces — `BgfxImGuiBackend` is separate from `Canvas` |
| RB-005 | RenderBackend | No visitor pattern in production render path | N/A |
| RB-006 | RenderBackend | No backend types in DiaGraphics public surface | Compliant — `bgfx::ViewId` (alias of `unsigned short`) in `BgfxImGuiBackend::Configure` is a `Dia::Bgfx::` surface, not DiaGraphics |
| RB-016 | RenderBackend | Phase 1 ship gate: ImGui regression-free | **Compliant — this feature is the ImGui regression-free guarantee** |
| SD-DBG-002 | DiaVisualDebugger | `#ifdef DIA_DEBUG` guards all debug draw code | Compliant — `BgfxImGuiBackend` and `Win32WndProcChain` both `#ifdef DIA_DEBUG` |
| SD-DBG-011 | DiaVisualDebugger | ImGui (MIT) approved as new external dependency | Compliant — already in External; this feature reuses it |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Vendoring strategy | Do we vendor bgfx's `imgui.cpp` into our repo, or stage it from `External/bgfx/`? | Stage from `External/bgfx/examples/common/imgui/` — `bgfx-env-setup` clones bgfx anyway; we just stage two more directories. Avoids duplicated source in our repo. The .vcxproj references `External/bgfx/examples/common/imgui/imgui.cpp` directly. |
| 2 | Win32 dependency in DiaSFML | Adding a Win32-specific shim to DiaSFML feels regressive. Is it justified? | Yes — DiaSFML already exposes `IWindow::GetSystemHandle()` which returns a Win32 HWND. The shim is small, debug-only, and replaced when SDL takes over the window. The alternative (rewriting bgfx's imgui_impl_win32) is much worse. |
| 3 | NanoVG transitive dep | Does bgfx's imgui.cpp depend on `nanovg`? | Inspect: `examples/common/imgui/imgui.cpp` uses NanoVG headers in its render path for some auxiliary widgets. Stage `examples/common/nanovg/` alongside; if not actually needed, drop in implementation step. Captured in deps.json amendment. |
| 4 | Mouse vs keyboard routing | What if the user clicks in the game viewport while ImGui demo window is open? | `imgui_impl_win32` populates `ImGui::GetIO().WantCaptureMouse`. Game input layer reads that flag and skips mouse processing when ImGui wants the mouse. Same pattern as SFML path. The chain returns true (consumed) only for messages ImGui wants exclusively. |
| 5 | DiaImGuiManager re-entrance | The manager already exists with SFML backend registered. What happens when bgfx tries to register too? | `DiaImGuiManager::SetBackend` replaces the current backend pointer. App wire-up sets the bgfx backend before calling `Init`. Old SFML backend's `Shutdown` should have already been called (or not — only one backend is ever live at a time during the coexistence window because `BGFX_BACKEND` selects one path). Captured in `Main.cpp` wire-up. |
| 6 | Coexistence | When `BGFX_BACKEND` is unset (SFML path), does `BgfxImGuiBackend` link in but stay dormant? | Yes — `DiaBgfx.lib` is linked unconditionally; only the runtime path branches. Build-time conditional inclusion is overcomplicated for a 600-LOC backend. |
| 7 | Render thread safety | bgfx's imgui.cpp issues bgfx draw calls. Is that safe from any thread? | bgfx draw submission is API-thread (Render PU) only. `BgfxImGuiBackend::Render` runs on the Render PU same as `Canvas::EndFrame`. Same threading constraint, no issue. |
| 8 | Hot reload | If a developer rebuilds DiaBgfx.dll, does ImGui state survive? | Today neither SFML nor bgfx supports DLL hot-reload; this feature doesn't change that. ImGui state is full lost on any rebuild — same as today. |
| 9 | Font loading | bgfx's imgui.cpp loads ProggyClean (default ImGui font). Is that the same as SFML path? | Yes — both end up using ImGui's default font. Visually identical. If a custom font is needed later (icon fonts, larger text), wire it through `imguiCreate(fontSize)`'s parameter. |
| 10 | View id allocation | Canvas reserves three view ids in `canvas-parity` (entity/debug/UIoverlay). ImGui needs a fourth. Does the order matter? | Yes — ImGui must draw last (highest view id). bgfx draws views in order. `Canvas::Initialize` assigns: entity=0, debug=1, UI=2, ImGui=3. Captured in `canvas-parity`'s implementation; this feature confirms it works. |
| 11 | Manager lifetime | `DiaImGuiManager` is a process-global. If two `Bgfx::Canvas` instances exist (multi-swapchain), do they share one ImGui? | Yes — single ImGui context per process. ImGui doesn't natively support multiple canvases / multiple windows without docking/multi-viewport (which is non-goal). For multi-canvas, ImGui renders into the *primary* canvas only; secondary canvases get no ImGui overlay. Documented as a Phase 1 limitation. |
| 12 | Phase 1 ship gate | Does this feature complete RB-016's "ImGui regression-free" requirement? | Yes — this feature is exactly the ImGui-on-bgfx parity work. After it lands, `diasfml-render-removal` can delete `SFMLImGuiBackend` confidently. |
| 13 | imgui_impl_win32 license | License-compatible? | MIT (`External/imgui/LICENSE.txt`) — same as the rest of imgui. Compatible with our repo. |
| 14 | Skipping `imguiBeginFrame` | The wiring in Implementation calls both `ImGui_ImplWin32_NewFrame` and `imguiBeginFrame`. Does that double-call `ImGui::NewFrame`? | Possible bug — bgfx's `imguiBeginFrame` ends with `ImGui::NewFrame()` internally. Calling it after `ImGui_ImplWin32_NewFrame` is correct (Win32_NewFrame just updates IO state). Final wiring confirmed during implementation by reading bgfx's example main loop. |

---
