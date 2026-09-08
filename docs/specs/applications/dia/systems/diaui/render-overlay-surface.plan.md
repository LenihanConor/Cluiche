# Plan: render-overlay-surface

**Spec:** [render-overlay-surface.md](render-overlay-surface.md)
**Status:** In Progress

## Session Notes

Binding decisions in force: PD-004 (no STL in public APIs — IUIRenderOverlay uses UIDataBuffer& and Vector2D only), RB-008 (UI overlay path extracted into DiaUI), AD-003 (Dia::UI:: and Dia::SFML:: namespaces).

This is a pure structural move with no behaviour change. The UI compositing block currently lives in RenderWindow::EndFrame (lines ~224-257). It uses mUIShader, mUIOverlayTexture, loads ui.frag from global/Presentation/ui.frag, and calls pushGLStates/popGLStates. All of that moves verbatim into SfmlUIRenderOverlay.

RenderWindow uses default-ctor + parameterised-ctor. The mUIShader/mUIOverlayTexture are only allocated in the parameterised ctor — mUIRenderOverlay follows the same pattern. Default ctor leaves mUIRenderOverlay nullptr; EndFrame guards with mUIRenderOverlay != nullptr (same guard pattern as mWindowContext today).

DiaUI has no .vcxproj.filters file — no filters update needed for DiaUI.
DiaUI dep on DiaMaths: module doc lists dia.core.* and dia.input; Maths::Vector2D comes through transitive dep. Adding dia.maths.vector explicitly to the module doc required.

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Add `IUIRenderOverlay.h` to `Dia/DiaUI/`; register in `DiaUI.vcxproj` | Build green | Done | sonnet | AC-1; header-only; no SFML/bgfx deps |
| 2 | Add `SfmlUIRenderOverlay.h/.cpp` to `Dia/DiaSFML/`; register in `DiaSFML.vcxproj` | Build green | Done | sonnet | AC-2; moves UI block from RenderWindow verbatim |
| 3 | Modify `RenderWindow.h/.cpp`: add `mUIRenderOverlay`; remove `mUIShader`/`mUIOverlayTexture`; delegate `EndFrame` + `SetCanvasSize` | Build green | Done | sonnet | AC-3/4/5; grep confirms no mUIShader/mUIOverlayTexture remain |
| 4 | `dia run googletest` — full suite green | 0 failures | Done | haiku | AC-7 |
| 5 | Update `dia.ui.architecture.module.md` + `dia.sfml.architecture.module.md` | Docs accurate | Done | haiku | AC-9/10 |
| 6 | Commit | — | Done | haiku | — |
