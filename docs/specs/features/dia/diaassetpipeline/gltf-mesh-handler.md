# Feature Spec: glTF Mesh Handler

**Parent:** @docs/specs/systems/dia/diaassetpipeline.md

## Summary

A build-time asset handler that reads `.gltf`/`.glb` source files and cooks them into the `.mesh3d` flat-binary format consumed at runtime by DiaMesh3D's `Mesh3DAssetHandler`. It registers as the `mesh3d` type handler in `AssetHandlerRegistry` and is dispatched on the `mesh3d.` asset-ID prefix like every other handler.

This is the **first real transform handler in the pipeline** — every existing handler is copy-as-is (`transform` returns the source path unchanged; `deploy` copies it). The glTF handler inverts that model: `transform` parses the glTF, packs the `.mesh3d` binary, and writes it **directly to its layout-resolved deploy path** (with the extension swapped from the source `.gltf`/`.glb` to `.mesh3d`); `deploy` is a confirming no-op because the cooked artifact is already in place. The cooked binary is the final artifact — there is no meaningful "transformed but not yet deployed" intermediate.

Scope is **static meshes only**. Skinned glTF (presence of `JOINTS_0`/`WEIGHTS_0` attributes or a skin) is detected and rejected with a clear error. Skinning data and the `.rig3d` companion format belong to DiaRig3D, which is not yet built, and `Vertex3D` carries no joint data.

## Problem

DiaMesh3D's runtime loader exists and reads a cooked `.mesh3d` binary, but nothing produces that binary. There is no path from an authored 3D model to a runtime-loadable mesh. Artists author in glTF (the interchange format bgfx and most tooling already speak), so the pipeline needs a handler that turns `.gltf`/`.glb` into the exact 41-byte-header flat binary the runtime reader expects. Without it the entire DiaMesh3D end-to-end chain (source → cook → load) is blocked, and DiaBgfx3D's mesh rendering has no content to draw.

## Acceptance Criteria

1. A `Mesh3DHandler` with `type_id = "mesh3d"` is registered in `AssetHandlerRegistry` via `register_built_in_handlers`.
2. `validate` errors (collected, no early exit per SD-APIPE-002) when: the source file does not exist; the source is not a readable `.gltf`/`.glb`; the glTF contains zero meshes or more than one mesh; any primitive uses a non-triangle mode (glTF `mode != 4`); any primitive is missing `POSITION`, `NORMAL`, `TANGENT`, or `TEXCOORD_0`; the glTF is skinned (has a `skin`, or any primitive has `JOINTS_0`/`WEIGHTS_0`); the resulting mesh would exceed the format limits (> 65535 vertices, > 196608 indices, > 32 submeshes).
3. `transform` reads the single mesh, converts each glTF **primitive** into one `Submesh`, and writes a `.mesh3d` binary to the deploy path resolved by the layout engine, with the filename stem preserved and extension forced to `.mesh3d`. Returns `TransformResult(success=True, output_path=<that path>)`.
4. The emitted binary matches the `Mesh3DBinaryReader` format exactly:
   - **Header (41 bytes):** magic `"MESH"` (bytes `0x4D 0x45 0x53 0x48`), `version = 1` (1 byte), `vertexCount` (uint32 LE), `indexCount` (uint32 LE), `submeshCount` (uint32 LE), AABB min x/y/z (3× float LE), AABB max x/y/z (3× float LE).
   - **Vertices:** `vertexCount` × `Vertex3D` (52 bytes each): `position` (3× float), `normal` (3× float), `tangent` (4× float, w = bitangent sign), `uv0` (2× float), `colour` (uint32, packed `0xRRGGBBAA`, default `0xFFFFFFFF`).
   - **Indices:** `indexCount` × uint16 LE.
   - **Submeshes:** `submeshCount` × 12 bytes: `indexStart` (uint32 LE), `indexCount` (uint32 LE), `materialId` (uint32 LE — CRC32 of the material name).
5. `materialId` is computed as `zlib.crc32(material_name.encode("utf-8")) & 0xFFFFFFFF`, which is byte-for-byte identical to the C++ `Dia::Core::StringCRC` of the same string (standard CRC-32: init `0xFFFFFFFF`, reflected `0xEDB88320` table, final XOR `0xFFFFFFFF`). A primitive with no assigned material uses material name `""` (CRC32 of empty string).
5a. **Material-name collision guard.** When two primitives reference materials whose names hash to the same `materialId` but the names differ (a genuine CRC32 collision), the cooker emits an error naming both materials and the asset — collapsing distinct materials silently is a content bug, not a valid merge. Two primitives that share the *same* material name are valid and intentionally share one `materialId`.
5b. **Vertex colour is opaque white.** Every vertex's packed `colour` is `0xFFFFFFFF`. The glTF `COLOR_0` attribute, if present, is ignored in this version.
6. The AABB is computed from the union of all primitive vertex positions in the single mesh.
7. Vertices are concatenated across primitives into one vertex buffer; each primitive's indices are rebased to absolute positions in that buffer; each `Submesh` records the `indexStart`/`indexCount` range for its primitive.
8. `deploy` is a no-op that returns `DeployResult(success=True, deploy_path=<transform output_path>)` — the binary was written to its final location during transform.
9. `pygltflib` is available to the pipeline after `dia env setup`: it is listed in a `requirements.txt` under `External/Python311/` and a `deps` (or dedicated python-packages) step in the env setup orchestrator installs it. If `pygltflib` is missing at handler import/use, the handler raises an actionable error naming the install command.
10. Indices are written as uint16; a mesh whose index values exceed 65535 (i.e. > 65535 vertices) is rejected at validate (AC-2), so 16-bit indices are always sufficient.

