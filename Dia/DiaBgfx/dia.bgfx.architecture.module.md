---
schema: dia.module.v1
module_id: dia.bgfx
name: DiaBgfx
owner_team: TBD
layer: platform
status: active
maturity: dev

path: Dia/DiaBgfx
language: cpp
parent_module_id: dia.root

summary: >
  bgfx-based rendering backend implementing ICanvas for sprites, debug primitives,
  UI overlay compositing, and ImGui debug overlays. Provides Canvas, SpriteRenderer,
  DebugRenderer, UIOverlayRenderer, BgfxTextureHandle, ShaderProgram, and
  BgfxImGuiBackend (DIA_DEBUG only).

intent: >
  Provide a renderer-agnostic GPU abstraction built on bgfx, replacing DiaSFML's
  render path with a multi-backend (D3D11/D3D12/Vulkan) implementation.

responsibilities:
  - Implement Dia::Graphics::ICanvas via Bgfx::Canvas
  - Provide BgfxTextureHandle implementing Dia::Graphics::ITexture
  - Implement Dia::UI::IUIRenderOverlay via UIOverlayRenderer
  - Load and manage cooked bgfx shader programs (ShaderProgram)
  - Consume FrameData sub-components directly (no visitor pattern per RB-005)

non_responsibilities:
  - Window creation or input handling (owned by DiaSFML::RenderWindow, then SFML::Window)
  - Asset discovery or loading orchestration (owned by DiaSFML::TextureHandler)
  - Phase 2 3D rendering (DiaMesh3D, DiaRig3D, DiaSkinning3D)

public_api:
  headers:
    - Dia/DiaBgfx/Canvas.h
    - Dia/DiaBgfx/Renderers/SpriteRenderer.h
    - Dia/DiaBgfx/Renderers/DebugRenderer.h
    - Dia/DiaBgfx/Renderers/UIOverlayRenderer.h
    - Dia/DiaBgfx/Resources/BgfxTextureHandle.h
    - Dia/DiaBgfx/Resources/ShaderProgram.h
  namespaces:
    - Dia::Bgfx
  entry_points:
    - Canvas
    - CanvasSettings
    - RendererType
    - SpriteRenderer
    - DebugRenderer
    - UIOverlayRenderer
    - BgfxTextureHandle
    - ShaderProgram
    - BgfxImGuiBackend  # DIA_DEBUG only

dependencies:
  required:
    - dia.core.core
    - dia.core.crc
    - dia.core.filepath
    - dia.core.memory
    - dia.graphics.frame
    - dia.graphics.interface
    - dia.graphics.misc
    - dia.graphics.assets
    - dia.maths.vector
    - dia.ui
    - dia.window.interface
    - dia.bgfx.bgfx
  forbidden: []
---
