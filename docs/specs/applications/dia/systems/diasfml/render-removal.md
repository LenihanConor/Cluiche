# Feature Spec: render-removal

## Parent System
@docs/specs/applications/dia/systems/render-backend/render-backend.md

**Research:** @docs/research/render_backend_swap/summary.md

## Status
`Done`

## Summary

Delete the SFML render path entirely. After this feature, `Dia/DiaSFML/` retains only `IWindow` and `IInputSource` implementations (window creation, sizing, title, icon, visibility, system handle, event polling, keyboard/mouse translation). All ICanvas-related code, all sprite/debug/UI/imgui rendering, and all SFML graphics types (`sf::RenderWindow`, `sf::RenderTexture`, `sf::Shader`, `sf::Sprite`, etc.) are removed from the engine. `BGFX_BACKEND` becomes meaningless — bgfx is the only render path. The `DiaSFML.vcxproj` shrinks from a graphics + input + window library to a window + input library only.

This is the **Phase 1 ship gate** (RB-016). Before this feature lands: bgfx is at parity; after this feature lands: SFML is no longer a renderer.

## Problem

After `canvas-parity` and `imgui-backend`, `Dia/DiaSFML/` contains *both* render paths simultaneously: the new bgfx-driven flow (consumed by `Dia::Bgfx::Canvas`) and the legacy SFML-driven flow (`RenderWindow::ProcessFrame`, `EntityFrameRenderer`, `DebugFrameRendererVisitor`, `SfmlUIRenderOverlay`, `SFMLImGuiBackend`, etc.). The dual paths are intentionally kept alive for parity verification, but they have a real cost:

- Every change to `FrameData` requires updating both consumers
- The runtime backend selector (`BGFX_BACKEND` env var) and the dual app-wire-up branch in `Main.cpp` are technical debt
- `DiaSFML.vcxproj` continues to depend on `sf::RenderWindow`, `sf::RenderTexture`, `sf::Shader`, `sf::Sprite`, `sf::Texture` — heavy SFML graphics surface even though nothing renders through it anymore
- The `TextureHandler::Tick()` backend dispatch (per `canvas-parity`) is awkward — it lives in DiaSFML but knows about bgfx
- `SFMLImGuiBackend` exists but is unreachable in production
- The `Win32WndProcChain` shim from `imgui-backend` has been validated; it stays, but hangs on a smaller, cleaner DiaSFML

The right end-state for Phase 1 is one render path (bgfx). This feature collapses the dual-path structure.

## Goals

