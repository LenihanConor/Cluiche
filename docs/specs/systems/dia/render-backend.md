# System Spec: RenderBackend

## Parent Application
@docs/specs/applications/dia.md

**Research:** @docs/research/render_backend_swap/summary.md

**Status:** `Approved`

---

## Purpose

RenderBackend is the system that owns the concrete rendering implementation behind `DiaGraphics::ICanvas`. Today that role is played by `DiaSFML`, which conflates renderer + window + input under a single SFML-shaped facade. This system replaces the SFML render path with **bgfx** as a new `DiaBgfx` module, in two phases:

- **Phase 1 — Parity** (implement first): `DiaBgfx::Canvas` reaches functional parity with `DiaSFML` for sprites, debug primitives, UI overlay, and ImGui. The SFML render path is then deleted. SFML keeps `IWindow` + `IInputSource` ownership unchanged.
- **Phase 2 — Light 3D** (specced now, implemented later): adds static + skinned mesh, glTF 2.0 asset loading, simple directional-light forward shading, and one shadow map. Decomposed into the 3D mirror of the existing 2D module family (`DiaMesh3D`, `DiaRig3D`, `DiaAnimation3D`, `DiaSkinning3D`, `DiaScene3D`) with `DiaBgfx` adding 3D sub-renderers.

Phase 3 (`DiaTerrain`) is deferred and outside this system's scope; it will be specced when Phase 2 is closing.

**Architectural role:**

```
Simulation / VisualDebuggers / UI
    ↓  fills FrameData + (Phase 2) Mesh3DFrameData
DiaGraphics (ICanvas, FrameData, draw command types)
    ↓  ProcessFrame
DiaBgfx::Canvas (Sprite/Debug/UIOverlay/(Phase 2) Mesh3D/SkinnedMesh sub-renderers)
    ↓  bgfx::frame()
GPU (D3D11 / D3D12 / Vulkan via bgfx)

DiaSDL (window + input only) ──► HWND ──► DiaBgfx swapchain
```

**Why bgfx over alternatives:** see `choose.md` — Filament's full PBR pipeline is overkill for the stated "light 3D, no photorealism" target; native D3D11/D3D12 sacrifices multi-backend insurance with no compensating Cluiche benefit; Sokol lacks D3D12/Vulkan and the draw-bucket scaffolding needed for `DebugFrameData` ergonomics.

---

## Responsibilities

### Phase 1 — Parity

- Provide `DiaBgfx::Canvas` — a new concrete `Graphics::ICanvas` implementation built on bgfx
- Provide internal sub-renderers (`SpriteRenderer`, `DebugRenderer`, `UIOverlayRenderer`) that consume `FrameData` directly without the visitor pattern
- Provide `DiaBgfx::TextureHandle` / `ShaderHandle` — opaque concrete impls of `Graphics::ITexture` / `Graphics::IShader`, keyed by `StringCRC`
- Provide `DiaBgfx::BgfxImGuiBackend` — concrete `IImGuiBackend` impl
- Extract a renderer-agnostic UI overlay surface from `DiaSFML` into `DiaUI`, with `DiaBgfx` as its first implementer
- Refactor `DiaGraphics::ITexture` to use `StringCRC` handles (regression fix from `unsigned int`)
- Add bgfx acquisition/build/verify to `DiaCLI` (`dia env setup`, `dia env verify`)
- Add a bgfx `shaderc` cook step to `DiaPipeline`
- Delete `DiaSFML` entirely (including render path, `IWindow` impl, and `IInputSource` impl); `IWindow` + `IInputSource` are now owned by `DiaSDL`.

### Phase 2 — Light 3D (specced now, implemented later)

