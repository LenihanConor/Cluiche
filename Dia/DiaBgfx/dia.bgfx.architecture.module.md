---
schema: dia.module.v1
module_id: dia.bgfx
name: DiaBgfx
owner_team: TBD
layer: domain/visual/core
status: active
maturity: dev

path: Dia/DiaBgfx
language: cpp
parent_module_id: dia.root

summary: >
  bgfx-based rendering backend implementing ICanvas for sprites, debug primitives,
  UI overlay compositing, CellField layers, and ImGui debug overlays. Provides Canvas,
  SpriteRenderer, DebugRenderer, UIOverlayRenderer, CellFieldRenderer, BgfxTextureHandle,
  ShaderProgram, and BgfxImGuiBackend (DIA_DEBUG only).

intent: >
  Provide a renderer-agnostic GPU abstraction built on bgfx, replacing DiaSFML's
  render path with a multi-backend (D3D11/D3D12/Vulkan) implementation.

responsibilities:
  - Implement Dia::Graphics::ICanvas via Bgfx::Canvas
  - Provide BgfxTextureHandle implementing Dia::Graphics::ITexture
  - Implement Dia::UI::IUIRenderOverlay via UIOverlayRenderer
  - Render CellField layers via CellFieldRenderer (drains CellFieldRegistry on render thread, draws one layer per request in view 8 between entity and debug views)
  - Load and manage cooked bgfx shader programs (ShaderProgram)
  - Consume FrameData sub-components directly (no visitor pattern per RB-005)
  - Manage CellField geometry caching (topology-specific static geometry by grid size)

non_responsibilities:
  - Window creation or input handling (owned by DiaSDL::Window)
  - Asset discovery or loading orchestration (owned by DiaAssets or application layer)
  - Phase 2 3D rendering (DiaMesh3D, DiaRig3D, DiaSkinning3D)

public_api:
  headers:
    - Dia/DiaBgfx/Canvas.h
    - Dia/DiaBgfx/Renderers/SpriteRenderer.h
    - Dia/DiaBgfx/Renderers/DebugRenderer.h
    - Dia/DiaBgfx/Renderers/UIOverlayRenderer.h
    - Dia/DiaBgfx/Renderers/CellFieldRenderer.h
    - Dia/DiaBgfx/Renderers/CellFieldObservation.h
    - Dia/DiaBgfx/Renderers/CellFieldDrawParams.h
    - Dia/DiaBgfx/Renderers/Ortho2D.h
    - Dia/DiaBgfx/Renderers/CellFieldViewOrder.h
    - Dia/DiaBgfx/Resources/BgfxTextureHandle.h
    - Dia/DiaBgfx/Resources/ShaderProgram.h
    - Dia/DiaBgfx/Resources/CellFieldGeometryCache.h
  namespaces:
    - Dia::Bgfx
  entry_points:
    - Canvas
    - CanvasSettings
    - RendererType
    - SpriteRenderer
    - DebugRenderer
    - UIOverlayRenderer
    - CellFieldRenderer
    - CellFieldObservation
    - CellFieldDrawParams
    - Ortho2D
    - CellFieldViewOrder
    - BgfxTextureHandle
    - ShaderProgram
    - CellFieldGeometryCache
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
    - dia.graphics.cellfield
    - dia.maths.vector
    - dia.ui
    - dia.window.interface
    - dia.bgfx.bgfx
  forbidden: []
---

## CellField Rendering

CellFieldRenderer consumes CellField layers via Canvas::SetCellFieldRegistry(). The registry is drained on the render thread, with one draw request per layer in view 8 (between entity and debug views). All CellField-specific geometry is cached by topology and grid size in CellFieldGeometryCache for reuse across frames.

Shaders: vs_cellfield.sc, fs_cellfield.sc.