- Delete the entire `Dia::Graphics::ICanvas` implementation from `Dia::SFML::RenderWindow` (or `Window` post-rename)
- Delete `EntityFrameRenderer`, `DebugFrameRendererVisitor`, `Conversion.h` (the SFML-specific conversion helpers used only by the renderer), `SfmlUIRenderOverlay`, `SFMLImGuiBackend`, `SfmlTexture`
- Move `TextureHandler` (the asset handler) out of `Dia/DiaSFML/` and into `Dia/DiaAssetRuntime/` (or a new module `Dia/DiaTextureAsset/` if asset-runtime feels wrong) — it is fundamentally an asset handler, not a graphics backend
- After the move, `TextureHandler::Tick()` produces `BgfxTextureHandle` directly; the backend dispatch added in `canvas-parity` is gone
- Remove `BGFX_BACKEND` env var handling from `Main.cpp` and the active-canvas selector — bgfx is unconditional
- Remove all SFML graphics types from `DiaSFML`'s headers and `.vcxproj`. Remaining SFML deps: `sf::Window`, `sf::Event` (input only). Remove `<SFML/Graphics.hpp>` includes
- `Dia::SFML::Window` (renamed in `canvas-parity`) implements **only** `IWindow` and `IInputSource`
- The `IUIRenderOverlay` defaulting that bgfx wires up at app construction becomes the sole UI overlay path; no more SFML-overlay fallback
- `dia.sfml.architecture.module.md` updated to reflect the narrowed scope: window + input only
- Update `pipeline.toml`'s cluichetest deploy step to stop copying SFML graphics DLLs (`sfml-graphics-*.dll`) — only `sfml-window-*.dll` and `sfml-system-*.dll` remain (verify which DLL bundles which symbols and adjust accordingly)
- All existing tests still green (or removed if they tested SFML render code that no longer exists; failing tests in this feature are not new bugs — they're tests measuring deleted code)
- DummyStage runs identically to its bgfx-pre-removal state on `dia run cluichetest` — no visual regression

## Non-Goals

- **Removing SFML window/input** — that's the future SDL migration; out of scope for this system entirely
- **Removing audio/font deps in SFML** — if any code uses `sf::Font`, `sf::SoundBuffer`, etc., they stay (audit happens in this feature; if found, capture as TBD work). Removal of audio is its own future work
- **Removing the `SFML` namespace from DiaSFML** — name stays; only the contents shrink
- **Renaming `DiaSFML.vcxproj` to `DiaWindowSFML.vcxproj`** — out of scope; the rename can happen alongside the SDL migration if desired
- **Deleting the `External/SFML/` directory** — SFML is still a runtime dep for window+input; `deps.json` keeps the SFML entry
- **Refactoring `IInputSource`** — input layer untouched
- **Removing the `Dia::SFML::Win32WndProcChain` shim from `imgui-backend`** — that stays; it's tied to the SFML-owned window's WndProc, not the SFML render path
- **Migrating non-SFML window backend** — that's a future research

## TextureHandler relocation

`TextureHandler` (the asset handler) currently lives in `Dia/DiaSFML/TextureHandler.{h,cpp}`. It was placed there historically because it produced `sf::Texture*` — but per `texture-handle-stringcrc` it produces `Dia::Graphics::ITexture*` (a renderer-agnostic handle). It has no remaining SFML dependency *except* the `sf::Image` decode path (worker thread side).

Two options:

| Option | Pros | Cons |
|---|---|---|
| **A. Move to `Dia/DiaAssetRuntime/TextureHandler.{h,cpp}`** | Co-located with asset runtime; obvious home | DiaAssetRuntime gains a render-asset concern; mild scope bloat |
| **B. New `Dia/DiaTextureAsset/`** | Tight scope; texture asset handling isolated | Yet another module; less reuse with future MeshAsset etc. |

Going with **A** for this feature. `Dia/DiaAssetRuntime/Handlers/TextureHandler.{h,cpp}`, namespace `Dia::AssetRuntime::TextureHandler`. The image decode (currently `sf::Image::loadFromFile`) gets replaced with `bgfx::imageLoad` (bgfx ships an image decoder using bimg — already vendored). This drops the last `sf::Image` reference in the asset path.

If `bgfx::imageLoad` is not the right call (e.g. it expects a binary blob, not a path), use bimg's `bimg::imageParse` after reading the file with `Dia::Core::File::Read`. Final API choice picked at implementation; both paths are battle-tested.

## What gets deleted

```
DELETED:
Dia/DiaSFML/EntityFrameRenderer.h
Dia/DiaSFML/EntityFrameRenderer.cpp
Dia/DiaSFML/DebugFrameRendererVisitor.h
Dia/DiaSFML/DebugFrameRendererVisitor.cpp
Dia/DiaSFML/SFMLImGuiBackend.h
Dia/DiaSFML/SFMLImGuiBackend.cpp
Dia/DiaSFML/SfmlTexture.h
Dia/DiaSFML/SfmlTexture.cpp
Dia/DiaSFML/SfmlUIRenderOverlay.h
Dia/DiaSFML/SfmlUIRenderOverlay.cpp
Dia/DiaSFML/Conversion.h           (used only by deleted renderers; verify; if InputSource also uses it, leave it)
Dia/DiaSFML/Conversion.cpp         (same)

MOVED:
Dia/DiaSFML/TextureHandler.h            -> Dia/DiaAssetRuntime/Handlers/TextureHandler.h
Dia/DiaSFML/TextureHandler.cpp          -> Dia/DiaAssetRuntime/Handlers/TextureHandler.cpp

MODIFIED:
Dia/DiaSFML/Window.h (renamed from RenderWindow.h in canvas-parity)
   - Remove inheritance from Graphics::ICanvas
   - Remove all ICanvas method declarations
   - Remove mBackBuffer, mUIRenderOverlay, mTextureHandler members (already done in render-overlay-surface; verify)
   - Remove sf::RenderWindow* mWindowContext -> sf::Window* mWindowContext (sf::Window is the input-only base class)
Dia/DiaSFML/Window.cpp
   - Body shrinks dramatically; loses StartFrame/ProcessFrame/EndFrame, SetCanvasSize, Initialize(ICanvas::Settings&)
   - Constructor no longer creates RenderTexture/Shader; only creates the window
Dia/DiaSFML/dia.sfml.architecture.module.md
   - public_api.entry_points trimmed: Window, InputSource (and Win32WndProcChain in DIA_DEBUG); remove EntityFrameRenderer, DebugFrameRendererVisitor, SFMLImGuiBackend, RenderWindow, RenderTarget, RenderTexture, Color, RGBA, Event references
   - dependencies.required: remove dia.graphics.frame, dia.graphics.misc, dia.graphics.interface (RenderWindow no longer impls ICanvas; window+input only)
   - Replace `dia.sfml.sfml` with the SFML-window+system subset only

Dia/DiaSFML/DiaSFML.vcxproj{,.filters}
   - Remove deleted .h/.cpp file references
   - Remove SFML/Graphics.hpp include path additions (if SFML's include is split per module; otherwise keep — header-only includes are cheap)
   - Remove sfml-graphics-d.lib / sfml-graphics.lib from linker; keep sfml-window, sfml-system

Dia/DiaAssetRuntime/DiaAssetRuntime.vcxproj{,.filters}
   - Add Handlers/TextureHandler.{h,cpp}
   - Add include path to External/bgfx/include (for bgfx::imageLoad)
   - Add link to bgfx.lib (for bimg/bx symbols if image decode path needs them)

Dia/DiaAssetRuntime/dia.assetruntime.architecture.module.md
   - public_api: add TextureHandler entry point
   - dependencies.required: add dia.graphics.assets (for ITexture interface), dia.bgfx (for BgfxTextureHandle creation)

Dia/DiaBgfx/Resources/BgfxTextureHandle.h
   - Already exists from canvas-parity; no header change. cpp may need minor tweak if TextureHandler relocates.

Cluiche/Cluiche/Main.cpp (or kernel module)
   - Delete BGFX_BACKEND env-var branch
   - Delete SFML-render-path active-canvas branch
   - Bgfx::Canvas constructed unconditionally
   - SFMLImGuiBackend reference deleted; only BgfxImGuiBackend constructed (still DIA_DEBUG-guarded)
   - Window construction unchanged (still DiaSFML::Window)

pipeline.toml
   - [targets.cluichetest.deploy]: remove sfml-graphics-d.dll, sfml-graphics.dll from copy list
   - Same for cluicheeditor if applicable

Cluiche/Tests/GoogleTests/DiaSFML/TestTextureHandler.cpp
   - Move to Cluiche/Tests/GoogleTests/DiaAssetRuntime/TestTextureHandler.cpp
   - Update includes
```

## CLI / runtime changes

- `BGFX_BACKEND` env var is no longer read. If a developer sets it, it's ignored (no warning — silently a no-op)
- `dia run cluichetest` runs through bgfx unconditionally
- `dia run googletest` may run faster (slightly smaller binary); functionally unchanged

## Phase 1 ship gate (RB-016) — verification checklist

This feature is the gate. Before it can be marked Done:

- [ ] Every existing visual debugger renders correctly via bgfx (rig2d, rigidbody2d, softbody2d, ik2d, geometry2d, animation2d)
- [ ] CluicheTest's DummyStage looks indistinguishable from the SFML output (manual visual diff in PR description)
- [ ] All GoogleTests + e2e harness scenarios pass
- [ ] ImGui debug overlay (DiaVisualDebuggerConsole) works
- [ ] UI overlay compositing works (DummyStage's UI buffer renders)
- [ ] No `sf::RenderWindow`, `sf::RenderTexture`, `sf::Shader`, `sf::Sprite`, `sf::Texture` references remain in any `Dia/` source file (verified by grep)

## Files Introduced / Modified

See *What gets deleted* section. Net effect:

- 11 files deleted
- 2 files moved (TextureHandler)
- 5 files modified (Window, Main.cpp, two .vcxproj, two architecture module markdowns, pipeline.toml)
- 0 new files

## Dependencies

| Dependency | Type | Notes |
|------------|------|-------|
| `canvas-parity` (Approved) | Hard | Bgfx render path must be working |
| `imgui-backend` (Approved) | Hard | Bgfx ImGui must be working — otherwise SFMLImGuiBackend cannot be deleted |
| `texture-handle-stringcrc` (Approved) | Hard | Renderer-agnostic ITexture must exist |
| `render-overlay-surface` (Approved) | Hard | IUIRenderOverlay seam exists |
| `texturehandler-two-phase-load` (Approved, async-asset-loading) | Hard | The async loader's plan-update (re-target against ITexture) must be done before TextureHandler can move cleanly |

## Acceptance Criteria

1. `Dia::Graphics::ICanvas` is **no longer implemented** by any class in `Dia/DiaSFML/` (verified by `grep -rn "public Dia::Graphics::ICanvas\|: public.*ICanvas" Dia/DiaSFML/` returning zero matches outside `Dia/DiaBgfx/`)
2. `Dia::SFML::EntityFrameRenderer`, `DebugFrameRendererVisitor`, `SFMLImGuiBackend`, `SfmlTexture`, `SfmlUIRenderOverlay` are deleted; their headers no longer exist on disk
3. `TextureHandler` is moved to `Dia/DiaAssetRuntime/Handlers/TextureHandler.{h,cpp}`; namespace updated to `Dia::AssetRuntime`
4. `Dia/DiaSFML/DiaSFML.vcxproj` no longer links against `sfml-graphics*.lib` (verified by inspection of `<AdditionalDependencies>`)
5. `pipeline.toml` no longer copies `sfml-graphics-*.dll` to deploy output for any target
6. `dia run cluichetest` runs unconditionally on bgfx (no env var branching); DummyStage renders identically to the pre-feature state (manual visual diff acceptable)
7. `dia run googletest` is green; `TestTextureHandler` tests run from their new `Dia/DiaAssetRuntime/` location
8. All existing visual debuggers render correctly via bgfx (rig2d, rigidbody2d, softbody2d, ik2d, geometry2d, animation2d) — manual sweep documented in PR description
9. ImGui debug overlay (DiaVisualDebuggerConsole) is functional — layer toggles, metrics, log tail
10. `Dia/DiaSFML/dia.sfml.architecture.module.md` `public_api.entry_points` lists only `Window`, `InputSource`, and (in DIA_DEBUG) `Win32WndProcChain`; render-related entries are gone
11. `grep -rn "sf::RenderWindow\|sf::RenderTexture\|sf::Sprite\|sf::Texture\|sf::Shader" Dia/` returns zero matches
12. `python Tools/dia_modules.py --validate` passes; module dependency graph remains acyclic
13. `BGFX_BACKEND` env var is no longer referenced in any source file (verified by grep)
14. Build clean under `/std:c++20` for `Debug|x64` and `Release|x64` after the deletions
15. CluicheTest binary size shrinks measurably (≥10% on Debug, expected from removing the SFML graphics path)
16. `Dia/DiaAssetRuntime/Handlers/TextureHandler.cpp`'s image decode uses `bgfx::imageLoad` (or equivalent bimg call) — no `sf::Image` references remain in the asset path
17. **Phase 1 ship gate met:** SFML render path deletable (this feature does the deletion); all tests green; visual parity confirmed; ImGui regression-free; UI overlay regression-free
18. The `RenderBackend` system spec status flips from `In Progress` to `In Progress (Phase 1 done; Phase 2 not started)` — explicit annotation

## Traceability

| Level | Spec | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia/dia.md |
| System (primary) | RenderBackend | @docs/specs/applications/dia/systems/render-backend/render-backend.md |

## Binding Decisions Compliance

| ID | Source | Decision | Compliance |
|----|--------|----------|------------|
| PD-001 | Platform | StringCRC for IDs | Compliant — TextureHandler relocation preserves StringCRC keying |
| PD-002 | Platform | ProcessingUnit/Phase/Module architecture | Compliant — no PU/Phase changes |
| PD-003 | Platform | Component-based entities | N/A |
| PD-004 | Platform | No STL containers in public APIs | Compliant — TextureHandler's relocated public surface unchanged |
| PD-005 | Platform | x64 only | Compliant |
| PD-006 | Platform | Visual Studio project files are source of truth | Compliant — `.vcxproj` files manually updated for deletions and the TextureHandler move |
| PD-007 | Platform | C++20 required | Compliant |
| PD-008 | Platform | `Directory.Build.props` owns OutDir/IntDir | Compliant |
| PD-009 | Platform | Generated output under `Cluiche/out/<AppName>/` | N/A — no generated output changes |
| PD-010 | Platform | `.diagame` typed imports | N/A |
| AD-001 | Dia App | Module YAML frontmatter | Compliant — both DiaSFML and DiaAssetRuntime architecture markdowns updated |
| AD-002 | Dia App | No STL in public APIs | Reinforces PD-004 |
| AD-003 | Dia App | Namespace `Dia::<Module>::` | Compliant — TextureHandler moves to `Dia::AssetRuntime::TextureHandler` |
| AD-004 | Dia App | ProcessingUnit/Phase/Module | N/A |
| AD-005 | Dia App | Component-based entities | N/A |
| RB-002 | RenderBackend | Two-phase delivery | Compliant — this feature closes Phase 1 |
| RB-004 | RenderBackend | Canvas implements ICanvas only | **Compliant — deleting the SFML ICanvas impl makes Bgfx::Canvas the sole implementer** |
| RB-005 | RenderBackend | No visitor pattern in production render path | Compliant — `DebugFrameRendererVisitor` deleted |
| RB-006 | RenderBackend | No backend types in DiaGraphics public surface | Compliant — preserved by deletion |
| RB-007 | RenderBackend | ITexture/IShader keyed by StringCRC | Compliant — preserved |
| RB-009 | RenderBackend | Async loading sequencing | Compliant — TextureHandler relocation finalises the async-loader's home |
| RB-016 | RenderBackend | Phase 1 ship gate: SFML render path deletable | **Compliant — this feature is the ship gate** |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | TextureHandler home | Why DiaAssetRuntime/Handlers/ instead of a new DiaTextureAsset module? | DiaAssetRuntime is the natural home — it already owns asset lifecycle (`IAssetTypeHandler`, `IAssetLoadCallback`). Adding a per-asset-type module multiplies module count without payoff. Future MeshAsset, AudioAsset would follow the same pattern under `Dia/DiaAssetRuntime/Handlers/`. |
| 2 | Image decode | Is `bgfx::imageLoad` actually the right entry point? | bgfx exposes `bgfx::Memory* mem = bgfx::copy(fileBytes, size); bgfx::TextureHandle = bgfx::createTexture(mem)` — but that's for binary KTX/DDS. For PNG/JPG decode, the right path is bimg's `bimg::imageParse` (creates a `bimg::ImageContainer`) followed by `bgfx::createTexture2D(... mem ...)`. Final API selected at implementation time. |
| 3 | sfml-graphics DLL audit | Are there any DLL deps that *look* like sfml-graphics but actually carry window/input symbols? | SFML separates: sfml-system (utilities), sfml-window (window+input), sfml-graphics (rendering). Window+input use system+window only. Verify with `dumpbin /dependents` on the existing CluicheTest.exe before and after to confirm graphics DLLs drop. |
| 4 | DiaAssetRuntime gaining DiaBgfx dep | Does DiaAssetRuntime now depend on DiaBgfx? Cyclic risk? | Yes, DiaAssetRuntime gains a `dia.bgfx` dependency. DiaBgfx depends on DiaGraphics + DiaWindow + DiaUI (per canvas-parity). No cycle: DiaBgfx does not depend on DiaAssetRuntime. Acyclic. Validated by `python Tools/dia_modules.py --validate`. |
| 5 | TextureHandler tests | Move tests with the file? | Yes — move TestTextureHandler from `Cluiche/Tests/GoogleTests/DiaSFML/` to `Cluiche/Tests/GoogleTests/DiaAssetRuntime/`. Per the test-utilities-ship-with-libraries memory, tests live next to the code they test. |
| 6 | RenderBackend system status | After this feature, system goes from "In Progress" to what? | "In Progress (Phase 1 Done)" — Phase 2 features are Approved but not started. The system spec only goes Done when Phase 2 ships. Captured in acceptance criterion 18. |
| 7 | Deletion atomicity | Should this feature be split into "delete renderer" + "move TextureHandler" + "wire-up cleanup"? | One feature, but the implementation plan should split into 3 sub-tasks for review hygiene. Plan template captures this. |
| 8 | SFML window dep removal | Could we go further and replace SFML window with native Win32 in this feature? | No — out of scope (NDR). SFML's window+input is fine; the SDL migration is its own future research. |
| 9 | Visual diff fragility | "DummyStage looks identical" is the same fragile criterion from canvas-parity. Better answer? | Same answer: PR description includes a screenshot pair. Future automated render-output diffing (in `cluichetestscenarios`) is a separate work item. The risk is bounded because the bgfx path was already validated in `canvas-parity`. |
| 10 | Future SDL migration | Will this feature's deletions need to be redone when SDL replaces SFML window/input? | No — those deletions are graphics-only. SDL migration touches `Dia::SFML::Window` (renames/replaces it) and `Dia::SFML::InputSource`, but not the deleted graphics classes. They stay deleted. |
| 11 | imgui_impl_win32 dependency | Does the WndProc chain still work if we ever swap SFML for SDL? | No — `Win32WndProcChain` is Win32-specific. SDL's equivalent is `SDL_AddEventWatch`. The shim gets replaced when SDL lands. Documented in `imgui-backend`'s spec. |
| 12 | Binary size win | "≥10% on Debug" — is that achievable? | Removing 6 .cpp files plus dropping sfml-graphics linkage should shrink Debug binary noticeably. 10% is conservative; realistic might be 15–25%. Set the bar at 10% so the criterion isn't a flaky heuristic. |
| 13 | Conversion.h fate | `Dia::SFML::Convert` is used by both render-side and input-side conversions. Audit needed? | Yes — `Conversion.h/.cpp` may have dual users. Audit at implementation: if any non-render caller exists, keep the file but slim it (delete render-only conversions); if only render uses it, delete the file. |

---