- Provide `DiaGraphics3D` — new module: `Camera3D`, `Light` (directional, point), `Mesh3DDrawCommand`, `Mesh3DFrameData`, `FrameData3D`; `Dia::Graphics3D::` namespace; `DiaGraphics::FrameData` unchanged
- Provide `DiaMesh3D` — new module: glTF 2.0 static mesh loading, mesh asset type
- Provide `DiaRig3D` — new module: skeleton + joint hierarchy, mirrors `DiaRig2D`
- Provide `DiaAnimation3D` — new module: glTF animation curves + sampling, mirrors `DiaAnimation2D`
- Provide `DiaSkinning3D` — new module: vertex skinning, GPU skinning data prep
- Provide `DiaScene3D` — new module: scene-graph (Camera + Lights + Renderables as a unit)
- Provide `DiaBgfx3D` — new module: `Canvas3D` (extends `DiaBgfx::Canvas`), `MeshRenderer`, `SkinnedMeshRenderer`, `ShadowRenderer`, `MaterialRegistry`, `MeshGpuCache`, 3D shaders; `Dia::Bgfx3D::` namespace

## Non-Responsibilities

- **Window creation and management** — owned by `DiaSDL`; SDL migration complete
- **Input handling** — owned by `DiaSDL` / `DiaInput`; SDL migration complete
- **PBR rendering, IBL, post-processing, TAA** — explicitly out of scope; this system targets stylized / light 3D, not photorealism
- **Terrain (`DiaTerrain`)** — Phase 3, deferred
- **IK in 3D (`DiaIK3D`)** — out of scope; mirrors the deferred 2D feature set
- **CEF / Ultralight UI rendering** — those systems own their compositor; `DiaBgfx` only exposes the renderer-agnostic UI overlay surface they consume
- **Asset pipeline format design** — glTF 2.0 is the chosen mesh/anim format; cooked binary format is deferred unless Phase 2 perf demands it
- **Audio, fonts, image decoding** — separate from rendering; currently SFML-backed, removal is its own work

---

## Public Interfaces

### DiaBgfx::Canvas (Phase 1)

```cpp
// Dia/DiaBgfx/Canvas.h
namespace Dia::Bgfx {

class Canvas : public Dia::Graphics::ICanvas {
public:
    Canvas();
    ~Canvas() override;

    // ICanvas
    void Initialize(const Dia::Graphics::ICanvas::Settings& settings) override;
    void SetCanvasSize(const Dia::Maths::Vector2D& size) override;
    void SetActiveContext(bool active) override;

    void StartFrame(const Dia::Graphics::FrameData& nextFrame) override;
    void ProcessFrame(const Dia::Graphics::FrameData& nextFrame) override;
    void EndFrame(const Dia::Graphics::FrameData& nextFrame) override;

    // Bgfx-specific bring-up: caller (DiaSFML) supplies the native window handle.
    void AttachToNativeWindow(Dia::Window::SystemHandle hwnd,
                              const Dia::Maths::Vector2D& size);

    // Resource access
    Dia::Graphics::ITexture* GetTexture(Dia::Core::StringCRC id);
    Dia::Graphics::IShader*  GetShader(Dia::Core::StringCRC id);

private:
    SpriteRenderer*    mSpriteRenderer;
    DebugRenderer*     mDebugRenderer;
    UIOverlayRenderer* mUIOverlayRenderer;
    // ... bgfx state, swapchain, view ids
};

} // namespace Dia::Bgfx
```

### DiaUI renderer-agnostic UI overlay surface (Phase 1, extracted)

```cpp
// Dia/DiaUI/IRenderOverlay.h
namespace Dia::UI {

// Contract that any renderer (DiaBgfx, future renderers) implements to composite
// the UI buffer into the final framebuffer. Replaces the sf::Shader-based overlay
// path that lived inside DiaSFML::RenderWindow.
class IRenderOverlay {
public:
    virtual ~IRenderOverlay() = default;
    virtual void Composite(const Dia::UI::UIDataBuffer& uiBuffer) = 0;
};

} // namespace Dia::UI
```

### DiaGraphics::ITexture (Phase 1, refactored)

```cpp
// Dia/DiaGraphics/Assets/ITexture.h
namespace Dia::Graphics {

// StringCRC-keyed handle (PD-001). Replaces the unsigned int handle today.
class ITexture {
public:
    virtual ~ITexture() = default;
    virtual Dia::Core::StringCRC GetId() const = 0;
    virtual Dia::Maths::Vector2D GetSize() const = 0;
    virtual bool IsReady() const = 0;  // async-loader-friendly
};

} // namespace Dia::Graphics
```

### DiaGraphics 3D types (Phase 2)

