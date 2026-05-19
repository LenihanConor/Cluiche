# Feature Spec: render-overlay-surface

## Parent System
@docs/specs/systems/dia/render-backend.md

**Modifies module:** `Dia/DiaUI/` (no DiaUI system spec exists yet — this feature is owned by RenderBackend per RB-008)

**Research:** @docs/research/render_backend_swap/summary.md

## Status
`Approved`

## Summary

Extract the renderer-agnostic UI overlay path out of `Dia::SFML::RenderWindow` and into `Dia::UI` as a new abstract surface `IUIRenderOverlay`. After this feature:

- `Dia::UI::IUIRenderOverlay` is the contract any renderer implements to composite a per-frame `UIDataBuffer` over its backbuffer
- A new `Dia::SFML::SfmlUIRenderOverlay` class implements `IUIRenderOverlay` and contains the existing logic that lives inside `RenderWindow::EndFrame` today (the `mUIShader` / `mUIOverlayTexture` / `pushGLStates` / `popGLStates` flow)
- `Dia::SFML::RenderWindow::ProcessFrame`/`EndFrame` calls into the new class instead of inlining the SFML UI shader code
- `Dia::SFML::RenderWindow` no longer holds `sf::Shader* mUIShader` or `sf::Texture* mUIOverlayTexture` directly; `SfmlUIRenderOverlay` owns them
- `DiaBgfx::BgfxUIRenderOverlay` will implement the same `IUIRenderOverlay` interface in the `diabgfx-canvas-parity` feature — this feature only defines and validates the seam in DiaSFML

This is a pre-bgfx refactor: it produces no behaviour change at runtime; its job is to cut the seam cleanly so that DiaBgfx can plug into it later. After this lands, the UI overlay path is *renderer-agnostic at the type level*, and the existing UI behaviour is unchanged.

## Problem

Today, `Dia::SFML::RenderWindow` (a class that already conflates `ICanvas` + `IWindow` + `IInputSource` per RB-004) inlines the UI compositing pipeline inside `EndFrame`:

```cpp
// RenderWindow.cpp lines 215–250 (excerpt)
sf::Sprite uiSprite(*mUIOverlayTexture);
if (nextFrame.GetUIData().GetBufferSize() > 0) {
    mUIOverlayTexture->update(nextFrame.GetUIData().GetBuffer());
    // ...debug save-to-png hatch...
}
mWindowContext->pushGLStates();
mWindowContext->draw(uiSprite, mUIShader);
mWindowContext->popGLStates();
```

`mUIShader`, `mUIOverlayTexture`, the `ui.frag` shader file lookup, and the GL state push/pop are all renderer-specific implementation details that today live in the `RenderWindow` class. When `DiaBgfx::Canvas` ships in `diabgfx-canvas-parity`, it needs an equivalent UI compositing path — there is no abstraction to plug into; either we copy-paste the structure into `DiaBgfx::Canvas`, or we extract a seam.

