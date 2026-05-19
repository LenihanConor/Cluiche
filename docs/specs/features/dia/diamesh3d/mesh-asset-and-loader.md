# Feature Spec: mesh-asset-and-loader

## Parent System
@docs/specs/systems/dia/render-backend.md

**Hard dependencies (already specced separately):**
- @docs/specs/systems/dia/diamaths.md — provides `Vector3D`, `Matrix44` (consumer of DiaMaths' `matrix44.md` + `vector3d-cross.md`)
- @docs/specs/systems/dia/diageometry3d.md — provides `Dia::Geometry3D::AABB` (consumer of DiaGeometry3D's `shape-primitives.md`)

**Research:** @docs/research/render_backend_swap/summary.md

## Status
`Approved`

## Summary

Create a new `Dia/DiaMesh3D/` module that owns the static-mesh asset type for the engine. Loads glTF 2.0 (`.gltf` + bin buffers, plus `.glb` binary container) into a renderer-agnostic `Mesh3DAsset` representation: vertex stream(s), index buffer, per-submesh material binding, AABB bounds. Plugs into the existing `DiaAssetRuntime` `IAssetTypeHandler` pattern so meshes load through the same async + sentinel + path-store flow as textures. Skinned-mesh data (joint indices, weights) is parsed and carried through but not consumed — that lights up in `diaskinning3d` and `diabgfx-3d-renderers`.

This feature does **not** render anything. It only produces `Mesh3DAsset` instances available to downstream systems via `DiaAssetRuntime::Get<Mesh3DAsset>(StringCRC assetId)`. Rendering happens in `diabgfx-3d-renderers`.

## Problem

Phase 2 needs static meshes at minimum (terrain — Phase 3 — and characters — `diaskinning3d`). glTF 2.0 is the chosen asset format (RB-015). Today the engine has no mesh asset type; `DiaAssetRuntime` handles textures only. Adding a `Mesh3DAsset` type requires:

- A renderer-agnostic data structure that holds vertex/index/submesh data without leaking bgfx (per PD-004 / RB-006)
- A glTF loader that runs on a worker thread (file I/O + parse) and produces the asset on the main thread (matching the existing texture two-phase pattern)
- A glTF parsing dependency — `cgltf` (single-header, MIT, dependency-free) is the conventional choice
- A submesh model that supports per-submesh material id binding (StringCRC)
- An AABB bounds computation for scene culling (consumes `Dia::Geometry3D::AABB`)

Skinned-mesh attributes (joint indices `JOINTS_0`, joint weights `WEIGHTS_0`) are parsed in this feature so meshes don't need re-parsing later, but the skinning pipeline (palette upload, per-bone transform sampling) is `diaskinning3d`'s job.

## Goals

- Create `Dia/DiaMesh3D/` module with `dia.mesh3d.architecture.module.md` YAML
- Define `Dia::Mesh3D::Mesh3DAsset` — the canonical static-mesh asset type
- Define `Dia::Mesh3D::Vertex3D` — the canonical vertex layout (position, normal, tangent, uv0, optional color, optional joint indices + weights for skinning)
- Define `Dia::Mesh3D::Submesh` — index range + material id within a mesh
- Define `Dia::Mesh3D::Mesh3DAssetHandler` implementing `Dia::AssetRuntime::IAssetTypeHandler` (mirrors `TextureHandler` pattern post-`diasfml-render-removal`)
- Vendor `cgltf` as a single-header dep at `External/cgltf/cgltf.h` (added to `deps.json`)
- Loader supports `.gltf` (JSON + bin buffers) and `.glb` (binary)
- Loader extracts: positions, normals, tangents (auto-generates if missing), uv0, indices, per-primitive material ids
- Loader extracts skinning attributes (joints, weights) when present and stashes them in `Vertex3D.jointIndices` / `jointWeights`; mesh's `IsSkinned()` flag set
- Compute axis-aligned bounding box from positions; store as `Dia::Geometry3D::AABB`
- Two-phase async loading: worker decodes glTF + populates host-side buffers; main thread is a no-op for static mesh (no GPU upload here — that lives in DiaBgfx where `bgfx::createVertexBuffer` is called from the bgfx API thread)
- All public APIs PD-004 compliant; `DynamicArrayC` for vertex/index/submesh storage internal to the asset
- Tests under `Dia/DiaMesh3D/Testing/` with a tiny synthetic glTF fixture (a unit cube, mesh with a single submesh)

## Non-Goals

- **Skinning evaluation** — `DiaSkinning3D` consumes the joint/weight attributes parsed here
- **Animation** — `DiaAnimation3D` parses glTF animations separately (TODO: decide if shared loader or separate)
- **Rendering** — `diabgfx-3d-renderers` consumes `Mesh3DAsset`
- **Mesh editing / authoring** — assets are read-only after load
- **Mesh LOD** — single-resolution mesh per asset; LOD is a future optimisation
- **Procedural mesh generation** — no `Mesh3DAsset::CreateBox()` factory; meshes come from glTF only
- **Cooked binary format** — runtime parses glTF directly per RB-015; cook step deferred unless perf demands it
- **Mesh reuse / instancing data** — `Mesh3DAsset` is a single mesh. Instancing is a render-side optimisation in `diabgfx-3d-renderers`
- **Tangent space** — if glTF lacks tangents, generate them via MikkTSpace algorithm; pulling in MikkTSpace as a dep is acceptable. Otherwise auto-generate naive tangents (cross of normal × up)
- **Vertex de-duplication** — glTF is already de-dup'd per primitive; we trust the input
- **Material data** — only `materialId` (StringCRC) is carried per submesh; resolving the id to actual shader/textures is `diabgfx-3d-renderers`' concern via a `MaterialRegistry`

## Public Interfaces

### `Dia::Mesh3D::Vertex3D`

```cpp
// Dia/DiaMesh3D/Vertex3D.h
#pragma once

#include <DiaMaths/Vector/Vector3D.h>
#include <DiaMaths/Vector/Vector2D.h>
#include <DiaMaths/Vector/Vector4D.h>

namespace Dia { namespace Mesh3D {

// Canonical vertex layout for the engine's static and skinned meshes.
// Total size: 80 bytes (without skinning), 96 bytes (with skinning data populated).
struct Vertex3D
{
    Dia::Maths::Vector3D position;     // 12 bytes
    Dia::Maths::Vector3D normal;       // 12 bytes
    Dia::Maths::Vector4D tangent;      // 16 bytes (xyz=tangent, w=bitangent sign)
    Dia::Maths::Vector2D uv0;          // 8 bytes — primary UV channel
    uint32_t             colour;       // 4 bytes — packed RGBA8 (0xFFFFFFFF default)

    // Skinning attributes — used by skinned meshes; ignored by static meshes.
    uint32_t             jointIndices; // 4 bytes — packed 4×uint8 indices
    Dia::Maths::Vector4D jointWeights; // 16 bytes — 4 weights summing to 1.0
};

} }
```

### `Dia::Mesh3D::Submesh`

```cpp
// Dia/DiaMesh3D/Submesh.h
#pragma once

#include <DiaCore/CRC/StringCRC.h>
#include <cstdint>

namespace Dia { namespace Mesh3D {

struct Submesh
{
    uint32_t              indexStart;   // first index in the mesh's index buffer
    uint32_t              indexCount;   // number of indices for this submesh
    Dia::Core::StringCRC  materialId;   // resolved by renderer
};

} }
```

### `Dia::Mesh3D::Mesh3DAsset`

```cpp
// Dia/DiaMesh3D/Mesh3DAsset.h
#pragma once

#include "DiaMesh3D/Vertex3D.h"
#include "DiaMesh3D/Submesh.h"
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaGeometry3D/Shapes/AABB.h>     // path TBD per DiaGeometry3D's shape-primitives feature
#include <DiaCore/CRC/StringCRC.h>
#include <atomic>

namespace Dia { namespace Mesh3D {

class Mesh3DAsset
{
public:
    enum class State : unsigned char {
        Pending = 0,
        Ready   = 1,
        Failed  = 2
    };

    static constexpr uint32_t kMaxVertices  = 65535;       // index width: uint16
    static constexpr uint32_t kMaxIndices   = 196608;      // 65535 * 3
    static constexpr uint32_t kMaxSubmeshes = 32;

    Mesh3DAsset(Dia::Core::StringCRC assetId);
    ~Mesh3DAsset();

    Dia::Core::StringCRC GetAssetId() const { return mAssetId; }
    State                GetState()   const { return mState.load(std::memory_order_acquire); }
    bool                 IsReady()    const { return GetState() == State::Ready; }
    bool                 IsSkinned()  const { return mIsSkinned; }

    const Dia::Core::Containers::DynamicArrayC<Vertex3D, kMaxVertices>&  GetVertices()  const;
    const Dia::Core::Containers::DynamicArrayC<uint16_t, kMaxIndices>&   GetIndices()   const;
    const Dia::Core::Containers::DynamicArrayC<Submesh,  kMaxSubmeshes>& GetSubmeshes() const;

    const Dia::Geometry3D::AABB& GetBounds() const { return mBounds; }

    // Loader-side population — called by Mesh3DAssetHandler on the asset-runtime owner thread.
    void Populate(const Vertex3D* vertices, uint32_t vertexCount,
                  const uint16_t* indices,  uint32_t indexCount,
                  const Submesh*  submeshes, uint32_t submeshCount,
                  const Dia::Geometry3D::AABB& bounds,
                  bool isSkinned);
    void MarkFailed(const char* reason);

private:
    Dia::Core::StringCRC                                                  mAssetId;
    Dia::Core::Containers::DynamicArrayC<Vertex3D, kMaxVertices>          mVertices;
    Dia::Core::Containers::DynamicArrayC<uint16_t, kMaxIndices>           mIndices;
    Dia::Core::Containers::DynamicArrayC<Submesh,  kMaxSubmeshes>         mSubmeshes;
    Dia::Geometry3D::AABB                                                 mBounds;
    bool                                                                  mIsSkinned;
    std::atomic<State>                                                    mState{State::Pending};
};

} }
```

`kMaxVertices = 65535` keeps indices to `uint16_t` — the most common bgfx index width and a sensible cap for "light 3D" meshes. Larger meshes can be authored as multi-mesh glTF (each `<65535` verts) at the asset pipeline level.

### `Dia::Mesh3D::Mesh3DAssetHandler`

```cpp
// Dia/DiaMesh3D/Mesh3DAssetHandler.h
#pragma once

#include <DiaAssetRuntime/IAssetTypeHandler.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Containers/Strings/String512.h>
#include <unordered_map>
#include <shared_mutex>

namespace Dia { namespace Mesh3D {

class Mesh3DAsset;

class Mesh3DAssetHandler : public Dia::AssetRuntime::IAssetTypeHandler
{
public:
    Mesh3DAssetHandler();
    ~Mesh3DAssetHandler();

    // Lookup the mesh for an asset id. Returns nullptr if not loaded.
    // Thread-safe; takes shared lock.
    Mesh3DAsset* LookupMesh(const Dia::Core::StringCRC& assetId) const;

    unsigned int GetLoadedCount() const;

    // IAssetTypeHandler
    void Load(const Dia::Core::StringCRC& assetId,
              const Dia::Core::Containers::String512& resolvedPath,
              Dia::AssetRuntime::IAssetLoadCallback* callback) override;
    void Unload(const Dia::Core::StringCRC& assetId) override;

    // Async pump — drains decoded glTF results on the asset-runtime owner thread.
    // (For Mesh3DAsset this is a no-op host-side; the data is already in the
    //  Mesh3DAsset object after the worker. Tick() exists for symmetry with
    //  TextureHandler::Tick — see Implementation for whether it's actually needed.)
    void Tick();

private:
    mutable std::shared_mutex                                  mMutex;
    std::unordered_map<unsigned int /*StringCRC.Value()*/, Mesh3DAsset*> mAssetIdToMesh;
    // ... pending-decode queue (glTF parse on worker thread)
};

} }
```

## Implementation

### Files introduced

```
Dia/DiaMesh3D/                                NEW MODULE
├── DiaMesh3D.vcxproj                         NEW
├── DiaMesh3D.vcxproj.filters                 NEW
├── dia.mesh3d.architecture.module.md         NEW
├── Vertex3D.h                                NEW
├── Submesh.h                                 NEW
├── Mesh3DAsset.h                             NEW
├── Mesh3DAsset.cpp                           NEW
├── Mesh3DAssetHandler.h                      NEW
├── Mesh3DAssetHandler.cpp                    NEW
├── GltfLoader.h                              NEW (internal — used by handler)
├── GltfLoader.cpp                            NEW (cgltf-driven parsing)
└── Testing/
    ├── MockMesh3DAsset.h                     NEW
    └── Fixtures/
        └── unit_cube.gltf                    NEW (test fixture)
```

### Files modified

```
deps.json (root)
   - Add cgltf single-file dep:
     {
       "id": "cgltf",
       "version": "1.13",
       "install_type": "single_file",
       "url": "https://raw.githubusercontent.com/jkuhlmann/cgltf/<pinned-sha>/cgltf.h",
       "sha256": "<hash>",
       "destination": "External/cgltf/cgltf.h"
     }
   (uses the single_file install type from `deps-single-file` feature; same pattern as Tailwind/Alpine/DaisyUI)

Cluiche/Cluiche.sln
   - Add DiaMesh3D.vcxproj reference

Dia/DiaMesh3D/DiaMesh3D.vcxproj
   - References External/cgltf/ on include path
   - Links DiaCore, DiaMaths, DiaGeometry3D, DiaAssetRuntime
```

### glTF parsing flow (worker thread)

```
1. Open file via Dia::Core::FilePath::Resolve.
2. cgltf_parse_file(...) → cgltf_data*.
3. cgltf_load_buffers(...) → loads .bin buffers.
4. cgltf_validate(...) → integrity check.
5. For each cgltf_mesh in cgltf_data->meshes:
     For each cgltf_primitive in mesh->primitives:
        - Read POSITION, NORMAL, TANGENT (if present), TEXCOORD_0, COLOR_0 (optional)
        - Read JOINTS_0, WEIGHTS_0 (optional → mIsSkinned = true)
        - Read indices (uint16_t expected; reject if uint32_t)
        - Resolve material name → StringCRC
        - Append to mVertices (with offset), mIndices (with offset), mSubmeshes
6. Compute mBounds = AABB over all positions via Dia::Geometry3D::AABB::FromPoints(...)
7. cgltf_free(cgltf_data).
8. Push result to handler's pending-result queue (mutex-guarded).
```

### Main-thread Tick (asset-runtime owner thread)

`TextureHandler::Tick` had work to do (GL upload). For static meshes, the worker has already populated the host-side buffers; there's no main-thread work needed. `Mesh3DAssetHandler::Tick` drains the result queue, calls `Mesh3DAsset::Populate(...)` on each, and fires `OnLoadComplete`. No GPU calls — those happen in `diabgfx-3d-renderers`'s `MeshRenderer::OnAssetReady` (an observer that uploads to bgfx when the asset becomes Ready).

## Files Introduced / Modified

| File | Change |
|------|--------|
| `Dia/DiaMesh3D/` (entire module) | NEW |
| `Dia/DiaMesh3D/DiaMesh3D.vcxproj{,.filters}` | NEW |
| `Dia/DiaMesh3D/dia.mesh3d.architecture.module.md` | NEW |
| `Cluiche/Cluiche.sln` | Add DiaMesh3D.vcxproj reference |
| `deps.json` | Add cgltf single-file entry |
| `External/cgltf/cgltf.h` | NEW — vendored single-header dep |
| `Cluiche/Tests/GoogleTests/` | Add `TestMesh3DAsset`, `TestGltfLoader` |

## Dependencies

| Dependency | Type | Notes |
|------------|------|-------|
| **DiaMaths `vector3d-cross`** + `matrix44` features | Hard | Vertex3D uses Vector3D/Vector4D; bounds compute needs Vector3D |
| **DiaGeometry3D `shape-primitives`** feature | Hard | Mesh3DAsset stores AABB; Geometry3D's AABB::FromPoints used |
| `texture-handle-stringcrc` (Approved) | Soft | Pattern reuse — same atomic State enum + StringCRC asset id |
| `graphics-3d-types` (Approved) | Soft | `Mesh3DDrawCommand::meshId` consumes Mesh3DAsset asset ids |
| `diasfml-render-removal` (Approved) | Soft | After it lands, `TextureHandler` lives in DiaAssetRuntime; Mesh3DAssetHandler follows the same precedent (lives in DiaMesh3D, but mirrors the pattern) |
| `cgltf` (new external dep) | Hard | Vendored via `deps-single-file` install type |
| Phase 1 features | Soft | Phase 1 ships first per RB-003 |

## Acceptance Criteria

1. `Dia/DiaMesh3D/DiaMesh3D.vcxproj` builds clean under `/std:c++20`; links DiaCore, DiaMaths, DiaGeometry3D, DiaAssetRuntime
2. `External/cgltf/cgltf.h` is staged via `dia env setup` from the `deps.json` single-file entry
3. `Dia::Mesh3D::Vertex3D` struct exists with the documented fields and 96-byte total size (verified by `static_assert(sizeof(Vertex3D) == 96)`)
4. `Dia::Mesh3D::Submesh` struct exists with indexStart/indexCount/materialId
5. `Dia::Mesh3D::Mesh3DAsset` exists with atomic State, IsReady/IsSkinned predicates, vertex/index/submesh accessors, GetBounds returning `Dia::Geometry3D::AABB`
6. `Dia::Mesh3D::Mesh3DAssetHandler` implements `IAssetTypeHandler`; `LookupMesh(StringCRC) → Mesh3DAsset*` is the public accessor
7. Loader parses `.gltf` (JSON + bin) and `.glb` (binary) — both forms tested
8. Loader extracts JOINTS_0 + WEIGHTS_0 when present; sets `IsSkinned() == true`
9. Loader auto-generates tangents if absent (naive cross-product fallback acceptable for Phase 2; MikkTSpace is a future improvement)
10. Loader rejects meshes with >65535 vertices per submesh with a clear error logged via DiaLogger
11. AABB computed correctly via `AABB::FromPoints(positions)`; verified against unit_cube.gltf fixture (expected min=(-0.5,-0.5,-0.5), max=(0.5,0.5,0.5))
12. `kMaxVertices/Indices/Submeshes` capacity overflow handled with logged failure (`OnLoadFailed` fired)
13. `dia run googletest` is green; new TestGltfLoader suite covers static cube load, skinned cube load (with synthetic JOINTS_0/WEIGHTS_0), malformed glTF rejection
14. `dia.mesh3d.architecture.module.md` validates via `python Tools/dia_modules.py --validate`
15. No bgfx types or SFML types in any DiaMesh3D public header (verified by grep — RB-006)
16. PD-004 audit: no `std::vector`, `std::string`, `std::array` in any public header

## Traceability

| Level | Spec | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System (primary) | RenderBackend | @docs/specs/systems/dia/render-backend.md |
| System (consumed) | DiaMaths | @docs/specs/systems/dia/diamaths.md |
| System (consumed) | DiaGeometry3D | @docs/specs/systems/dia/diageometry3d.md |
| System (consumed) | DiaAssetRuntime | @docs/specs/systems/dia/diaassetruntime.md |

## Binding Decisions Compliance

| ID | Source | Decision | Compliance |
|----|--------|----------|------------|
| PD-001 | Platform | StringCRC for IDs | Compliant — meshId, materialId, joint indices via StringCRC where applicable |
| PD-004 | Platform | No STL in public APIs | Compliant — DynamicArrayC throughout; cgltf is internal-only |
| PD-005 | Platform | x64 only | Compliant |
| PD-006 | Platform | VS project files source of truth | Compliant — `.vcxproj{,.filters}` manually maintained |
| PD-007 | Platform | C++20 | Compliant |
| PD-008 | Platform | Directory.Build.props ownership | Compliant |
| PD-009 | Platform | Generated output under Cluiche/out/ | N/A — assets read from existing paths |
| AD-001 | Dia App | Module YAML frontmatter | Compliant — `dia.mesh3d.architecture.module.md` created |
| AD-003 | Dia App | Namespace Dia::<Module>:: | Compliant — `Dia::Mesh3D::` |
| RB-002 | RenderBackend | Two-phase delivery | Phase 2 |
| RB-003 | RenderBackend | Phase 2 specs Approved, not in progress | Compliant |
| RB-006 | RenderBackend | No backend types in public surface | Compliant — no bgfx/SFML types in DiaMesh3D headers |
| RB-013 | RenderBackend | Module decomposition mirrors 2D family | Compliant — `DiaMesh3D` is the 3D mesh asset module |
| RB-015 | RenderBackend | glTF 2.0 runtime parse | **Compliant — this feature implements it** |

## AI Review Questions

| # | # | Question | Answer |
|---|---|----------|--------|
| 1 | Single header vs library | cgltf vs tinygltf? | cgltf: single-header, MIT, no deps, mature, used by Bevy/Filament. tinygltf: also viable but has nlohmann/json transitive dep (STL exposure). cgltf wins on PD-004 cleanliness. |
| 2 | Index width | `kMaxVertices = 65535` forces `uint16_t` indices. Restrictive? | For Phase 2's "light 3D" scope, yes — adequate. Larger meshes (terrain, complex characters) split into multiple submeshes pre-cook. Phase 3 (DiaTerrain) may need 32-bit indices; address there |
| 3 | Tangent generation | Auto-generate or require glTF to provide? | If absent, generate naive (cross of normal × world up). Adequate for "light 3D" non-photoreal target. MikkTSpace is the right answer eventually but adds another dep |
| 4 | Joint count | glTF supports 4 joints per vertex via JOINTS_0/WEIGHTS_0. Adequate? | Yes — covers 99% of skinned characters. JOINTS_1/WEIGHTS_1 for >4 joints per vertex is rare; defer to a future feature if needed |
| 5 | Material id source | Where does `materialId` (StringCRC) come from? | Hash of glTF material name (`mesh.material.name`). Convention: glTF authors name materials canonically (e.g. `dragon_skin`, `terrain_grass`). The renderer's MaterialRegistry maps these StringCRCs to actual shader+texture combos |
| 6 | Skinning data on static meshes | Does Vertex3D always carry joint indices/weights even for static meshes? | Yes (96 bytes per vertex unconditional). Static meshes have `IsSkinned() == false` and the skinning attributes are zero-initialised. Trade-off: simpler vertex layout (one struct, one bgfx vertex decl); marginal vertex memory cost |
| 7 | Mesh upload | Where does GPU upload live? | `diabgfx-3d-renderers` — `Bgfx::MeshRenderer` observes Mesh3DAsset state transitions to Ready and calls `bgfx::createVertexBuffer` / `bgfx::createIndexBuffer` from the bgfx API thread |
| 8 | Async loader Tick | Does Mesh3DAssetHandler need a Tick? | Yes — to drain worker results on the owner thread (preserves AssertOwnerThread). The Tick is short (just unmarshalling Populate calls); no GPU work |
| 9 | Vertex buffer streaming | Phase 2 covers static + skinned. Does this feature support streaming meshes (load on demand)? | No — same scope as TextureHandler. Whole-mesh load only |
| 10 | Animation parsing | glTF animations live in same file. Are they parsed here? | No — `DiaAnimation3D` parses them separately via its own loader. Both loaders use cgltf; they're separate asset types |
| 11 | Skinned mesh skeleton | The mesh has `JOINTS_0` indices into a skeleton. Where does the skeleton come from? | `DiaRig3D` asset — also parsed from the same glTF file. Linkage via the glTF skin index. Captured in `diarig3d` feature spec |
| 12 | AABB compute | Compute on every load, or store in glTF metadata? | Compute on every load. Trivial cost relative to glTF parse. Avoids stale-bounds bugs |

---