## Out of Scope

- **Skinning / `.rig3d` emission** — deferred to DiaRig3D. Skinned input is rejected, not partially cooked.
- **Tangent generation** — `TANGENT` must be present in the source; the cooker does not compute it from positions/UVs. (Deferred enhancement.)
- **Triangulation / primitive-mode conversion** — only triangle lists (`mode = 4`) are accepted.
- **Multiple meshes per file** — exactly one mesh per `.gltf`/`.glb`.
- **Material asset resolution** — the cooker hashes the material *name* into `materialId`. Resolving that ID to a real material (textures, shaders) is DiaBgfx3D's MaterialRegistry, at runtime.
- **Incremental / hash-skip cooking** — full cook every run (SD-APIPE-005).

## API Design

```python
# dia_cli/commands/asset/handlers/mesh3d.py

import struct
import zlib
from pathlib import Path

from dia_cli.commands.asset.handler import (
    AssetHandler, AssetError, TransformResult, DeployResult,
)

_MAGIC = b"MESH"
_VERSION = 1
_MAX_VERTICES = 65535
_MAX_INDICES = 196608
_MAX_SUBMESHES = 32


class Mesh3DHandler(AssetHandler):
    """Cooks .gltf/.glb into the .mesh3d flat binary (static meshes only)."""
    type_id = "mesh3d"

    def validate(self, record: dict, context: "BuildContext") -> list[AssetError]:
        # source exists; loads as glTF; exactly one mesh; all primitives are
        # triangles with POSITION/NORMAL/TANGENT/TEXCOORD_0; not skinned;
        # within vertex/index/submesh limits. Collect all errors, no early exit.
        ...

    def transform(self, record: dict, context: "BuildContext") -> TransformResult:
        # parse glTF -> concatenate primitive vertices, rebase indices,
        # build submeshes, compute AABB, pack binary, write to deploy path
        # (stem + ".mesh3d"). Return output_path.
        ...

    def deploy(self, record: dict, context: "BuildContext") -> DeployResult:
        # No-op: transform already wrote the cooked binary to its deploy path.
        ...


def _material_id(name: str) -> int:
    return zlib.crc32(name.encode("utf-8")) & 0xFFFFFFFF


def _resolve_mesh3d_deploy_path(record, context) -> Path:
    # Reuse the layout engine to resolve scope/tag dir, then force the
    # filename to <source-stem>.mesh3d (this handler's deployed name differs
    # from its source name — the one special case in the pipeline).
    ...
```

### Binary packing (struct format)

```
header = struct.pack("<4sB3I6f", b"MESH", 1,
                     vertex_count, index_count, submesh_count,
                     min_x, min_y, min_z, max_x, max_y, max_z)
vertex = struct.pack("<3f3f4f2fI", px,py,pz, nx,ny,nz, tx,ty,tz,tw, u,v, colour)
indices = struct.pack(f"<{index_count}H", *all_indices)
submesh = struct.pack("<3I", index_start, index_count, material_id)
```

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Add `pygltflib` to `External/Python311/requirements.txt` and wire a package-install step into the `dia env setup` orchestrator (`deps` or new `python-packages` step) | `dia env verify` reports pygltflib present; fresh-clone install path documented | | sonnet | Closes the "dia env Python package management" backlog loose-end |
| 2 | Implement `Mesh3DHandler.validate` — source exists, single mesh, triangle-only, required attributes present, skinned rejected, limit checks; collect all errors | Unit: each failure mode produces the expected `AssetError`; valid glTF returns `[]` | | sonnet | RED first per TDD; use a small fixture `.gltf` |
| 3 | Implement glTF→buffers extraction + binary packer — concatenate primitive vertices, rebase indices, build submeshes, compute AABB, `struct.pack` the 41-byte header + payload; `_material_id` via `zlib.crc32` | Unit: packed bytes round-trip through a reader matching `Mesh3DBinaryReader`; CRC parity check vs known StringCRC values | | sonnet | The correctness core of the feature |
| 4 | Implement `transform` (write to layout path, stem + `.mesh3d`) and no-op `deploy`; `_resolve_mesh3d_deploy_path` extension override | Unit: transform writes binary to expected deploy path; deploy returns that path | | sonnet | Handler-owned path keeps the special case contained |
| 5 | Register `Mesh3DHandler` in `handlers/__init__.py` `_BUILT_IN_HANDLERS` | Unit: registry resolves `mesh3d` to `Mesh3DHandler` | | haiku | |
| 6 | End-to-end test: a fixture `.gltf` through validate→transform→deploy yields a `.mesh3d` the reader format parses | Integration: cook fixture, assert header counts + AABB + submesh material IDs | | sonnet | Proves the format contract against a real file |

