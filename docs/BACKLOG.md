# Cluiche Backlog

Derived from spec status across `docs/specs/`. When a spec moves to Done, move it to [BACKLOG-HISTORY.md](BACKLOG-HISTORY.md).

---

## Ready to Build (Approved → implement)

These specs are `Approved` with all features `Approved`. No spec work needed — go straight to implementation.

### Systems

| System | Spec | Features | Depends On |
|--------|------|----------|------------|
| DiaBgfx3D | [diabgfx3d.md](specs/applications/dia/systems/diabgfx3d/diabgfx3d.md) | `gpu-resources` — MaterialRegistry + MeshGpuCache ✅ done. `3d-renderers` — MeshRenderer ✅, ShadowRenderer ✅; SkinnedMeshRenderer pending DiaSkinning3D. `canvas3d` — Canvas3D ✅ (ProcessFrame, ambient, health, metrics). `mesh-texture-pipeline` — albedo + normal map textures, TBN shading, moving light (**Approved, ready to build**). | DiaBgfx (Phase 1) ✅, DiaGraphics3D ✅, DiaMesh3D ✅ |


---

### Standalone Features (system Done, feature Approved)

| Feature | Spec | System | Notes |
|---------|------|--------|-------|
| Mesh3DRenderSystemTestStage | [mesh3d-render-system-stage.md](specs/applications/cluichetest/systems/teststages/mesh3d-render-system-stage.md) | TestStages | 5 tasks: catalogue ID fix (haiku), module impl (sonnet), pytest scenario, visual verify, commit. Task 1 independently fixes the `GetLoadProgress` warning spam. |
| ~~Spline3D~~ | [spline3d.md](specs/applications/dia/systems/diageometry3d/spline3d.md) | DiaGeometry3D | 2 tasks: implement Spline3D + SplineFactory3D, GoogleTest suite. Prerequisite for LightPathBehaviour3D. |
| ~~LightPathBehaviour3D~~ | [light-path-behaviour.md](specs/applications/dia/systems/dialighting3d/light-path-behaviour.md) | DiaLighting3D | 3 tasks: add DiaGeometry3D dep, implement behaviour, GoogleTest suite. Depends on Spline3D. |
| DiaMesh3DVisualDebugger — mesh3d-bounds-and-origins | [mesh3d-bounds-and-origins.md](specs/applications/dia/systems/diamesh3dvisualdebugger/mesh3d-bounds-and-origins.md) | DiaMesh3DVisualDebugger | 6 tasks: layer name constants, vcxproj+sln, module YAML, MeshBoundsDrawer, MeshOriginDrawer, tests. No feature deps. |
| DiaMesh3DVisualDebugger — mesh3d-stats | [mesh3d-stats.md](specs/applications/dia/systems/diamesh3dvisualdebugger/mesh3d-stats.md) | DiaMesh3DVisualDebugger | 2 tasks: MeshStatsDrawer, tests. Depends on mesh3d-bounds-and-origins. |
| DiaLighting3DVisualDebugger — debug-widget-config | [debug-widget-config.md](specs/applications/dia/systems/dialighting3dvisualdebugger/debug-widget-config.md) | DiaLighting3DVisualDebugger | 5 tasks: layer name constants, vcxproj+sln, module YAML, LightWidgetsDrawer, tests. No feature deps. |
| DiaLighting3DVisualDebugger — path-arc-preview | [path-arc-preview.md](specs/applications/dia/systems/dialighting3dvisualdebugger/path-arc-preview.md) | DiaLighting3DVisualDebugger | 4 tasks: GetPathBehaviour(), GetSpline(), LightPathArcDrawer, tests. Depends on debug-widget-config + LightPathBehaviour3D. |

---

## In Progress

_Nothing here._

---

## Spec Work Needed (Draft or unset — review/approve before building)