```cpp
// Dia/DiaGraphics/Mesh3D/Mesh3DDrawCommand.h
namespace Dia::Graphics {

struct Mesh3DDrawCommand {
    Dia::Core::StringCRC meshId;
    Dia::Core::StringCRC materialId;
    Dia::Maths::Matrix44 transform;
    // skinning palette index — 0 if static
    uint32_t             skinningPaletteIndex;
};

struct Camera3D {
    Dia::Maths::Matrix44 view;
    Dia::Maths::Matrix44 projection;
};

struct DirectionalLight {
    Dia::Maths::Vector3D direction;
    Dia::Graphics::RGBA  colour;
    float                intensity;
};

class Mesh3DFrameData {
public:
    void RequestDraw(const Mesh3DDrawCommand& cmd);
    void SetCamera(const Camera3D& camera);
    void AddLight(const DirectionalLight& light);
    void Clear();
    // ... accessors
};

} // namespace Dia::Graphics
```

(Phase 2 module-level interfaces — `DiaMesh3D::Mesh`, `DiaRig3D::Skeleton`, `DiaAnimation3D::AnimationClip`, `DiaSkinning3D::SkinningPalette`, `DiaScene3D::Scene` — are designed in their respective feature specs.)

---

## Features

### Phase 1 — Parity (implement now)

| Feature | Description | Spec | Status |
|---------|-------------|------|--------|
| bgfx-env-setup | DiaCLI `env setup` / `env verify` acquires + builds + validates bgfx libs (extends DiaEnv with source-built `install_type:"build"`) | [bgfx-env-setup.md](../../features/dia/diaenv/bgfx-env-setup.md) | Approved |
| diaui-render-overlay-surface | Extract renderer-agnostic UI overlay (`IUIRenderOverlay`) from DiaSFML into DiaUI; `Dia::SFML::SfmlUIRenderOverlay` is the first impl | [render-overlay-surface.md](../../features/dia/diaui/render-overlay-surface.md) | Approved |
| diagraphics-texture-handle-stringcrc | Refactor `ITexture` to `StringCRC` handles; coordinates with async loader | [texture-handle-stringcrc.md](../../features/dia/diagraphics/texture-handle-stringcrc.md) | Approved |
| diabgfx-canvas-parity | `DiaBgfx::Canvas` implements `ICanvas` with sprite + debug + UI parity vs DiaSFML; new module `Dia/DiaBgfx/`; D3D11 default backend; six initial `.sc` shaders | [canvas-parity.md](../../features/dia/diabgfx/canvas-parity.md) | Done |
| diabgfx-imgui-backend | `DiaBgfx::BgfxImGuiBackend` implements `IImGuiBackend`; vendors bgfx upstream's reference imgui renderer; Win32 WndProc chain shim in DiaSFML for input | [imgui-backend.md](../../features/dia/diabgfx/imgui-backend.md) | Approved |
| diapipeline-shaderc-cook | bgfx `shaderc` integrated as a cook step in DiaPipeline | [bgfx-shader-cook.md](../../features/dia/diapipeline/bgfx-shader-cook.md) | Approved |
| diasfml-render-removal | Delete render path from DiaSFML; keep `IWindow` + `IInputSource`; move `TextureHandler` to DiaAssetRuntime; **Phase 1 ship gate (RB-016)** | [render-removal.md](../../features/dia/diasfml/render-removal.md) | Done |
| replace-diasfml-with-sdl3 | Replace DiaSFML window+input with DiaSDL; delete `Dia/DiaSFML/` entirely | [replace-diasfml-with-sdl3.md](../../features/dia/diasdl/replace-diasfml-with-sdl3.md) | In Progress |

### Phase 2 — Light 3D (specced now, implemented later)

