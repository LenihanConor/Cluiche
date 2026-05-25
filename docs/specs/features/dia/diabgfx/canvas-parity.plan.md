---
name: canvas-parity-plan
description: Implementation plan for DiaBgfx canvas-parity feature
metadata:
  type: plan
  spec: docs/specs/features/dia/diabgfx/canvas-parity.md
  status: In Progress
---

# Plan: canvas-parity

## Session Notes

Creating the DiaBgfx module with Bgfx::Canvas implementing ICanvas. Key constraints:
- PD-004: no STL in public headers; STL freely used in .cpp
- RB-004: Canvas implements ICanvas only — NOT IWindow or IInputSource
- RB-005: no visitor pattern; sub-renderers consume FrameData directly
- RB-006: bgfx::TextureHandle must NOT appear in DiaGraphics public surface
- PD-008: inherit Directory.Build.props; no OutDir/PlatformToolset overrides in vcxproj
- bgfx extern layout: include/ lib/Debug bgfx.lib lib/Release bgfx.lib; also need bx/include bimg/include
- bgfx.lib is fat (bx+bimg already inside) — only bgfx.lib needed at link time
- No External/bgfx/src or examples — include path is External/bgfx/include only
- RenderWindow rename deferred to render-removal (user decision)
- BGFX_BACKEND env var: KernelModule reads it; if set, constructs Bgfx::Canvas alongside SFML
- UIRenderOverlay: UIModule uses Ultralight (not SFML overlay) — GetUIRenderOverlay() on Canvas exists but UIModule doesn't use IUIRenderOverlay directly today; wire-up deferred until render-removal
- TextureHandler backend dispatch: add static flag TextureHandler::SetBgfxActive(bool); Tick() dispatches based on it
- CanvasSettings cookedShaderRoot: KernelModule sets to "Cluiche/out/cluichetest/shaders" resolved at startup
- bgfx D3D11 default; RendererType enum maps to bgfx::RendererType enum values
- ImGui suppression: Canvas stores bool mImGuiSuppressed; StartFrame/EndFrame skip ImGui calls when bgfx active

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Create DiaBgfx.vcxproj + DiaBgfx.vcxproj.filters + dia.bgfx.architecture.module.md | AC-1, AC-18 | Done | haiku | Static lib; links bgfx.lib; include dirs for bgfx/bx/bimg |
| 2 | Add DiaBgfx to Cluiche.sln | AC-1 | Done | haiku | Follow existing project entry pattern |
| 3 | Implement ShaderProgram.h/.cpp | AC-1 | Done | sonnet | Loads cooked .bin files; bgfx::createProgram |
| 4 | Implement BgfxTextureHandle.h/.cpp | AC-7 | Done | sonnet | ITexture impl; bgfx::TextureHandle; atomic State; UploadFromMemory |
| 5 | Implement SpriteRenderer.h/.cpp | AC-4 | Done | sonnet | Consumes EntityFrameData; TransientVertexBuffer; ortho proj; batch per ITexture* |
| 6 | Implement DebugRenderer.h/.cpp | AC-5 | Done | sonnet | Consumes DebugFrameData; all 8 primitive types; TransientVertexBuffer; Text2D via bgfx::dbgTextPrintf |
| 7 | Implement UIOverlayRenderer.h/.cpp | AC-6 | Done | sonnet | Implements IUIRenderOverlay; bgfx 2D texture; fullscreen quad |
| 8 | Implement Canvas.h/.cpp | AC-2, AC-3, AC-15 | Done | sonnet | ICanvas impl; AttachToNativeWindow; sub-renderer lifecycle; bgfx::init/frame; ImGui suppression |
| 9 | Add six .sc shaders + varying.def.sc under Dia/DiaBgfx/Shaders/ | AC-8, AC-9 | Done | sonnet | vs/fs_sprite, vs/fs_debug, vs/fs_ui_overlay; varying.def.sc |
| 10 | Extend TextureHandler to dispatch BgfxTextureHandle on bgfx path | AC-7 | Done | sonnet | Add SetBgfxActive static; Tick() creates BgfxTextureHandle when flag set |
| 11 | Extend KernelModule: BGFX_BACKEND env var → construct Bgfx::Canvas | AC-10, AC-16 | Done | sonnet | If env var set, construct Canvas, AttachToNativeWindow, SetBgfxActive; sCanvas → bgfx canvas |
| 12 | Update vcxproj: add all DiaBgfx files | AC-1 | Done | sonnet | Canvas, sub-renderers, resources, shaders (as None items) |
| 13 | pipeline.toml: add [bgfx] section | - | Done | sonnet | default_backend = "dx11" |
| 14 | Update spec status to Done | - | Done | sonnet | |
