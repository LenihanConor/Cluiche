**Spec:** @docs/specs/applications/dia/systems/diabgfx3d/static-mesh-demo.md
**Status:** Done

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Extend shader cook — multi-root support + pipeline.toml | `dia pipeline --target cluichetest` cooks 3d/*.sc without error | Done | sonnet | source_roots list in pipeline_config.py + pipeline.toml; cook iterates all roots; 546 CLI tests pass |
| 2 | Canvas3D shader load + default material | Build clean; runtime log confirms programs loaded | Done | sonnet | LoadFromPath on ShaderProgram; Init3DPrograms() lazy on first StartFrame; default_3d material registered; ShadowRenderer gains SetProgram() |
| 3 | Procedural unit cube Mesh3DAsset | GoogleTest: cube has 24 verts, 36 indices, valid normals | Done | sonnet | Primitives/UnitCube.h inline factory; RegisterMesh() on handler; 5 tests pass |
| 4 | Mesh3DTestStage module + scaffold | `dia run cluichetest` loads stage; draws cube | Done | sonnet | Canvas3D in KernelModule; SimToRender3D stream in RenderModule; Mesh3DTestTestStageModule writes cube frame; build passes |
| 5 | End-to-end verify | Visual: lit box on screen; `dia run googletest` green | Done | opus | 3D shaders cook (33 total, `3d/` subdir preserved); pipeline 4/4 pass; 5 UnitCube tests pass; exit 0 |

---

## Implementation Notes

### Task 1 — Multi-root shader cook

**Problem:** `pipeline.toml` has `source_root = "Dia/DiaBgfx/Shaders"` (singular). The cook function (`bgfx_shader_cook.py`) resolves one `source_root` and rglobs `*.sc` from it. `Dia/DiaBgfx3D/Shaders/3d/` is never discovered.

**Solution:** Change `source_root` (string) to `source_roots` (list of strings) in `BgfxShadersConfig`. Backward-compatible: if old `source_root` key exists, wrap it in a list.

Files to change:
- `pipeline.toml` — `[bgfx_shaders]` section: `source_roots = ["Dia/DiaBgfx/Shaders", "Dia/DiaBgfx3D/Shaders"]`
- `Dia/DiaCLI/dia_cli/commands/pipeline/pipeline_config.py` — `BgfxShadersConfig` adds `source_roots: list[str]`; read with fallback from `source_root`
- `Dia/DiaCLI/dia_cli/commands/pipeline/bgfx_shader_cook.py` — iterate `cfg.source_roots`; each root keeps its relative structure (subdirs preserved: `3d/vs_mesh.bin`)

**Output layout:** `assets/shaders/dx11/3d/vs_mesh.bin` (the `3d/` subdir preserved by `rel_shader = sc_path.relative_to(source_root)`)

### Task 2 — Canvas3D shader load + default material

**Problem:** `Canvas3D` has no shader loading. The base `Canvas` loads shaders in `DeferredInit()` for sprite/debug/ui programs using `ShaderProgram::Load(root, backend, vsName, fsName)`. Canvas3D needs to do the same for `3d/mesh` and `3d/shadow_caster`.

**Solution:** Override `DeferredInit` in `Canvas3D` (it's private but we can add a public `Init3D()` called from the render thread, or make Canvas expose `GetShaderRoot()` as protected). Simplest: add a `LoadMeshPrograms()` method in Canvas3D that:
1. Creates two `ShaderProgram*` (mesh, shadow_caster)
2. Calls `program->Load(shaderRoot, backend, "3d/mesh", "3d/shadow_caster")` — note: the Load API concatenates `root/backend/vs_NAME.bin`, so we need the path format to be `vs_3d/mesh.bin` or adjust. Actually `ShaderProgram::Load` uses `snprintf("%s/%s/vs_%s.bin", root, backend, vsName)` so vsName="mesh" → `assets/shaders/dx11/vs_mesh.bin`. For 3d subdir: vsName needs to be "3d/mesh" → produces `assets/shaders/dx11/vs_3d/mesh.bin` which is WRONG.

**Correct approach:** The cook output preserves relative path from source_root. So `Dia/DiaBgfx3D/Shaders/3d/vs_mesh.sc` with source_root `Dia/DiaBgfx3D/Shaders` → output at `assets/shaders/dx11/3d/vs_mesh.bin`. ShaderProgram::Load would need to accept a subdir or the name includes path. Current API: `Load(root, backend, vsName, fsName)` → `snprintf("%s/%s/vs_%s.bin", root, backend, vsName)`. If we pass vsName="3d/mesh" → `assets/shaders/dx11/vs_3d/mesh.bin` — NO, snprintf prefixes `vs_`. So it generates `vs_3d/mesh.bin` which has the prefix in the wrong spot.

**Fix option A:** Add `ShaderProgram::LoadPath(root, backend, vsRelPath, fsRelPath)` that doesn't prepend `vs_`/`fs_` — just appends the relative bin path directly. This is cleaner since the cooked file is already named `vs_mesh.bin`.

**Fix option B:** Pass vsName as the full relative path including the `vs_` prefix: change Load() to not prepend prefix. Breaking change.

**Best choice: Option A** — add a new overload `LoadFromPath(root, backend, vsRelBin, fsRelBin)` that constructs `root/backend/vsRelBin` and `root/backend/fsRelBin`. Non-breaking.

Then in Canvas3D::Init3DPrograms:
```cpp
mMeshProgram->LoadFromPath(shaderRoot, backend, "3d/vs_mesh.bin", "3d/fs_mesh.bin");
mShadowProgram->LoadFromPath(shaderRoot, backend, "3d/vs_shadow_caster.bin", "3d/fs_shadow_caster.bin");
```

Register default material:
```cpp
MaterialDescriptor def;
def.id = Dia::Core::StringCRC("default_3d");
def.program = mMeshProgram;
def.baseColourRGBA = 0xCCCCCCFF;
mMaterialRegistry->Register(def);
```

**Canvas base class change:** Need access to `mShaderRoot` and `mRendererType` from Canvas3D. Options:
- Make them `protected` (minimal change — Canvas3D already inherits Canvas)
- Or add `protected const char* GetShaderRoot() const` + `protected RendererType GetRendererType() const`

Getter approach is cleaner. Two one-line getters added to Canvas.h protected section.

**When to call:** Canvas3D overrides nothing in the init flow. The base `Canvas::DeferredInit` is called during the first `StartFrame` on the render thread. Canvas3D needs a hook. Options:
- Override `StartFrame` → call base → if first-time, call Init3DPrograms()
- Let RenderModule call Init3DPrograms() after detecting IsInitialised()

Simplest: override `StartFrame` in Canvas3D — call `Canvas::StartFrame(frame)`, then if `!m3DInitialised && IsInitialised()` call `Init3DPrograms()`.

### Task 3 — Procedural unit cube

A static helper `Mesh3DAsset CreateUnitCube()` that builds a 24-vertex (for proper normals per face), 36-index unit cube [-0.5, +0.5]. Vertex3D has position, normal, tangent, uv0, colour — all filled.

Location: `Dia/DiaMesh3D/Primitives/UnitCube.h` + `.cpp` (or just a free function in a Primitives namespace).

GoogleTest: verify vert count, index count, winding, normals are unit-length and axis-aligned per face.

### Task 4 — Mesh3DTestStage

Uses `dia scaffold stage Mesh3D` to create the 4 touch points. The module:
1. In `OnStart`: register unit cube with `Mesh3DAssetHandler` under a known StringCRC
2. In `OnUpdate`: fill `FrameData3D` each frame:
   - `SetCamera(position=(0,2,-5), lookAt=(0,0,0), fovY=60°, aspect from window)`
   - `AddDirectionalLight(direction=(0.5,-1,0.3).normalized, colour=white)`
   - `RequestDrawMesh(meshId="unit_cube", materialId="default_3d", transform=identity)`
3. Calls `Canvas3D::ProcessFrame(frameData3d)` via the existing render pipeline (SimToRender frame stream)

**Integration:** RenderModule currently uses `Dia::Bgfx::Canvas`. For the 3D stage, KernelModule needs to create a `Canvas3D` instead. Options:
- Always create Canvas3D (it inherits Canvas; 2D-only stages won't call ProcessFrame(FrameData3D&) — they use the base ICanvas path which still works)
- Conditional based on .diagame config

**Best choice:** Always create Canvas3D in CluicheGameBaseline's KernelModule. It's backward compatible — Canvas3D inherits Canvas, and all 2D paths call `RenderFrame(FrameData)` which routes to the inherited `ProcessFrame(const FrameData&)`. The 3D path only fires when `RenderModule` detects a `FrameData3D` stream.

RenderModule changes: add a `ServiceStreamReader<Mesh3DAssetHandler>` and a second stream reader for `FrameData3D`. When available, cast the canvas to `Canvas3D*` and call `ProcessFrame(FrameData3D)`.

### Task 5 — End-to-end verify

- `dia pipeline --target cluichetest` → shaders cook (check 3d/ subdir output)
- `dia run cluichetest` → Mesh3DTestStage active → lit cube visible
- `dia run googletest` → all green
- Observation scan on new files
