# Research: Choice — Render Backend Swap (replace SFML)

**Date:** 2026-05-17
**Chosen candidate:** bgfx phased (Candidate 10) — *spec all phases now, implement Phase 1 only*

## Rationale

bgfx is the right backend for the stated trajectory: light 3D within 6–12 months (terrain, static + skinned models, animation, no photorealism), without locking into a full PBR engine like Filament. It is production-proven, has multi-backend coverage (D3D11/D3D12/Vulkan/GL on Windows), strong debug-draw ergonomics for `DebugFrameData`, and a mature ImGui adapter. PD-001/PD-004/PD-005/PD-006/PD-007 are all satisfiable at the public API layer.

The phased delivery shape (Candidate 10, score 4.35) was chosen over the single-milestone shape (Candidate 1, score 3.75) because the parity work alone touches enough surfaces — `ICanvas`, `IInputSource` (SFML keeps owning input for now), `IImGuiBackend`, asset loaders, `DebugFrameRendererVisitor`, `EntityFrameRenderer`, the UI overlay shader path — that bundling 3D into the same work item invites either scope cut or scope drift. A hard parity ship gate at the end of Phase 1 ("SFML is deletable from the render path") gives a clean rollback point and a clear demoable milestone before any 3D risk is taken on.

Filament was ruled out as overkill: its PBR pipeline, IBL/TAA/post chain, and scene-graph paradigm are designed for content the brief explicitly does not need (photorealism), and the integration cost of adapting `FrameData` / `SpriteDrawCommand` / `DebugPrimitive` onto Filament's worldview rivals the cost of writing the renderer ourselves on bgfx.

## What Was Ruled Out

| Candidate | Reason not chosen |
|-----------|-------------------|
| 1 — bgfx single milestone | Same backend, but bundles parity + 3D into one push; loses the clean SFML-removal ship gate and the option to pivot mid-stream |
| 2 — Diligent Engine | Modern API design is appealing but smaller community than bgfx and no clear advantage for the "light 3D" target |
| 3 — Sokol | Cheapest integration, but no D3D12/Vulkan, smaller ecosystem, every feature hand-written with less community scaffolding than bgfx |
| 4 — Filament | XL cost; PBR + IBL + post chain are exactly the features the brief does **not** need |
| 5 — D3D11 native | Highest Cluiche Fit, but no multi-backend insurance; viable fallback if bgfx integration disappoints in Phase 1 |
| 6 — D3D12 native | XL bring-up cost on a brief that doesn't need explicit-API performance ceiling |
| 7 — SDL_GPU | Tightly coupled to a future SDL window/input migration that is out of scope for now |
| 8 — SFML 3.0 + custom 3D layer | Doesn't solve the stated goal; postpones it and accumulates GL2-era technical debt |
| 9 — Magnum | Smaller community than bgfx, higher Risk for an unfamiliar engine paradigm |

## Pre-Spec Commitments

These constraints were stated by the user during research and must be carried into the specs.

### Backend & delivery shape

- **Backend:** bgfx, integrated as a new `Dia/DiaBgfx/` module sibling to `DiaSFML`
- **Delivery shape:** Spec all phases up-front; **implement Phase 1 only** in the first work cycle. Phase 2 specs are written but stay `Approved` (not `In Progress`) until Phase 1 ships
- **Phases:**
  - **Phase 1 — Parity (implement now):** `DiaBgfx::Canvas` reaches functional parity with `DiaSFML` for sprites, debug primitives, UI overlay compositing, and ImGui backend. All existing tests + visual debuggers green on bgfx. Hard ship gate: SFML render path is deletable
  - **Phase 2 — Light 3D (spec now, implement later):** Static mesh, skinned mesh + animation, simple directional-light forward shader, optional shadow map. Explicitly **not** photoreal — no PBR/IBL/post-stack
  - **Phase 3 — Terrain (defer):** `DiaTerrain` heightmap terrain. Out of scope for the current research; specced when Phase 2 is closing
- **SFML stays as window + input owner** throughout Phases 1 and 2. Replacing the window/input layer (likely SDL) is a separate, later research session

### DiaBgfx architectural commitments (do not mirror DiaSFML's mistakes)

- **No conflation of concerns.** `DiaBgfx::Canvas` implements `ICanvas` **only**. `IWindow` and `IInputSource` stay in `DiaSFML` for now. At app wire-up, the SFML-owned window hands its native HWND to `DiaBgfx::Canvas` for swapchain creation
- **No visitor pattern in the production render path.** `DiaBgfx::Canvas::ProcessFrame` consumes `FrameData` directly via internal sub-renderers (`SpriteRenderer`, `DebugRenderer`, `UIOverlayRenderer`). Visitors retained only where there is a non-rendering consumer (e.g. test mocks)
- **No backend types in public surface.** `DiaGraphics::ITexture` and `DiaGraphics::IShader` are the public handle types; `DiaBgfx::TextureHandle` / `ShaderHandle` are opaque concrete impls. No `bgfx::TextureHandle*` leaks across DiaBgfx's public boundary (PD-004 spirit)
- **StringCRC-keyed resources.** Texture and shader lookup keyed by `StringCRC`, not `unsigned int` (PD-001). This is a regression to fix during the swap
- **Internal layout:**
  ```
  Dia/DiaBgfx/
  ├── Canvas.h/.cpp                 # ICanvas impl
  ├── Renderers/
  │   ├── SpriteRenderer.h/.cpp     # EntityFrameData → batched draws
  │   ├── DebugRenderer.h/.cpp      # DebugFrameData → dynamic vertex buffers
  │   └── UIOverlayRenderer.h/.cpp  # Implements DiaUI's overlay surface
  ├── Resources/
  │   ├── TextureHandle.h/.cpp      # ITexture impl
  │   └── ShaderHandle.h/.cpp       # IShader impl
  ├── Shaders/                       # .sc shader sources cooked by shaderc
  ├── BgfxImGuiBackend.h/.cpp       # IImGuiBackend impl
  └── dia.bgfx.architecture.module.md
  ```