| Item | Spec | What's needed |
|------|------|---------------|
| ~~DiaLighting3DVisualDebugger (new system)~~ | [dialighting3dvisualdebugger.md](specs/applications/dia/systems/dialighting3dvisualdebugger/dialighting3dvisualdebugger.md) | All design questions resolved — needs feature specs authored then approve. 2 features: debug-widget-config (sphere/arrow per light), path-arc-preview (arc line in XZ for directional, spline arc spheres for point/spot). Depends on Spline3D + LightPathBehaviour3D + DiaVisualDebugger. |
| DiaRenderTest CLI Pipeline | [diarendertest.md](specs/applications/dia/systems/diarendertest/diarendertest.md) | Spec `Draft` — awaiting approval. All design questions resolved. 6 features: png-writer, diff-engine, expectations, python-tools, metrics-writer, cluichetest-integration. Research: [render_offline_test/summary.md](research/render_offline_test/summary.md) |
| RenderTestPlugin (CluicheEditor) | — | Needs `/spec-system` — visual debugger panel: wipe slider, region grid, expectation authoring, AI triage panel, render targets. Depends on DiaRenderTest CLI Pipeline shipping first. Mockup: [render_test_debugger_mockup.html](research/render_offline_test/render_test_debugger_mockup.html). Research: [render_offline_test/summary.md](research/render_offline_test/summary.md) |

---

## E2E Orchestration Stack

Architecture redesigned 2026-05-20. Source of truth: **[docs/research/e2e_testing/design-decisions.md](research/e2e_testing/design-decisions.md)**.

### Deferred

| # | Item | Notes |
|---|------|-------|
| 6b | Extract AutomationModuleBase into DiaAutomation | Evaluate from working code after 2+ apps use it |
| DiaRig3D system | feature spec exists (`skeleton-and-pose.md`) | Needs `/spec-system` — Bone3D, Skeleton3D, Pose3D, FK, `SkeletonComponent3D`, `Rig3DAsset`. Mirrors DiaRig2D. | 
| DiaAnimation3D system | feature spec exists (`clip-and-player.md`) | Needs `/spec-system` — AnimationClip3D, ClipPlayer3D, STEP/LINEAR/CUBICSPLINE, `AnimationComponent3D`. glTF animation import is build-time only. |
| DiaStateMachineEditor system | TBD | Needs `/spec-system` — editor plugin for state machine visual debugging + design-time editing. Depends on DiaStateMachine ✅, DiaEditor |
| DiaSkinning3D | TBD — needs `/spec-system` | `skinning-palette` feature already Approved; needs own system spec. SkinningManager, per-frame Matrix34 palettes, `skinningPaletteIndex` on draw commands. | DiaAnimation3D, DiaGraphics3D |
---

### Blocked on Linux/CMake migration

| Item | Blocked by | Notes |
|------|-----------|-------|
| Clang-Tidy analysis | CMake migration (compile_commands.json) | Unblocked by C2 (Foundation CMake pilot) — see DiaArchitecture system below |
| TSan (ThreadSanitizer) | Linux target (WSL2 CI) | Only reliable race detector for Main/Render/Sim threading model; TSan doesn't run on Windows |

---

## Loose Ends (non-spec items)

| Item | Notes |
|------|-------|
| RenderTechnique asset type | Layer-level rendering policy (blend mode, post-process like bloom/distortion). Layers reference a technique by name; renderer resolves at draw time. Needs `/spec-feature` under DiaGraphics or DiaBgfx once the scene system lands. |
| Camera2D controller (pan/zoom/reset) | Application-side input→Camera2D wiring for CluicheTest stages (keyboard pan, scroll zoom, home-key reset). Unblocked once coord2d-debug-overlay ships Camera2D + renderer integration. |
| `Dia::Core::Blackboard` — general-purpose key-value store | Identified during DiaStateMachine research; useful for AI, animation, gameplay. Needs `/spec-feature` under DiaCore. |
| **DiaBgfx3D — Multiple directional lights** | Shader and frame data only consume `dirLights[0]`. Supporting 2–4 lights requires uniform array upload and a shader loop. Needs `/spec-feature` under DiaBgfx3D. |
| **DiaBgfx3D — Specular / simple PBR** | Current model is pure Lambert. Blinn-Phong or minimal metallic-roughness BRDF would allow materials to look distinct. Unblocked once `mesh-texture-pipeline` ships. Needs `/spec-feature` under DiaBgfx3D. |
| RenderTechnique asset type | Layer-level rendering policy (blend mode, post-process like bloom/distortion). Layers reference a technique by name; renderer resolves at draw time. Needs `/spec-feature` under DiaGraphics or DiaBgfx once the scene system lands. |
| Camera2D controller (pan/zoom/reset) | Application-side input→Camera2D wiring for CluicheTest stages (keyboard pan, scroll zoom, home-key reset). Unblocked once coord2d-debug-overlay ships Camera2D + renderer integration. |
| `Dia::Core::Blackboard` — general-purpose key-value store | Identified during DiaStateMachine research; useful for AI, animation, gameplay. Needs `/spec-feature` under DiaCore. |
