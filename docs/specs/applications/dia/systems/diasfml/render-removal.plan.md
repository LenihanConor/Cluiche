---
name: render-removal-plan
description: Implementation plan for the render-removal feature — Phase 1 ship gate that deletes the SFML render path and makes bgfx unconditional
metadata:
  type: plan
  spec: docs/specs/features/dia/diasfml/render-removal.md
  status: Done
---

# Plan: render-removal

## Session Notes

Deletes the SFML render path and makes bgfx unconditional. All 6 prerequisite Phase 1 features are Done.

**Spec decisions and constraints:**
- RB-004: Canvas implements ICanvas only — deleting RenderWindow's ICanvas impl makes Bgfx::Canvas the sole implementer
- RB-005: No visitor pattern — `DebugFrameRendererVisitor` deleted
- RB-016: Phase 1 ship gate — this feature closes Phase 1
- RenderWindow rename (deferred from canvas-parity): `RenderWindow.h/.cpp` → `Window.h/.cpp`, class `Dia::SFML::Window`
- BGFX_BACKEND env var lives in `KernelModule.cpp` (not Main.cpp); remove the branch, always construct `Bgfx::Canvas`
- TextureHandler moves from `Dia::SFML` → `Dia::AssetRuntime`; drops `sBgfxActive`/SfmlTexture dual-path; always produces BgfxTextureHandle; image decode switches from `sf::Image` → `bimg::imageParse`
- `ProcessGpuDeletions()` can be removed from TextureHandler — it existed only for deferred `sf::Texture` GPU deletion; bgfx handles its own resource destruction internally
- `AssetServiceModule` currently casts `kernel->GetWindow()` to `RenderWindow*` to get `GetTextureHandler()` — after the move, use `KernelModule::GetStaticTextureHandler()` directly
- Conversion.h is render-only (confirmed by audit); deleted
- `ui.frag` shader in `Dia/DiaSFML/` is the SFML UI overlay shader; deleted
- `sfml-graphics-s-d.lib` / `sfml-graphics-s.lib` must be removed from `DiaSFML.vcxproj` linker deps
- `pipeline.toml` SFML deploy uses `*.dll` wildcard — change to explicit `sfml-window` + `sfml-system` entries to stop deploying `sfml-graphics*.dll`
- Tasks must run in order — each depends on the previous

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Delete 10 render-only .h/.cpp files from `Dia/DiaSFML/` + `ui.frag`; remove their entries from `DiaSFML.vcxproj` + `DiaSFML.vcxproj.filters` | AC-2 | Done | haiku | EntityFrameRenderer, DebugFrameRendererVisitor, SFMLImGuiBackend, SfmlTexture, SfmlUIRenderOverlay, Conversion; ui.frag shader |
| 2 | Strip `ICanvas` from `RenderWindow`; rename `RenderWindow.h/.cpp` → `Window.h/.cpp`, class `Dia::SFML::RenderWindow` → `Dia::SFML::Window`; update `RenderWindowFactory` return type; update all `#include` + type refs across codebase | AC-1, AC-10 | Done | sonnet | Remove: ICanvas inheritance, Initialize(ICanvas::Settings), StartFrame/ProcessFrame/EndFrame, SetCanvasSize, SetActiveContext (ICanvas version), mBackBuffer, mUIRenderOverlay, mTextureHandler. Keep: IWindow, InputSource, Win32WndProcChain. sf::RenderWindow* → sf::Window* |
| 3 | Move `TextureHandler.h/.cpp` to `Dia/DiaAssetRuntime/Handlers/`; rename namespace `Dia::SFML` → `Dia::AssetRuntime`; replace `sf::Image::loadFromFile` → stb_image decode in DiaBgfx via `BgfxTextureHandle::UploadFromEncodedMemory`; remove `ProcessGpuDeletions()`; update `DiaAssetRuntime.vcxproj` | AC-3, AC-16 | Done | sonnet | bimgDebug.lib didn't include imageParse; used stb_image.h (bundled in bimg/3rdparty) via DiaBgfx instead |
| 4 | Update `KernelModule.cpp/.h`: remove `BGFX_BACKEND` env-var branch; always construct `Bgfx::Canvas`; KernelModule owns `Dia::AssetRuntime::TextureHandler` directly | AC-13, AC-4, AC-11 | Done | sonnet | |
| 5 | Update callers: `AssetServiceModule.cpp`, `RenderModule.cpp`, `DummyLevelModule.cpp` | AC-7 | Done | haiku | |
| 6 | Remove `sfml-graphics` from `DiaSFML.vcxproj` linker deps | AC-4 | Done | haiku | |
| 7 | Update `pipeline.toml` SFML deploy entries to explicit sfml-window + sfml-system only | AC-5 | Done | haiku | |
| 8 | Move `TestTextureHandler.cpp` to `DiaAssetRuntime/`; update `GoogleTests.vcxproj` | AC-7 | Done | haiku | |
| 9 | Update architecture module docs | AC-10 | Done | haiku | |
| 10 | `dia run googletest` — 5277/5278 pass (1 pre-existing failure in VisualDebuggerConsole) | AC-7 | Done | sonnet | bimgDebug.lib missing imageParse; fixed by moving decode into DiaBgfx/stb_image |
| 11 | `dia run cluichetest` — passes; `dia run cluicheeditor` — passes | AC-6, AC-8, AC-9 | Done | haiku | Fixed: imgui core sources (imgui.cpp etc.) moved from DiaSFML → DiaBgfx |
| 12 | Update feature spec status → `Done`; update render-backend system spec annotation; update BACKLOG.md; commit | AC-17, AC-18 | Done | haiku | |