### Async texture loading coordination

- An async texture loader is in flight in parallel
- **Sequencing rule:** finish async loading on top of `DiaGraphics::ITexture` *first*, then `DiaBgfx::TextureHandle` implements `ITexture` and the async path lights up on bgfx for free. Inverting the order makes the async work handle both backends mid-flight
- Decode (PNG/JPG → RGBA bytes) happens on a worker pool — backend-agnostic. Upload happens on the bgfx render thread via a callback the renderer registers for
- Quick audit needed: confirm no code outside `DiaSFML` touches `sf::Texture` directly — if so, that's a parity blocker

### Public API discipline

- `Graphics::ICanvas` and `FrameData` remain the only surface DiaGraphics consumers see (PD-004)
- bgfx prebuilt (CMake/GENie) and consumed via `.vcxproj` references; no top-level CMake-of-CMakes (PD-006)
- bgfx `shaderc` invoked from `DiaPipelineEditor` as a cook step
- Default bgfx backend on Windows: deferred to spec time (D3D11 vs D3D12)

### DiaCLI env

- `dia env setup` acquires bgfx (clone, build via GENie/CMake, install prebuilt libs)
- `dia env verify` confirms prebuilt bgfx libs are present and valid
- Owned by its own feature spec under DiaCLI

### Phase 2 module decomposition (specced now, implemented later)

Mirrors the existing 2D module family (`DiaRig2D`, `DiaAnimation2D`, `DiaIK2D`, `DiaSkinning` implicit):

| Module | Role |
|--------|------|
| `DiaGraphics` | Gains 3D types: `Camera3D`, `Light`, `Mesh3DDrawCommand`, `Mesh3DFrameData` |
| `DiaMesh3D` (new) | glTF 2.0 static mesh loading, mesh asset type |
| `DiaRig3D` (new) | Skeleton + joint hierarchy (mirrors DiaRig2D) |
| `DiaAnimation3D` (new) | glTF animation curves, sampling, playback (mirrors DiaAnimation2D) |
| `DiaSkinning3D` (new) | Vertex skinning, GPU skinning data prep |
| `DiaScene3D` (new) | Scene-graph: Camera + Lights + Renderables as a unit |
| `DiaBgfx` | Gains 3D rendering: mesh, skinned mesh, simple forward + shadow shader |
| Future `DiaIK3D` | Out of scope for Phase 2 |

**Asset format:** glTF 2.0 (runtime parse for now; cook step deferred unless perf demands it).

## Spec map

**One system spec:** `docs/specs/systems/render_backend.md` — covers bgfx adoption and both phases coherently.

**Phase 1 feature specs (implement now):**
1. `bgfx-env-setup` — DiaCLI env install + verify for bgfx
2. `diaui-render-overlay-surface` — extract renderer-agnostic UI overlay from DiaSFML into DiaUI
3. `diabgfx-canvas-parity` — `DiaBgfx::Canvas` implements ICanvas with sprite + debug + UI parity
4. `diabgfx-imgui-backend` — IImGuiBackend impl on bgfx
5. `diabgfx-texture-pipeline` — ITexture refactor to StringCRC handles + bgfx impl (coordinates with async loader)
6. `diasfml-render-removal` — delete render path from DiaSFML, keep window+input

**Phase 2 feature specs (spec now, implement later):**
1. `diagraphics-3d-types` — 3D types added to DiaGraphics
2. `diamesh3d` — new module, glTF static mesh
3. `diarig3d` — new module, skeleton/joint hierarchy
4. `diaanimation3d` — new module, glTF animation curves
5. `diaskinning3d` — new module, vertex skinning + GPU skinning data
6. `diascene3d` — new module, scene-graph (Camera + Lights + Renderables)
7. `diabgfx-3d-renderers` — mesh, skinned mesh, simple forward + shadow on bgfx

**Phase 3 (deferred, no specs yet):** `DiaTerrain`.

## Open Items for Spec Phase

- Default bgfx backend on Windows (D3D11 vs D3D12) — pick during system spec
- Phase 1 acceptance: which specific tests / visual debuggers form the parity gate
- ImGui backend regression strategy: ship `BgfxImGuiBackend` alongside parity, retire `SFMLImGuiBackend` when SFML render path is deleted
- Async-loader audit: confirm no code outside `DiaSFML` touches `sf::Texture` directly

## Next Step

Run `/spec-system` for `render_backend`, then `/spec-feature` for each of the 6 Phase 1 + 7 Phase 2 features. Attach `summary.md` to each.