| Feature | Description | Spec | Status |
|---------|-------------|------|--------|
| diagraphics3d-types | `Camera3D`, `Light`, `Mesh3DDrawCommand`, `Mesh3DFrameData`, `FrameData3D` in new DiaGraphics3D module; `Dia::Graphics3D::` namespace; DiaGraphics::FrameData unchanged (G3D-003) | [graphics-3d-types.md](../../features/dia/diagraphics3d/graphics-3d-types.md) | Approved |
| diamesh3d | New module — glTF 2.0 static + skinned mesh loading via cgltf; `Mesh3DAsset` carries vertex/index/submesh data + AABB; consumes DiaMaths + DiaGeometry3D | [mesh-asset-and-loader.md](../../features/dia/diamesh3d/mesh-asset-and-loader.md) | Approved |
| diarig3d | New module — Bone3D + Skeleton3D + Pose3D + glTF/JSON loader + SkeletonComponent3D; FK produces Matrix34 skinning palette; mirrors DiaRig2D | [skeleton-and-pose.md](../../features/dia/diarig3d/skeleton-and-pose.md) | Approved |
| diaanimation3d | New module — `AnimationClip3D` + `ClipPlayer3D` + glTF loader; STEP/LINEAR/CUBICSPLINE; samples into Pose3D; `AnimationComponent3D` ties to SkeletonComponent3D | [clip-and-player.md](../../features/dia/diaanimation3d/clip-and-player.md) | Approved |
| diaskinning3d | New module — `SkinningManager` produces per-frame Matrix34 palettes from posed Skeleton3D; index referenced by Mesh3DDrawCommand | [skinning-palette.md](../../features/dia/diaskinning3d/skinning-palette.md) | Approved |
| diascene3d | New module — flat-list scene with Transform3D parent chains; `Submit(scene, frameData)` performs frustum culling + skinning palette assignment + draw command emission | [scene-graph.md](../../features/dia/diascene3d/scene-graph.md) | Approved |
| diabgfx3d-3d-renderers | `Canvas3D`, `MeshRenderer`, `SkinnedMeshRenderer`, `ShadowRenderer`, `MaterialRegistry`, `MeshGpuCache`; six 3D `.sc` shaders; lambert + ambient + single-cascade directional shadow; `Dia::Bgfx3D::` namespace; new `Dia/DiaBgfx3D/` module; **Phase 2 ship gate** | [3d-renderers.md](../../features/dia/diabgfx3d/3d-renderers.md) | Approved |

**Natural build order (Phase 1):** bgfx-env-setup → diagraphics-texture-handle-stringcrc → diaui-render-overlay-surface → diapipeline-shaderc-cook → diabgfx-canvas-parity → diabgfx-imgui-backend → diasfml-render-removal.

**Natural build order (Phase 2):** diagraphics-3d-types → diamesh3d → diarig3d → diaanimation3d → diaskinning3d → diascene3d → diabgfx-3d-renderers.

---

## Dependencies on Other Systems

**Required:**
- **DiaGraphics** — `ICanvas`, `FrameData`, `DebugPrimitive`, `SpriteDrawCommand`, `ITexture`, `IShader`, `RGBA`; extended in Phase 1 (texture handle refactor) and Phase 2 (3D types)
- **DiaCore** — `StringCRC`, `DynamicArrayC`, memory utilities
- **DiaMaths** — `Vector2D`, `Vector3D`, `Matrix44`, transforms (Phase 2)
- **DiaUI** — extended in Phase 1 with `IRenderOverlay`; consumes that surface from `DiaBgfx`
- **DiaSDL** — provides the native window handle (`IWindow::GetSystemHandle()`); `IInputSource` unchanged
- **DiaInput** — `IInputSource` unchanged
- **DiaImGui** — `IImGuiBackend` interface; `DiaBgfx::BgfxImGuiBackend` implements it
- **DiaCLI / DiaEnv** — `dia env setup` / `dia env verify` extended for bgfx
- **DiaPipeline** — bgfx `shaderc` invoked as a cook step
- **DiaAssetCatalogue / DiaAssetRuntime** — texture loaders produce `DiaBgfx::TextureHandle` behind `ITexture`
- **DiaApplicationFlow** — `Render` ProcessingUnit lifecycle calls `ICanvas` unchanged

**External:**
- **bgfx** (BSD 2-clause) + **bx**, **bimg** — new external dependency, prebuilt via GENie/CMake, consumed via `.vcxproj` references (PD-006). Phase 1.
- **shaderc** (bgfx tooling) — new build-time dependency invoked by DiaPipeline. Phase 1.
- **cgltf** (MIT, single-header) — proposed glTF 2.0 loader for Phase 2 (`DiaMesh3D`, `DiaAnimation3D`); confirmed in Phase 2 feature specs.