## Dependencies

| Dependency | What this feature uses |
|------------|------------------------|
| Feature 1 — Handler Registry & Build Runner | `AssetHandler`, `AssetError`, `TransformResult`, `DeployResult`, `BuildContext`, dispatch on `mesh3d.` prefix |
| Feature 3 — Deploy Layout Engine | `resolve_deploy_path` for scope/tag directory resolution (handler overrides the filename extension) |
| DiaMesh3D (runtime) | The `.mesh3d` binary format defined by `Mesh3DBinaryReader` — this feature must produce exactly what that reader consumes |
| `pygltflib` | glTF/glb parsing (accessors, buffer views, primitives, materials) |
| Python stdlib | `struct` (binary packing), `zlib` (CRC32 = StringCRC parity), `pathlib` |

## Files

| File | Action |
|------|--------|
| `Dia/DiaCLI/dia_cli/commands/asset/handlers/mesh3d.py` | Create — `Mesh3DHandler`, packer, `_material_id`, path resolver |
| `Dia/DiaCLI/dia_cli/commands/asset/handlers/__init__.py` | Edit — import + register `Mesh3DHandler` |
| `Dia/DiaCLI/requirements.txt` | Create — list `pygltflib` (lives alongside DiaCLI; installed by `dia env setup --python-packages`) |
| `Dia/DiaCLI/dia_cli/commands/env/setup_orchestrator.py` | Edit — install Python packages from requirements.txt during `dia env setup` |
| `Dia/DiaCLI/tests/test_mesh3d_handler.py` | Create — validate failure modes, packer round-trip, CRC parity, e2e cook |
| `Dia/DiaCLI/tests/fixtures/*.gltf` | Create — minimal triangle mesh fixture (and a skinned one for the reject test) |

## Binding Decisions Compliance

Only the parent decisions that actually constrain this feature:

| ID | Decision | Compliance |
|----|----------|------------|
| SD-APIPE-002 | Collect all errors, no early exit | `validate` accumulates every failure into the returned list before returning. |
| SD-APIPE-003 | Copy-as-is is the *default* transform | This handler is the deliberate exception — it registers a real transform. The default still applies to types with no handler; this feature does not change that. |
| SD-APIPE-004 | Deploy layout driven by scope + tags | The cooked `.mesh3d` deploys to the scope/tag path from the layout engine; only the filename extension is overridden. |
| SD-APIPE-006 | All handlers are pure Python | `Mesh3DHandler` is pure Python (`pygltflib`, `struct`, `zlib`); no C++ compilation to add it. |
| PD-001 | StringCRC for IDs | `materialId` is CRC32 of the material name, byte-identical to `Dia::Core::StringCRC`. Asset ID stays `mesh3d.<name>`. |
| PD-005 | x64 Windows only | Windows paths; no cross-platform concerns. |
| PD-009 | Generated output under `Cluiche/out/` | The cooked `.mesh3d` is a deployed *binary asset*, not build output — so it goes to `bin/<App>/<Config>/<Platform>/assets/`, exactly where every other deployed asset (textures, audio) lands per SD-CAT-004's runtime-asset location. PD-009 governs build logs/intermediates, which still go to `out/` (the NDJSON event log). Deployed assets to `bin/` is the established convention; this feature follows it. |
| SD-CAT-001 | Asset IDs are `type.name` composites | Dispatch keyed on the `mesh3d` prefix; record IDs are `mesh3d.<name>`. |

## Resolved Design Decisions

| # | Decision | Rationale |
|---|----------|-----------|
| 1 | **Vertex colour is opaque white (`0xFFFFFFFF`); `COLOR_0` ignored** (AC-5b). | Smallest correct v1 — avoids vec3/vec4 + float/normalized-ubyte component handling. Revisit if content needs per-vertex colour. |
| 2 | **Name-based material identity, guarded by a collision check** (AC-5a). Same name → shared `materialId` (intentional). Different names hashing to the same ID → cook error. | Matches how MaterialRegistry keys materials; the guard turns a silent merge of distinct materials into a loud, actionable error. |
| 3 | **Cook in `transform`, no-op `deploy`; special case contained in this handler.** | The cooked binary is the final artifact, so a transform/deploy split would only add a wasteful self-copy. No build-runner change. Generalise into a first-class "transform produces deployed artifact" concept only if a second cooking handler (texture BCn, audio compression) makes the pattern repeat. |

## Status

`Done` — Plan: @docs/specs/features/dia/diaassetpipeline/gltf-mesh-handler.plan.md
