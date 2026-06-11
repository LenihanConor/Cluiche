# Cluiche Backlog

Derived from spec status across `docs/specs/`. When a spec moves to Done, move it to [BACKLOG-HISTORY.md](BACKLOG-HISTORY.md).

---

## Ready to Build (Approved → implement)

These specs are `Approved` with all features `Approved`. No spec work needed — go straight to implementation.

### Systems

| System | Spec | Features | Depends On |
|--------|------|----------|------------|
| ~~DiaBgfx3D~~ | [diabgfx3d.md](specs/systems/dia/diabgfx3d.md) ✅ | `gpu-resources` — MaterialRegistry + MeshGpuCache (needs DiaMesh3D). `3d-renderers` — MeshRenderer, SkinnedMeshRenderer, ShadowRenderer, 6 shaders (needs DiaMesh3D + DiaSkinning3D). `canvas3d` — Canvas3D shell + MaterialRegistry **done** (tasks 1–3); CluicheTest demo blocked on DiaScene3D chain. | DiaBgfx (Phase 1) ✅, DiaGraphics3D ✅, DiaMesh3D, DiaScene3D chain |
| ~~DiaMesh3D~~ | [diamesh3d.md](specs/systems/dia/diamesh3d.md) ✅ | `mesh-asset-and-loader` — `Vertex3D` (52 bytes, no joint data), `Submesh`, `Mesh3DAsset`, `Mesh3DAssetHandler` (cooked `.mesh3d` flat-binary loader, type prefix `"mesh3d."`). glTF import is DiaAssetPipeline build-time only. | DiaMaths, DiaGeometry3D, DiaAssetRuntime |
| ~~DiaRig3D~~ | TBD — needs `/spec-system` | `skeleton-and-pose` feature already Approved; needs own system spec. Bone3D, Skeleton3D, Pose3D, FK, `SkeletonComponent3D`. Also owns `Rig3DAsset` (skeleton + per-vertex skin binding — joint indices + weights as parallel array to mesh vertices). Mirrors DiaRig2D. | DiaMesh3D |
| ~~DiaAnimation3D~~ | TBD — needs `/spec-system` | `clip-and-player` feature already Approved; needs own system spec. AnimationClip3D, ClipPlayer3D, STEP/LINEAR/CUBICSPLINE interpolation, `AnimationComponent3D`. glTF animation import is DiaAssetPipeline build-time only (parallel to DiaMesh3D's cooked binary approach). | DiaRig3D |
| DiaSkinning3D | TBD — needs `/spec-system` | `skinning-palette` feature already Approved; needs own system spec. SkinningManager, per-frame Matrix34 palettes, `skinningPaletteIndex` on draw commands. | DiaAnimation3D, DiaGraphics3D |
| ~~DiaChatPlugin~~ | [diachatplugin.md](specs/systems/cluicheeditor/diachatplugin.md) ✅ | Dockable AI assistant panel — Ollama/Claude/Gemini via DiaPython orchestrator, direct `ExecuteAction()` tool dispatch, auto-gen knowledge context (`editor_actions.md`, `data_types.md`) + hand-authored files (`engine_overview.md`, `editor_workflows.md`, `asset_style_guide.md`), hybrid chat+detail UI, destructive action confirmation gate, context window indicator, empty state. Phase 2: multi-step agentic loop. Requires `project.list` action + DiaEditorAPI data-type-registry feature. | DiaEditorAPI Phase 1 + data-type-registry, DiaEditor, DiaPython, DiaUICEF |


---

### Standalone Features (system Done, feature Approved)

| Feature | Spec | System | Notes |
|---------|------|--------|-------|

---

## Ready to Build (cont.)

---

## In Progress

_Nothing here._

---

## Spec Work Needed (Draft or unset — review/approve before building)

| Item | Spec | What's needed |
|------|------|---------------|
| DiaStateMachineEditor system | TBD | Needs `/spec-system` — editor plugin for state machine visual debugging + design-time editing. Depends on DiaStateMachine ✅, DiaEditor |


---

## E2E Orchestration Stack (build in dependency order)

Architecture redesigned 2026-05-20. Source of truth: **[docs/research/e2e_testing/design-decisions.md](research/e2e_testing/design-decisions.md)**.



### Deferred

| # | Item | Notes |
|---|------|-------|
| 6b | Extract AutomationModuleBase into DiaAutomation | Evaluate from working code after 2+ apps use it |

---


### Blocked on Linux/CMake migration

| Item | Blocked by | Notes |
|------|-----------|-------|
| Clang-Tidy analysis | CMake migration (compile_commands.json) | Unblocked by C2 (Foundation CMake pilot) — see DiaArchitecture system below |
| TSan (ThreadSanitizer) | Linux target (WSL2 CI) | Only reliable race detector for Main/Render/Sim threading model; TSan doesn't run on Windows |

---

---

## Loose Ends (non-spec items)

| Item | Notes |
|------|-------|
| DiaChatPlugin — model picker dropdown | Backend selector already exists but model is hardcoded. Query `ollama list` / known Claude+Gemini models at startup, populate a dropdown so the user can switch models without code changes. Persist last selection. |
| `dia env` Python package management | Add `requirements.txt` to `External/Python311/`, wire `dia env setup` to `pip install --target site-packages`, so fresh clones get `requests` etc. without manual pip. Pipeline deploy already handles runtime copy. |
| RenderTechnique asset type | Layer-level rendering policy (blend mode, post-process like bloom/distortion). Layers reference a technique by name; renderer resolves at draw time. Needs `/spec-feature` under DiaGraphics or DiaBgfx once the scene system lands. |
| Camera2D controller (pan/zoom/reset) | Application-side input→Camera2D wiring for CluicheTest stages (keyboard pan, scroll zoom, home-key reset). Unblocked once coord2d-debug-overlay ships Camera2D + renderer integration. |
| DiaAssetRuntime — Hot reload path | Asset Lifecycle Management adds `Failed → Loading` retry but no `Loaded → Loading → Loaded` path. Still need a `ReloadAsset(assetId)` for live iteration (future feature on top of lifecycle management). |
| `Dia::Core::Blackboard` — general-purpose key-value store | Identified during DiaStateMachine research; useful for AI, animation, gameplay. Needs `/spec-feature` under DiaCore. |