**Explicitly excluded:**
- **Filament, Diligent, Sokol, SDL_GPU, D3D11/D3D12 native** — ruled out in research
- **PBR / IBL / post-processing libraries** — out of scope
- **DiaTerrain** — Phase 3, separate spec

**Dependents:**
- **CluicheTest** — render output via `DiaBgfx::Canvas`
- **CluicheEditor** — render output via `DiaBgfx::Canvas` (multi-window/multi-swapchain confirmed during Phase 1)
- **DiaVisualDebugger** — emits `DebugFrameData` consumed by `DiaBgfx::DebugRenderer`
- All `DiaXxxxVisualDebugger` modules (rig2d, rigidbody2d, softbody2d, ik2d, geometry2d, animation2d) — feed `FrameData` consumed by DiaBgfx

---

## Out of Scope

- Photorealistic rendering (PBR, IBL, TAA, deferred) — Filament-class capability not needed; brief is "light 3D, no photorealism"
- Window/input migration to SDL — complete; DiaSDL now owns `IWindow` + `IInputSource`
- Terrain rendering (`DiaTerrain`) — Phase 3, deferred
- 3D IK (`DiaIK3D`) — mirrors the deferred 2D scope
- Cooked binary mesh/anim format — defer; Phase 2 starts with runtime glTF parsing
- Multiple light types beyond directional + point — Phase 2 covers the simple cases only
- Compute-based skinning — Phase 2 uses vertex-shader skinning; compute is a later optimisation
- Hot-reload of shaders/materials — out of scope for this system; existing pipeline cadence applies
- Cross-platform support — PD-005 binds to Windows x64; Metal/Linux backends are not pursued

---

## Decisions

