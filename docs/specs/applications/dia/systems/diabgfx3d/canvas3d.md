# Feature Spec: canvas3d

**Parent:** [diabgfx3d.md](diabgfx3d.md)
**Cross-cutting:** [render-backend.md](../render-backend/render-backend.md) — Phase 2 ship gate (RB-002)

**Hard dependencies:**
- `gpu-resources` (this system) — `MaterialRegistry`, `MeshGpuCache`
- `3d-renderers` (this system) — `MeshRenderer`, `SkinnedMeshRenderer`, `ShadowRenderer`
- `canvas-parity` (DiaBgfx, Phase 1) — `Dia::Bgfx::Canvas` exists
- `imgui-backend` (DiaBgfx, Phase 1) — ImGui backend wired
- `graphics-3d-types` — `FrameData3D` is the 2D+3D frame packet (G3D-002)
- `scene-graph` (DiaScene3D) — populates `FrameData3D` for CluicheTest demo

**Status:** Approved

## Summary

`Dia::Bgfx3D::Canvas3D` extends `Dia::Bgfx::Canvas`, owns the 3D sub-renderers and shadow framebuffer, and dispatches the full 2D+3D frame in the correct pass order. It is the entry point for Phase 2 rendering. This feature also wires up the CluicheTest 3D demo scene and the `DiaBgfx3D.vcxproj` static library project — closing the Phase 2 ship gate (RB-002).

## Goals

- `Dia::Bgfx3D::Canvas3D : DiaBgfx::Canvas` — new module entry point; `ProcessFrame(const FrameData3D&)` dispatches shadow → mesh → skinned → inherited 2D passes → ImGui EndFrame
- `GetMaterialRegistry()` exposes the registry for app-side material registration
- `Canvas3D` owns `MeshRenderer`, `SkinnedMeshRenderer`, `ShadowRenderer`, `MeshGpuCache`, and `MaterialRegistry` by value/pointer; creates them in constructor, destroys in destructor (calls `MeshGpuCache::DestroyAll`)
- Base `ICanvas::ProcessFrame(const FrameData&)` remains satisfied by the inherited `DiaBgfx::Canvas` implementation — `Canvas3D` does not break existing callers
- `DiaBgfx3D.vcxproj` static library registered in `Cluiche.sln`
- `dia.bgfx3d.architecture.module.md` YAML module doc created
- CluicheTest gains a 3D demo scene: loads a glTF asset, drives `AnimationComponent3D::Update` each tick, renders via `Canvas3D`; accessible via `dia run cluichetest --3d-demo`
- GoogleTests for `MaterialRegistry` and `MeshGpuCache` run green (from `gpu-resources`)
- All existing 2D tests remain green after `Canvas3D` is introduced

## Binding Decisions

- **BG3-001** — `DiaBgfx3D` is a separate module from `DiaBgfx`; 2D-only games do not pull in the 3D chain
- **BG3-002** — `Canvas3D : DiaBgfx::Canvas`; 2D passes inherited unchanged
- **BG3-003** — `Canvas3D::ProcessFrame` accepts `FrameData3D`, not the base `FrameData`; base overload still satisfied by inheritance
- **BG3-010** — Phase 2 ship gate: CluicheTest renders a glTF skinned character animated at 60 FPS (64 characters target); all 2D tests remain green
- **BG3-011** — Namespace `Dia::Bgfx3D::`
- **G3D-002** — `FrameData3D` is the 2D+3D frame packet; accepted by `Canvas3D::ProcessFrame`
- **RB-004** — `Canvas3D` implements `ICanvas` only; no window or input surface added

## Public Interface

```cpp
// Dia/DiaBgfx3D/Canvas3D.h
namespace Dia { namespace Bgfx3D {

class Canvas3D : public Dia::Bgfx::Canvas
{
public:
    Canvas3D();
    ~Canvas3D() override;

    // Extended entry point — dispatches shadow → mesh → skinned → inherited 2D passes.
    void ProcessFrame(const Dia::Graphics3D::FrameData3D& frameData);

    // App wire-up: register materials before rendering begins.
    MaterialRegistry* GetMaterialRegistry();

private:
    unsigned short       mMeshViewId;
    unsigned short       mShadowViewId;
    MeshRenderer*        mMeshRenderer;
    SkinnedMeshRenderer* mSkinnedMeshRenderer;
    ShadowRenderer*      mShadowRenderer;
    MeshGpuCache*        mMeshGpuCache;
    MaterialRegistry     mMaterialRegistry;
    unsigned short       mShadowFramebuffer;   // bgfx::FrameBufferHandle::idx
};

} }
```

### Frame pass order (`Canvas3D::ProcessFrame`)

1. `mShadowRenderer->RenderShadowMap(frameData)` — depth-only from light POV (`mShadowViewId`)
2. `mMeshRenderer->Draw(frameData)` — static meshes (`mMeshViewId`)
3. `mSkinnedMeshRenderer->Draw(frameData)` — skinned meshes (`mMeshViewId`)
4. `mSpriteRenderer->Draw(...)` — 2D sprites (inherited)
5. `mDebugRenderer->Draw(...)` — debug primitives (inherited)
6. `mUIOverlayRenderer->Composite(...)` — UI overlay (inherited)
7. ImGui `EndFrame` (inherited)
8. `bgfx::frame()`

## Acceptance Criteria

1. `Canvas3D::ProcessFrame(const FrameData3D&)` compiles and dispatches all seven passes in the order above
2. Inherited `DiaBgfx::Canvas::ProcessFrame(const FrameData&)` still links and works for callers that do not know about 3D
3. `GetMaterialRegistry()` returns a non-null pointer after construction
4. `MeshGpuCache::DestroyAll()` is called in `Canvas3D` destructor; no GPU resource leak on shutdown
5. `DiaBgfx3D.vcxproj` builds as a static library in `Debug|x64` and `Release|x64`; registered in `Cluiche.sln`
6. `dia.bgfx3d.architecture.module.md` exists and passes `python Tools/dia_modules.py --validate`
7. `dia run cluichetest --3d-demo` launches CluicheTest; a glTF skinned character renders and animates (visually verified)
8. Performance bar: 64 visible animated characters at ≤ 16 ms median frame time on D3D11 (dev hardware)
9. `dia run googletest` is fully green; no regressions in 2D test suites
10. No bgfx types appear in any `DiaBgfx3D` public header (AC from `3d-renderers` and `gpu-resources` reinforced here at integration)
11. ImGui debug overlay continues to function alongside 3D rendering (layer toggles work while 3D scene renders)
12. **Phase 2 ship gate (RB-002):** light 3D renders end-to-end; `diabgfx3d.md` system spec flips to `Done`

## Tasks

| # | Task | Status |
|---|------|--------|
| 1 | `Canvas3D.h/.cpp` — constructor wires MaterialRegistry; ProcessFrame delegates 2D passes; 3D renderer slots no-op until upstream ships | Done |
| 2 | `DiaBgfx3D.vcxproj` + `.vcxproj.filters`; register in `Cluiche.sln` | Done |
| 3 | `dia.bgfx3d.architecture.module.md` YAML module doc | Done |
| 4 | CluicheTest 3D demo scene — default material, glTF asset, AnimationComponent3D drive, Canvas3D swap | Blocked — needs DiaMesh3D, DiaScene3D |
| 5 | Verify: `dia run cluichetest --3d-demo` renders skinned character; `dia run googletest` green | Blocked — needs Task 4 |