Per RB-008 (system-spec binding decision), the seam is extracted into `DiaUI` so that future renderer modules (`DiaBgfx`, and potentially `DiaUICEF`'s direct-render variant if it ever ships) can share a single contract. This keeps the UI compositing concept in DiaUI (where it conceptually lives) and the renderer-side implementations as small adapters.

## Goals

- Define `Dia::UI::IUIRenderOverlay` — a renderer-agnostic abstract interface for compositing a `UIDataBuffer` onto a backbuffer
- Implement `Dia::SFML::SfmlUIRenderOverlay` that holds and operates the existing `sf::Shader` + `sf::Texture` state and reproduces today's UI compositing exactly
- `Dia::SFML::RenderWindow` constructs a `SfmlUIRenderOverlay`, hands it the `sf::RenderTexture` backbuffer pointer in `Initialize`, and calls `Composite(uiBuffer)` from `EndFrame`
- `RenderWindow` no longer holds `mUIShader` or `mUIOverlayTexture` member fields — those move to `SfmlUIRenderOverlay`
- The shader file lookup (`global/Presentation/ui.frag`) moves into `SfmlUIRenderOverlay`'s constructor; the path is unchanged
- The hidden `debugUIRendertexture` save-to-png hatch is preserved (kept inside the new class) so debug-trace behaviour does not change
- All existing tests continue to pass (`dia run googletest`)
- DummyStage UI overlay renders identically when run via `dia run cluichetest`

## Non-Goals

- **bgfx implementation** — `DiaBgfx::BgfxUIRenderOverlay` belongs in `diabgfx-canvas-parity`, not here
- **UI shader source change** — the existing `ui.frag` shader is unchanged; only its loader moves
- **`UIDataBuffer` API changes** — the type and its accessors are unchanged
- **Multi-page UI overlay** — `IUIRenderOverlay` accepts one `UIDataBuffer` per frame, matching today's contract
- **DiaUICEF / DiaUIUltralight refactor** — those modules use their own off-screen rendering paths (CEF SharedTextureWrapper, Ultralight's GPU surface) and are out of scope. They may opt into `IUIRenderOverlay` later if useful, but that's a separate decision
- **GL state hygiene improvement** — the current `pushGLStates`/`popGLStates` is preserved verbatim; cleaning that up is a future maintenance task
- **ImGui rendering** — ImGui has its own backend (`SFMLImGuiBackend` today, `BgfxImGuiBackend` in `diabgfx-imgui-backend`); not part of this seam

## Public Interfaces

### `Dia::UI::IUIRenderOverlay` (new)

```cpp
// Dia/DiaUI/IUIRenderOverlay.h
#pragma once

#include <DiaMaths/Vector/Vector2D.h>

namespace Dia
{
    namespace UI
    {
        class UIDataBuffer;

        ///
        /// Renderer-agnostic UI overlay surface.
        ///
        /// A renderer module (DiaSFML, DiaBgfx, etc.) implements this to composite
        /// a per-frame UI buffer over its backbuffer. The renderer constructs the
        /// concrete impl during canvas init and calls Composite() once per frame
        /// from its EndFrame() (or equivalent post-pass) hook.
        ///
        /// Thread model: created and called on the render thread only.
        ///
        class IUIRenderOverlay
        {
        public:
            virtual ~IUIRenderOverlay() = default;

            ///
            /// Resize internal overlay surface to match the canvas size.
            /// Called when the canvas size changes (window resize or initial
            /// layout). May reallocate GPU resources.
            ///
            virtual void OnCanvasSizeChanged(const Dia::Maths::Vector2D& size) = 0;

            ///
            /// Composite the UI buffer over the current backbuffer.
            /// Called once per frame after the entity + debug passes.
            ///
            /// The UIDataBuffer's lifetime is owned by the FrameData; the
            /// implementation must consume it within this call and may not
            /// retain pointers to its contents.
            ///
            /// If buffer.GetBufferSize() == 0, the implementation should
            /// composite the previously-uploaded overlay (preserves the
            /// today-behaviour where an empty UI buffer continues to show
            /// the last drawn UI).
            ///
            virtual void Composite(const UIDataBuffer& buffer) = 0;

        protected:
            IUIRenderOverlay() = default;

        private:
            IUIRenderOverlay(const IUIRenderOverlay&) = delete;
            IUIRenderOverlay& operator=(const IUIRenderOverlay&) = delete;
        };
    }
}
```

### `Dia::SFML::SfmlUIRenderOverlay` (new)

```cpp
// Dia/DiaSFML/SfmlUIRenderOverlay.h
#pragma once

#include <DiaUI/IUIRenderOverlay.h>

namespace sf
{
    class RenderWindow;
    class RenderTexture;
    class Shader;
    class Texture;
}

namespace Dia
{
    namespace SFML
    {
        class SfmlUIRenderOverlay : public Dia::UI::IUIRenderOverlay
        {
        public:
            // Caller passes the render-window context (final present target)
            // and the backbuffer texture (offscreen colour target).
            SfmlUIRenderOverlay(sf::RenderWindow* windowContext, sf::RenderTexture* backBuffer);
            ~SfmlUIRenderOverlay() override;

            void OnCanvasSizeChanged(const Dia::Maths::Vector2D& size) override;
            void Composite(const Dia::UI::UIDataBuffer& buffer) override;

        private:
            sf::RenderWindow*  mWindowContext;   // not owned
            sf::RenderTexture* mBackBuffer;      // not owned
            sf::Shader*        mUIShader;        // owned
            sf::Texture*       mUIOverlayTexture;// owned
        };
    }
}
```

### `Dia::SFML::RenderWindow` (modified)

The `mUIShader` and `mUIOverlayTexture` members move into `SfmlUIRenderOverlay`. `RenderWindow` gains:

```cpp
class RenderWindow : public Graphics::ICanvas, public Window::IWindow, public InputSource
{
    // ... existing members ...
private:
    SfmlUIRenderOverlay* mUIRenderOverlay;  // owned; allocated in ctor, deleted in dtor
};
```

The `EndFrame` UI block is replaced with:

```cpp
if (mUIRenderOverlay != nullptr)
{
    mUIRenderOverlay->Composite(nextFrame.GetUIData());
}
```

`SetCanvasSize` and the equivalent IWindow `SetSize` call propagate to `mUIRenderOverlay->OnCanvasSizeChanged(size)`.

## Implementation

### Files introduced

```
Dia/DiaUI/IUIRenderOverlay.h            NEW — abstract interface
Dia/DiaSFML/SfmlUIRenderOverlay.h       NEW — concrete impl
Dia/DiaSFML/SfmlUIRenderOverlay.cpp     NEW — moves UI logic from RenderWindow.cpp
```

### Files modified

```
Dia/DiaSFML/RenderWindow.h
   - Remove sf::Shader* mUIShader and sf::Texture* mUIOverlayTexture members
   - Add SfmlUIRenderOverlay* mUIRenderOverlay
   - Forward-declare class SfmlUIRenderOverlay; remove sf::Shader / sf::Texture forward decls if no other use

Dia/DiaSFML/RenderWindow.cpp
   - Constructor: replace direct mUIShader / mUIOverlayTexture allocation with mUIRenderOverlay = DIA_NEW(SfmlUIRenderOverlay(mWindowContext, mBackBuffer))
   - Constructor: remove the ui.frag FilePath::Resolve + loadFromFile block (moves to SfmlUIRenderOverlay ctor)
   - Destructor: replace dual DIA_DELETE with DIA_DELETE(mUIRenderOverlay)
   - SetCanvasSize/SetSize: forward to mUIRenderOverlay->OnCanvasSizeChanged
   - EndFrame: replace lines 217–238 with mUIRenderOverlay->Composite(nextFrame.GetUIData())
   - Remove #include <SFML/Graphics.hpp> direct mUIShader/mUIOverlayTexture usage if redundant

Dia/DiaUI/DiaUI.vcxproj{,.filters}
   - Add IUIRenderOverlay.h to project (header-only file under Public Headers)

Dia/DiaSFML/DiaSFML.vcxproj{,.filters}
   - Add SfmlUIRenderOverlay.h and SfmlUIRenderOverlay.cpp

Dia/DiaUI/dia.ui.architecture.module.md
   - public_api.headers: add Dia/DiaUI/IUIRenderOverlay.h
   - public_api.entry_points: add IUIRenderOverlay
   - dependencies.required: add dia.maths.vector (already transitive via DiaCore? — verify; add if missing)

Dia/DiaSFML/dia.sfml.architecture.module.md
   - public_api.entry_points: add SfmlUIRenderOverlay
   - dependencies.required: add dia.ui (already present? — verify)
```

### Migration order

1. Add `IUIRenderOverlay.h` to DiaUI. Build (header-only addition; no impact yet).
2. Add `SfmlUIRenderOverlay.h/.cpp` to DiaSFML with logic copied verbatim from `RenderWindow.cpp`. Build.
3. Modify `RenderWindow` to own a `SfmlUIRenderOverlay*` and delegate to it. Remove the inline UI block from `EndFrame`. Remove the now-orphaned member fields. Build + run.
4. Validate visually: `dia run cluichetest` — DummyStage UI overlay renders identically. Compare against pre-feature screenshot.
5. Run `dia run googletest` — all green.
6. Update both architecture module markdowns.

### What does **not** change

- The `ui.frag` shader file location and contents (still `global/Presentation/ui.frag`)
- The two shader uniforms (`uiOverlayTex`, `backBufferTex`)
- The `pushGLStates`/`popGLStates` discipline
- The `debugUIRendertexture` save-to-png debug hatch
- `UIDataBuffer`'s API
- The order: entity pass → debug pass → UI overlay pass → ImGui render → window present

The intent is *exact behavioural equivalence*. This is a structural move only.

## Files Introduced / Modified

| File | Change |
|------|--------|
| `Dia/DiaUI/IUIRenderOverlay.h` | NEW |
| `Dia/DiaUI/DiaUI.vcxproj{,.filters}` | Add header to project |
| `Dia/DiaUI/dia.ui.architecture.module.md` | public_api updated |
| `Dia/DiaSFML/SfmlUIRenderOverlay.h` | NEW |
| `Dia/DiaSFML/SfmlUIRenderOverlay.cpp` | NEW (moved logic) |
| `Dia/DiaSFML/DiaSFML.vcxproj{,.filters}` | Add new files |
| `Dia/DiaSFML/RenderWindow.h` | Members reshaped (remove mUIShader/mUIOverlayTexture; add mUIRenderOverlay) |
| `Dia/DiaSFML/RenderWindow.cpp` | Body refactor — UI block delegated to overlay |
| `Dia/DiaSFML/dia.sfml.architecture.module.md` | public_api updated |

## Dependencies

| Dependency | Type | Notes |
|------------|------|-------|
| `bgfx-env-setup` (Approved) | None | Independent of bgfx; no bgfx code in this feature |
| `texture-handle-stringcrc` (Approved) | None | Independent — UI overlay uses `UIDataBuffer` which is not a texture asset |
| `diabgfx-canvas-parity` | Reverse | This feature is a prerequisite for `diabgfx-canvas-parity` (which adds `BgfxUIRenderOverlay`) |
| `diasfml-render-removal` | Reverse | When DiaSFML render path is deleted, `SfmlUIRenderOverlay` is deleted along with `RenderWindow`'s ICanvas surface |

## Acceptance Criteria

1. `Dia::UI::IUIRenderOverlay` exists in `Dia/DiaUI/IUIRenderOverlay.h`, declares the two pure-virtual methods specified above, and compiles standalone (no SFML / bgfx headers required)
2. `Dia::SFML::SfmlUIRenderOverlay` implements `IUIRenderOverlay` and contains the `sf::Shader*`, `sf::Texture*`, and the `ui.frag` loader code that previously lived in `RenderWindow.cpp`
3. `Dia::SFML::RenderWindow` no longer declares or holds `sf::Shader* mUIShader` or `sf::Texture* mUIOverlayTexture` member fields; instead holds `SfmlUIRenderOverlay* mUIRenderOverlay`
4. `RenderWindow::EndFrame`'s UI compositing block (the lines from `sf::Sprite uiSprite` through `popGLStates`) is replaced with a single delegating call to `mUIRenderOverlay->Composite(nextFrame.GetUIData())`
5. `RenderWindow::SetCanvasSize` and `IWindow::SetSize` (where applicable) forward to `mUIRenderOverlay->OnCanvasSizeChanged`
6. `dia run cluichetest` renders DummyStage's UI overlay identically to the pre-feature behaviour (manual visual diff acceptable)
7. `dia run googletest` is green; no test changes required (no public API removed; only internal field layout changed)
8. The hidden `debugUIRendertexture` save-to-png debug hatch still works (preserved inside `SfmlUIRenderOverlay`)
9. `dia.ui.architecture.module.md` lists `IUIRenderOverlay` in `public_api.entry_points`
10. `dia.sfml.architecture.module.md` lists `SfmlUIRenderOverlay` in `public_api.entry_points`
11. Build is clean under `/std:c++20` with zero new warnings
12. Module dependency graph still validates: `python Tools/dia_modules.py --validate` passes (DiaSFML already depends on DiaUI; no new edge introduced)
13. `grep -rn "mUIShader\|mUIOverlayTexture" Dia/DiaSFML/RenderWindow.*` returns zero matches after the refactor
14. The `ui.frag` shader file is loaded exactly once per `SfmlUIRenderOverlay` lifetime (same as today)

## Traceability

| Level | Spec | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System (primary) | RenderBackend | @docs/specs/systems/dia/render-backend.md |
| Module touched | DiaUI | (no system spec yet — module YAML at `Dia/DiaUI/dia.ui.architecture.module.md`) |
| Module touched | DiaSFML | (no system spec yet — module YAML at `Dia/DiaSFML/dia.sfml.architecture.module.md`) |

## Binding Decisions Compliance

| ID | Source | Decision | Compliance |
|----|--------|----------|------------|
| PD-001 | Platform | StringCRC for all entity/component IDs | N/A — UI overlay does not introduce identifiers |
| PD-002 | Platform | ProcessingUnit/Phase/Module architecture | Compliant — no PU/Phase changes; `Composite` runs in the render PU's existing EndFrame slot |
| PD-003 | Platform | Component-based entities | N/A — UI overlay is below the entity layer |
| PD-004 | Platform | No STL containers in public APIs | Compliant — `IUIRenderOverlay` interface uses `UIDataBuffer&` and `Maths::Vector2D`; no STL exposed. `SfmlUIRenderOverlay` private impl may use STL freely |
| PD-005 | Platform | x64 only | Compliant — no platform-specific code; bit-width-neutral |
| PD-006 | Platform | Visual Studio project files are source of truth | Compliant — `IUIRenderOverlay.h` added to `DiaUI.vcxproj{,.filters}`; `SfmlUIRenderOverlay.{h,cpp}` added to `DiaSFML.vcxproj{,.filters}` |
| PD-007 | Platform | C++20 required | Compliant — uses `= default`, `= delete`, no obscure features |
| PD-008 | Platform | `Directory.Build.props` owns OutDir/IntDir | Compliant — no per-project overrides |
| PD-009 | Platform | Generated output under `Cluiche/out/<AppName>/` | N/A — no generated output |
| PD-010 | Platform | `.diagame` typed imports | N/A — no manifest changes |
| AD-001 | Dia App | Module YAML frontmatter documentation | Compliant — both `dia.ui.architecture.module.md` and `dia.sfml.architecture.module.md` updated |
| AD-002 | Dia App | No STL containers in public APIs | Reinforces PD-004 |
| AD-003 | Dia App | Namespace `Dia::<Module>::` | Compliant — `Dia::UI::IUIRenderOverlay`, `Dia::SFML::SfmlUIRenderOverlay` |
| AD-004 | Dia App | ProcessingUnit/Phase/Module for app structure | N/A |
| AD-005 | Dia App | Component-based entities | N/A |
| RB-002 | RenderBackend | Two-phase delivery: parity first, light 3D after | Compliant — this feature lives in Phase 1 (parity) |
| RB-004 | RenderBackend | DiaBgfx::Canvas implements ICanvas only | Reinforces — by extracting UI overlay out of RenderWindow, the path that DiaBgfx::Canvas needs to implement is also clean |
| RB-008 | RenderBackend | UI overlay path extracted into renderer-agnostic IRenderOverlay in DiaUI | **Compliant — this feature is the implementation of RB-008** |
| RB-016 | RenderBackend | Phase 1 ship gate: SFML render path deletable, ImGui regression-free, UI overlay regression-free | Compliant — preserves UI overlay behaviour exactly so the ship gate is not threatened |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Naming | Why `IUIRenderOverlay` instead of `IRenderOverlay` (as referenced in the system spec RB-008)? | Renamed for clarity. The system spec used a working name; "UIRenderOverlay" is more specific because it's specifically the *UI* overlay — distinct from "render overlay" in general (which could be misread as debug overlays etc.). The system spec's RB-008 description is updated implicitly by this feature's choice; no spec amendment needed. |
| 2 | Ownership | Why does `SfmlUIRenderOverlay` take raw `sf::RenderWindow*` and `sf::RenderTexture*` pointers instead of references? | The lifecycle is: `RenderWindow` ctor creates the SFML objects, then constructs `SfmlUIRenderOverlay` passing them; both are owned by `RenderWindow`. Raw pointers match the existing codebase convention for "non-owning, lifetime-managed-by-parent" relationships. References would be valid too but the codebase consistently uses raw pointers for back-references. |
| 3 | DiaUI dep on Maths | The `IUIRenderOverlay` interface uses `Dia::Maths::Vector2D`. Does DiaUI currently depend on DiaMaths? | Verify in implementation: `dia.ui.architecture.module.md` shows dependencies as core/containers/filepath/memory/strings/input. `Maths::Vector2D` is used by `IPage`/`Page` already (likely via DiaInput's transitive dep). Confirm `dia.maths.vector` is in the required list; add explicitly if missing. |
| 4 | OnCanvasSizeChanged contract | When is `OnCanvasSizeChanged` first called? | `RenderWindow`'s ctor creates `SfmlUIRenderOverlay` with the initial backbuffer already at the correct size, so the SFML impl can just allocate `mUIOverlayTexture` to match in its own ctor. `OnCanvasSizeChanged` then fires only on subsequent resize events. This matches existing behaviour (today's code allocates the overlay texture in the ctor). |
| 5 | Empty UI buffer behaviour | The interface contract says "if buffer is empty, composite the previously-uploaded overlay (preserves today-behaviour)". Is that actually what today does? | Yes — today's `RenderWindow::EndFrame` only calls `mUIOverlayTexture->update(...)` when `GetBufferSize() > 0`, but always draws the sprite using whatever's currently in `mUIOverlayTexture`. So an empty UI buffer leaves the previous overlay visible. Documented in the contract for any future implementer. |
| 6 | Path for ui.frag | The shader path `global/Presentation/ui.frag` is hardcoded. Should this be configurable per-renderer? | Out of scope. Today it's hardcoded; we preserve that. A future cleanup could make it a parameter passed by the canvas at construction time, but it's not load-bearing for the bgfx swap (which will use its own shader). |
| 7 | Multiple windows / multi-canvas | If there are multiple `RenderWindow` instances (e.g. CluicheEditor with editor + game viewports), does each get its own `SfmlUIRenderOverlay`? | Yes — each `RenderWindow` constructs its own. `SfmlUIRenderOverlay` is per-canvas, not a singleton. This works today and continues to work. Confirmed in the spec by making the `SfmlUIRenderOverlay*` a member of `RenderWindow`. |
| 8 | DiaUICEF / DiaUIUltralight | Should those modules implement `IUIRenderOverlay` too? | Out of scope. Those modules use their own off-screen rendering paths (CEF SharedTextureWrapper, Ultralight GPU surface) and feed `UIDataBuffer` to whoever the active renderer is. They are *producers* of `UIDataBuffer`, not implementers of `IUIRenderOverlay`. The interface they care about is `UIDataBuffer` itself, which is unchanged. |
| 9 | ImGui rendering path | Today `RenderWindow::EndFrame` calls `Dia::ImGui::Render()` after the UI overlay composite. Does that move into `IUIRenderOverlay`? | No. ImGui has its own backend (`SFMLImGuiBackend` today, `BgfxImGuiBackend` in `diabgfx-imgui-backend`). The render call stays in `RenderWindow::EndFrame`, after `Composite()` returns. Two separate seams. |
| 10 | RenderWindow shrinkage | After this feature, `RenderWindow` is one step closer to being just IWindow + IInputSource. Are we also splitting those? | Not in this feature. `diasfml-render-removal` (Phase 1's last feature) deletes the ICanvas surface from `RenderWindow` entirely after `DiaBgfx::Canvas` ships. This feature only refactors what stays inside the ICanvas surface. |
| 11 | Test coverage | Should we add a unit test for `SfmlUIRenderOverlay`? | Hard to unit-test a class that needs a real SFML window context. The existing acceptance criterion (DummyStage runs visually identical via `dia run cluichetest`) is the validation. If unit-testable seams exist on `IUIRenderOverlay` itself (parameter validation, contract compliance), add a `MockUIRenderOverlay` to `Dia/DiaUI/Testing/` — defer until needed. |
| 12 | Architecture module dep edge | Does adding `dia.ui` to DiaSFML's required deps create a cycle? | Today `dia.sfml`'s required deps include `dia.graphics.frame`, `dia.input`, `dia.window.interface` — DiaUI is **not** in DiaSFML's deps today. Verify: DiaSFML uses `nextFrame.GetUIData()` (which returns `Dia::UI::UIDataBuffer&`) — so DiaSFML must already include DiaUI transitively via DiaGraphics (FrameData inherits UIFrameData). Adding the explicit edge is correct and validates via `python Tools/dia_modules.py --validate`. |
| 13 | Behavioural verification | "Manual visual diff" is acceptance criterion 6. How do we make this less fragile? | A screenshot-based regression test would be ideal but requires deterministic render output (frame timing, font hinting) that we don't have today. Acceptable for this feature: a screenshot pair attached to the implementation PR's description as a one-time visual diff. Future work in `cluichetestscenarios` could add automated render-output diffing if it becomes load-bearing. |

---