| ID | Decision | Rationale | Scope | Status | Binding |
|----|----------|-----------|-------|--------|---------|
| RB-001 | Adopt bgfx as the GPU abstraction (not Filament, Diligent, Sokol, SDL_GPU, D3D11/12 native) | Production-proven; multi-backend (D3D11/12/Vulkan); strong debug-draw ergonomics; right level of abstraction for "light 3D, no photorealism" target | RenderBackend system | Accepted | Yes |
| RB-002 | Two-phase delivery: Phase 1 (parity) first, Phase 2 (light 3D) after Phase 1 ships | Phase 1 alone touches enough surfaces (ICanvas, IImGuiBackend, asset loaders, UI overlay shader, DebugFrameRenderer, EntityFrameRenderer) that bundling 3D risks scope cut or drift; hard ship gate at "SFML render path deletable" | RenderBackend system | Accepted | Yes |
| RB-003 | Phase 2 specs are written up-front but stay `Approved` (not `In Progress`) until Phase 1 ships | "Spec everything now, implement Phase 1 only" — captures architectural decisions while keeping implementation focus | RenderBackend system | Accepted | Yes |
| RB-004 | `DiaBgfx::Canvas` implements `ICanvas` only — no `IWindow` / `IInputSource` conflation | DiaSFML conflates because SFML conflates; bgfx is renderer-only and the new module must reflect that. `IWindow` + `IInputSource` stay in DiaSFML during both phases | DiaBgfx | Accepted | Yes |
| RB-005 | No visitor pattern in the production render path; sub-renderers consume `FrameData` directly | Visitors in DiaSFML add indirection without payoff (one operation per data type). Direct sub-renderers (Sprite/Debug/UIOverlay) are simpler. Visitors retained only for non-rendering consumers (e.g. test mocks) | DiaBgfx | Accepted | Yes |
| RB-006 | No bgfx types in DiaGraphics public surface; `bgfx::TextureHandle` etc. opaque to consumers | PD-004 spirit — bgfx is an implementation detail. `DiaBgfx::TextureHandle` implements `Graphics::ITexture`; consumers see only the interface | DiaBgfx, DiaGraphics | Accepted | Yes |
| RB-007 | `Graphics::ITexture` and `Graphics::IShader` keyed by `StringCRC`, not `unsigned int` | Aligns with PD-001; current `unsigned int` handles are a regression. Refactor happens in Phase 1 alongside async-loader work | DiaGraphics | Accepted | Yes |
| RB-008 | UI overlay path extracted into renderer-agnostic `IRenderOverlay` in `DiaUI`; `DiaBgfx` is the first implementer | Today the UI overlay shader lives inside `DiaSFML::RenderWindow` — that path must move during Phase 1. Extracting it to `DiaUI` future-proofs DiaUICEF / DiaUIUltralight to share the abstraction | DiaUI, DiaBgfx | Accepted | Yes |
| RB-009 | Async texture loading sequencing: ITexture refactor first, then DiaBgfx::TextureHandle implements it | Inverting the order forces the parallel async-loader work to handle both backends mid-flight; doing it this way makes async light up on bgfx for free | DiaGraphics, DiaBgfx, async-loader work | Accepted | Yes |
| RB-010 | bgfx prebuilt via GENie/CMake; consumed via `.vcxproj` references | PD-006 — Visual Studio project files are source of truth; no top-level CMake-of-CMakes | Build integration | Accepted | Yes |
| RB-011 | bgfx `shaderc` invoked as a cook step in DiaPipeline | Shader pipeline must be a first-class concern; ad-hoc shader compilation is excluded | DiaPipeline, DiaBgfx | Accepted | Yes |
| RB-012 | Default bgfx backend on Windows: deferred decision (D3D11 vs D3D12) — pick during `diabgfx-canvas-parity` feature | Both are viable; the call should be made when the parity work has surfaced any backend-specific concerns | DiaBgfx | Proposed | Yes |
| RB-013 | Phase 2 module decomposition mirrors the existing 2D family: `DiaRig3D` / `DiaAnimation3D` / `DiaSkinning3D` (not bundled) | Matches `DiaRig2D` / `DiaAnimation2D` / `DiaIK2D` precedent; keeps each module's responsibility tight; aligns with single-responsibility module principle | Phase 2 | Accepted | Yes |
| RB-014 | Phase 2 introduces `DiaScene3D` for scene-graph (Camera + Lights + Renderables) | 2D works without a scene abstraction (raw FrameData drops); 3D needs camera + lights bundled coherently with renderables, especially for shadow-map setup. New concept justified in 3D | DiaScene3D | Accepted | Yes |
| RB-015 | glTF 2.0 is the Phase 2 mesh + skinning + animation asset format (runtime parse, no cook step initially) | Industry-standard, well-tooled, has skinning + morph + animation built in. Cooked binary format deferred unless perf demands it | DiaMesh3D, DiaAnimation3D | Accepted | Yes |
| RB-016 | Phase 1 ship gate: SFML render path deletable. All existing visual debuggers + tests green on bgfx. ImGui regression-free. UI overlay regression-free | Hard checkpoint before any 3D risk is taken on; gives clean rollback / pivot point if bgfx integration disappoints | Phase 1 | Accepted | Yes |
| RB-017 | DiaCLI `env setup` / `env verify` must learn bgfx as a first-class concern | bgfx is a new external dep with a non-trivial build (GENie/CMake); env tooling cannot leave acquisition manual | DiaEnv | Accepted | Yes |
| RB-018 | Phase 3 (`DiaTerrain`) is out of scope for this system spec | Specced separately when Phase 2 closes; keeps this system bounded | RenderBackend system | Accepted | Yes |

---

## Inherited Binding Decisions

| ID | Source | Decision | Implication for this system |
|----|--------|----------|-----------------------------|
| PD-001 | Platform | StringCRC for all entity/component IDs | `ITexture` and `IShader` handles refactored to `StringCRC` (RB-007); all bgfx-side resource lookup keyed by `StringCRC`; layer/view names use `StringCRC` |
| PD-002 | Platform | ProcessingUnit/Phase/Module architecture | `DiaBgfx::Canvas` initialised in a Phase, lives inside the Render `ProcessingUnit`; `bgfx::frame()` called from `EndFrame()`; cross-thread record/submit respects Phase boundaries |
| PD-003 | Platform | Component-based entities | Sprite/Mesh/Light/Camera become `IComponent`s in higher-level systems (CluicheTest); the backend itself is below this — but its API accepts frame data assembled from components |
| PD-004 | Platform | No STL containers in public APIs | DiaBgfx public surface uses `DynamicArrayC`, no `std::vector` / `std::string`. bgfx types do **not** leak through `Graphics::ITexture` / `Graphics::IShader` (RB-006). Internal implementation may use STL freely |
| PD-005 | Platform | x64 only | All new vcxprojs target x64; bgfx prebuilt as x64; no Win32/ARM concern |
| PD-006 | Platform | Visual Studio project files are source of truth | bgfx prebuilt via its own GENie/CMake step orchestrated by DiaCLI; consumed via `.vcxproj` references with prebuilt libs (RB-010); no top-level CMake. New `.vcxproj` files: `DiaBgfx.vcxproj` (Phase 1), `DiaMesh3D/Rig3D/Animation3D/Skinning3D/Scene3D.vcxproj` (Phase 2) |
| PD-007 | Platform | C++20 required | All new code under `/std:c++20`; bgfx public headers verified to compile cleanly under C++20 during `bgfx-env-setup` |
| PD-008 | Platform | `Directory.Build.props` owns OutDir/IntDir/toolchain | New vcxprojs inherit; no per-project overrides. bgfx prebuilt libs land in a path consumable by Directory.Build.props rules |
| PD-009 | Platform | Generated output under `Cluiche/out/<AppName>/` | Cooked shader binaries (from `shaderc`) land under `Cluiche/out/<AppName>/shaders/` |
| PD-010 | Platform | `.diagame` / `.diastage` typed imports route loading | Phase 2 mesh/animation assets resolve through the standard manifest import path, not direct file paths |
| AD-001 | Dia App | Module YAML frontmatter documentation | `dia.bgfx.architecture.module.md` (Phase 1); `dia.mesh3d/rig3d/animation3d/skinning3d/scene3d.architecture.module.md` (Phase 2) |
| AD-002 | Dia App | No STL containers in public APIs | Reinforces PD-004 |
| AD-003 | Dia App | Namespace `Dia::<Module>::` | `Dia::Bgfx::` for DiaBgfx; `Dia::Mesh3D::`, `Dia::Rig3D::`, `Dia::Animation3D::`, `Dia::Skinning3D::`, `Dia::Scene3D::` for Phase 2 modules |
| AD-004 | Dia App | ProcessingUnit/Phase/Module for app structure | Reinforces PD-002 |
| AD-005 | Dia App | Component-based entities | Phase 2 mesh/skinned-mesh integration via `IComponent` in higher-level systems |

---

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Phase 1 ship gate | What concretely qualifies as "parity" — every visual debugger green, every CluicheTest scene rendering identically, every test passing? | All three. The Phase 1 ship gate (RB-016) is: (a) every existing visual debugger renders correctly on `DiaBgfx`, (b) CluicheTest's scenes look indistinguishable from the SFML output (manual visual diff acceptable for this gate), (c) all GoogleTests + e2e harness scenarios pass, (d) ImGui debug overlays work, (e) UI overlay compositing works. Listed precisely in the `diasfml-render-removal` feature spec. |
| 2 | Window handle handoff | How does `DiaSFML` hand the HWND to `DiaBgfx` without circular coupling? | `Dia::Window::IWindow::GetSystemHandle()` already returns the HWND. At app wire-up, the application module (CluicheTest, CluicheEditor) constructs the `DiaSFML::Window` first, then constructs `DiaBgfx::Canvas` and calls `Canvas::AttachToNativeWindow(window->GetSystemHandle(), size)`. No new coupling — `DiaBgfx` depends on `DiaWindow` (interface), not `DiaSFML` (impl). |
| 3 | Multi-window | CluicheEditor uses CEF; CluicheTest is a single window. Does Phase 1 need multi-swapchain support? | Yes — CluicheEditor renders editor chrome via CEF and game viewport via DiaBgfx. Phase 1 must support at least two simultaneous swapchains on bgfx. Confirmed feasible (bgfx multi-view + multiple framebuffers). Captured as a `diabgfx-canvas-parity` acceptance criterion. |
| 4 | Async loader collision | The user has async texture loading in flight. What if it lands before DiaBgfx? | Per RB-009: ITexture refactor (StringCRC handles) is the *first* Phase 1 feature, and async-loader work targets `Graphics::ITexture` only. Audit captured: confirm nothing outside `DiaSFML` touches `sf::Texture` directly — if so, those callsites must be lifted to `ITexture` before Phase 1 starts. |
| 5 | Visitor removal | DiaGraphics defines `DebugFrameDataVisitor` as a public interface. Removing visitor usage in DiaBgfx doesn't affect DiaGraphics' public API, but does anyone else implement the visitor today? | Only `DiaSFML::DebugFrameRendererVisitor` and test mocks (`Dia/DiaGraphics/Testing/MockVisitors.h`) implement it today. `DiaBgfx::DebugRenderer` consumes `DebugFrameData` directly, bypassing the visitor. The visitor type stays in DiaGraphics for the test mocks; DiaSFML's impl is deleted at Phase 1 ship gate. |
| 6 | Phase 2 module count | Five new modules in Phase 2 (Mesh3D / Rig3D / Animation3D / Skinning3D / Scene3D) plus DiaGraphics extensions plus DiaBgfx 3D renderers — is this too many? | Mirrors the existing 2D family precedent (DiaRig2D / DiaAnimation2D / DiaIK2D). Each has a distinct responsibility — bundling them would conflate skeleton authoring with animation playback with skinning evaluation. The module count is the right grain for testability and module-graph hygiene. |
| 7 | DiaScene3D necessity | 2D doesn't have a `DiaScene2D` — why does 3D need one? | 2D draws into raw FrameData with no camera; the orthographic projection is implicit. 3D needs Camera + Lights bundled with Renderables coherently for view-dependent culling, light gathering, and shadow-map setup. The asymmetry is justified by the asymmetry between 2D and 3D rendering — 3D cannot be expressed as an unstructured draw-command stream the way 2D can. |
| 8 | bgfx default backend | RB-012 defers the D3D11 vs D3D12 default decision. Is that risk acceptable? | Yes — both are supported by bgfx; switching is a one-line `bgfx::Init` change. The decision benefits from being made *after* parity work has surfaced any backend-specific concerns (e.g. D3D12 descriptor heap pressure with many UI sprites, or D3D11 driver-thread quirks with multi-swapchain). Capture as an open question on the `diabgfx-canvas-parity` feature spec. |
| 9 | DiaSFML residual content | After Phase 1, what remains in DiaSFML? | Nothing — DiaSFML has been deleted entirely. `IWindow` + `IInputSource` are now owned by `DiaSDL`. |
| 10 | shaderc cook integration | `shaderc` is a per-shader compile step; how does DiaPipeline orchestrate it across multiple bgfx backends? | `shaderc` outputs per-backend binary blobs (one for each enabled bgfx backend). DiaPipeline cooks each `.sc` file once per active backend (D3D11, D3D12, Vulkan, GL) into `Cluiche/out/<App>/shaders/<backend>/<shader>.bin`. At runtime, DiaBgfx selects the matching folder for the active bgfx renderer. Captured as a `diapipeline-shaderc-cook` feature spec acceptance criterion. |
| 11 | TextureHandle StringCRC migration | If `ITexture` is keyed by StringCRC, what's the source of truth for the StringCRC? Asset name? File path? | Asset catalogue ID — same as every other Dia asset (PD-001). The asset catalogue maps `StringCRC(asset.name)` to file paths; texture loaders use the StringCRC directly. Captured in `diagraphics-texture-handle-stringcrc` feature spec. |
| 12 | Phase 2 status timing | When Phase 2 specs are written but Phase 1 is `In Progress`, what status do Phase 2 features carry? | `Approved` per RB-003 — the specs are reviewed and locked, but no implementation is in flight. When Phase 1 ships and Phase 2 work begins, individual feature specs flip to `In Progress`. The system spec stays `In Progress` throughout both phases; it only goes `Done` when Phase 2 ships. |

---

## Status

`Approved` — All 5 spec steps complete. System cannot be marked `Done` until all 14 child feature specs are `Approved`. Phase 1 features implement first; Phase 2 features stay `Approved` until Phase 1 ships (RB-003).
