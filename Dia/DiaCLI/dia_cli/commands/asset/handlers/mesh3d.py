from __future__ import annotations

import struct
import zlib
from pathlib import Path
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from dia_cli.commands.asset.context import BuildContext

from dia_cli.commands.asset.handler import AssetHandler, AssetError, TransformResult, DeployResult

_MAGIC = b"MESH"
_VERSION = 1
_MAX_VERTICES = 65535
_MAX_INDICES = 196608
_MAX_SUBMESHES = 32
_REQUIRED_ATTRS = {"POSITION", "NORMAL", "TANGENT", "TEXCOORD_0"}
_SKINNING_ATTRS = {"JOINTS_0", "WEIGHTS_0"}


def _material_id(name: str) -> int:
    return zlib.crc32(name.encode("utf-8")) & 0xFFFFFFFF


def _read_accessor_floats(gltf, acc_idx: int, n_components: int) -> list[list[float]]:
    """Read float data from a glTF accessor into a list of component lists."""
    import base64
    acc = gltf.accessors[acc_idx]
    bv = gltf.bufferViews[acc.bufferView]
    buf = gltf.buffers[bv.buffer]
    uri: str = buf.uri or ""
    if not uri.startswith("data:"):
        raise ValueError("external buffer files not supported")
    raw = base64.b64decode(uri.split(",", 1)[1])
    offset = (bv.byteOffset or 0) + (acc.byteOffset or 0)
    fmt = f"<{acc.count * n_components}f"
    data = struct.unpack_from(fmt, raw, offset)
    return [list(data[i * n_components:(i + 1) * n_components]) for i in range(acc.count)]


def _read_accessor_uint16(gltf, acc_idx: int) -> list[int]:
    """Read uint16 scalar data from a glTF accessor into a flat list of ints."""
    import base64
    acc = gltf.accessors[acc_idx]
    bv = gltf.bufferViews[acc.bufferView]
    buf = gltf.buffers[bv.buffer]
    uri: str = buf.uri or ""
    if not uri.startswith("data:"):
        raise ValueError("external buffer files not supported")
    raw = base64.b64decode(uri.split(",", 1)[1])
    offset = (bv.byteOffset or 0) + (acc.byteOffset or 0)
    return list(struct.unpack_from(f"<{acc.count}H", raw, offset))


def _extract_buffers(
    gltf,
    source_path: Path,
) -> "tuple[list[dict], list[int], list[dict], tuple]":
    """Extract vertex/index/submesh data from a validated single-mesh glTF.

    Returns (vertices, indices, submeshes, aabb) where:
      vertices : list of dicts with keys: position, normal, tangent, uv0
      indices  : flat list of ints (rebased to combined buffer)
      submeshes: list of dicts with keys: index_start, index_count, material_id
      aabb     : (min_x, min_y, min_z, max_x, max_y, max_z) floats

    Raises ValueError on material-name CRC collision (AC-5a).
    """
    mesh = gltf.meshes[0]

    all_vertices: list[dict] = []
    all_indices: list[int] = []
    all_submeshes: list[dict] = []

    # Track materialId → first material name that produced it (for collision detection).
    seen_ids: dict[int, str] = {}

    aabb_min = [float("inf"), float("inf"), float("inf")]
    aabb_max = [float("-inf"), float("-inf"), float("-inf")]

    for prim in mesh.primitives:
        attrs = prim.attributes
        vertex_base = len(all_vertices)

        # ----- positions -----
        positions = _read_accessor_floats(gltf, attrs.POSITION, 3)

        # ----- normals -----
        normals = _read_accessor_floats(gltf, attrs.NORMAL, 3)

        # ----- tangents (VEC4) -----
        tangents = _read_accessor_floats(gltf, attrs.TANGENT, 4)

        # ----- UVs -----
        uvs = _read_accessor_floats(gltf, attrs.TEXCOORD_0, 2)

        # ----- assemble vertex dicts -----
        for pos, nor, tan, uv in zip(positions, normals, tangents, uvs):
            all_vertices.append({
                "position": pos,
                "normal": nor,
                "tangent": tan,
                "uv0": uv,
            })
            # AABB accumulation
            for axis in range(3):
                if pos[axis] < aabb_min[axis]:
                    aabb_min[axis] = pos[axis]
                if pos[axis] > aabb_max[axis]:
                    aabb_max[axis] = pos[axis]

        # ----- indices (rebased) -----
        raw_indices = _read_accessor_uint16(gltf, prim.indices)
        index_start = len(all_indices)
        for idx in raw_indices:
            all_indices.append(idx + vertex_base)

        # ----- material id -----
        if prim.material is not None and prim.material < len(gltf.materials):
            mat_name: str = gltf.materials[prim.material].name or ""
        else:
            mat_name = ""

        mat_id = _material_id(mat_name)

        # AC-5a: collision guard
        if mat_id in seen_ids:
            existing_name = seen_ids[mat_id]
            if existing_name != mat_name:
                raise ValueError(
                    f"Material CRC collision: '{mat_name}' and '{existing_name}' "
                    f"both hash to materialId {mat_id:#010x}"
                )
        else:
            seen_ids[mat_id] = mat_name

        all_submeshes.append({
            "index_start": index_start,
            "index_count": len(raw_indices),
            "material_id": mat_id,
        })

    aabb = (
        aabb_min[0], aabb_min[1], aabb_min[2],
        aabb_max[0], aabb_max[1], aabb_max[2],
    )
    return all_vertices, all_indices, all_submeshes, aabb


def _pack_mesh3d(
    vertices: list[dict],
    indices: list[int],
    submeshes: list[dict],
    aabb: tuple,
) -> bytes:
    """Pack vertex/index/submesh data into the .mesh3d flat binary.

    Binary layout (matches Mesh3DBinaryReader exactly):
      Header  : 41 bytes — magic(4s) version(B) vertCount(I) idxCount(I) subCount(I) aabb(6f)
      Vertices: vertCount × 52 bytes each
      Indices : idxCount × 2 bytes (uint16 LE)
      Submeshes: subCount × 12 bytes (indexStart, indexCount, materialId — 3×uint32 LE)
    """
    vertex_count = len(vertices)
    index_count = len(indices)
    submesh_count = len(submeshes)

    min_x, min_y, min_z, max_x, max_y, max_z = aabb

    # ---- header ----
    header = struct.pack(
        "<4sB3I6f",
        _MAGIC,
        _VERSION,
        vertex_count,
        index_count,
        submesh_count,
        min_x, min_y, min_z,
        max_x, max_y, max_z,
    )

    # ---- vertices ----
    vertex_buf = bytearray()
    for v in vertices:
        px, py, pz = v["position"]
        nx, ny, nz = v["normal"]
        tx, ty, tz, tw = v["tangent"]
        u, uv = v["uv0"]
        colour = 0xFFFFFFFF  # AC-5b: always white
        vertex_buf += struct.pack(
            "<3f3f4f2fI",
            px, py, pz,
            nx, ny, nz,
            tx, ty, tz, tw,
            u, uv,
            colour,
        )

    # ---- indices ----
    index_buf = struct.pack(f"<{index_count}H", *indices)

    # ---- submeshes ----
    submesh_buf = bytearray()
    for s in submeshes:
        submesh_buf += struct.pack(
            "<3I",
            s["index_start"],
            s["index_count"],
            s["material_id"],
        )

    return header + bytes(vertex_buf) + index_buf + bytes(submesh_buf)


def _resolve_mesh3d_deploy_path(record: dict, context: "BuildContext") -> Path:
    """Resolve deploy path: same scope/tag directory as layout engine, but extension = .mesh3d."""
    from dia_cli.commands.asset.layout import resolve_deploy_path
    layout_path = resolve_deploy_path(record, context)
    source_stem = Path(record.get("source_path", "")).stem
    return layout_path.parent / f"{source_stem}.mesh3d"


class Mesh3DHandler(AssetHandler):
    """Cooks .gltf/.glb into the .mesh3d flat binary (static meshes only)."""

    type_id = "mesh3d"

    def validate(self, record: dict, context: "BuildContext") -> list[AssetError]:
        """Collect all validation errors for a mesh3d asset record.

        Follows SD-APIPE-002: collects ALL errors — no early exit where possible.
        File-not-found is the only case that stops further checks (no content to parse).
        """
        errors: list[AssetError] = []
        asset_id: str = record["asset_id"] if "asset_id" in record else record.get("id", "")

        # ------------------------------------------------------------------ #
        # 1. Source file existence
        # ------------------------------------------------------------------ #
        raw_path: str = record.get("source_path", "")
        source_path = Path(raw_path)
        if not source_path.is_absolute():
            source_path = context.source_root / source_path

        if not source_path.exists():
            errors.append(AssetError(
                asset_id=asset_id,
                phase="validate",
                message=f"Source file not found: {source_path}",
            ))
            return errors  # cannot continue — no file to parse

        # ------------------------------------------------------------------ #
        # 2. Extension check
        # ------------------------------------------------------------------ #
        suffix = source_path.suffix.lower()
        if suffix not in (".gltf", ".glb"):
            errors.append(AssetError(
                asset_id=asset_id,
                phase="validate",
                message=f"Source file must be .gltf or .glb, got '{suffix}'",
            ))
            return errors  # unreadable as glTF — stop here

        # ------------------------------------------------------------------ #
        # 3. Parse the glTF
        # ------------------------------------------------------------------ #
        try:
            import pygltflib  # local import to keep the module loadable without pygltflib

            gltf = pygltflib.GLTF2().load(str(source_path))
        except Exception as exc:
            errors.append(AssetError(
                asset_id=asset_id,
                phase="validate",
                message=f"Failed to parse glTF/glb: {exc}",
            ))
            return errors  # cannot inspect structure

        # ------------------------------------------------------------------ #
        # 4. Mesh count — exactly one mesh
        # ------------------------------------------------------------------ #
        mesh_count = len(gltf.meshes)
        if mesh_count == 0:
            errors.append(AssetError(
                asset_id=asset_id,
                phase="validate",
                message="glTF contains zero meshes; exactly one is required",
            ))
            return errors  # nothing else to check
        if mesh_count > 1:
            errors.append(AssetError(
                asset_id=asset_id,
                phase="validate",
                message=f"glTF contains {mesh_count} meshes; exactly one is required",
            ))
            # Still check the first mesh for further errors

        mesh = gltf.meshes[0]

        # ------------------------------------------------------------------ #
        # 5. Skinning — skins array or JOINTS_0/WEIGHTS_0 attributes
        # ------------------------------------------------------------------ #
        if gltf.skins:
            errors.append(AssetError(
                asset_id=asset_id,
                phase="validate",
                message="glTF is skinned (has a 'skins' entry); only static meshes are supported",
            ))

        # ------------------------------------------------------------------ #
        # 6. Per-primitive checks
        # ------------------------------------------------------------------ #
        total_vertices = 0
        total_indices = 0
        submesh_count = len(mesh.primitives)

        if submesh_count > _MAX_SUBMESHES:
            errors.append(AssetError(
                asset_id=asset_id,
                phase="validate",
                message=f"Mesh has {submesh_count} primitives; maximum is {_MAX_SUBMESHES}",
            ))

        for prim_idx, prim in enumerate(mesh.primitives):
            label = f"primitive[{prim_idx}]"

            # 6a. Mode must be TRIANGLES (4)
            if prim.mode != 4:
                errors.append(AssetError(
                    asset_id=asset_id,
                    phase="validate",
                    message=f"{label}: non-triangle mode {prim.mode} (expected 4=TRIANGLES)",
                ))

            # 6b. Required attributes
            attrs = prim.attributes
            for req in _REQUIRED_ATTRS:
                if getattr(attrs, req, None) is None:
                    errors.append(AssetError(
                        asset_id=asset_id,
                        phase="validate",
                        message=f"{label}: missing required attribute '{req}'",
                    ))

            # 6c. Skinning attributes on primitive
            for skin_attr in _SKINNING_ATTRS:
                if getattr(attrs, skin_attr, None) is not None:
                    errors.append(AssetError(
                        asset_id=asset_id,
                        phase="validate",
                        message=f"{label}: skinning attribute '{skin_attr}' found; only static meshes are supported",
                    ))

            # 6d. Accumulate vertex / index counts for limit checks
            if attrs.POSITION is not None and attrs.POSITION < len(gltf.accessors):
                total_vertices += gltf.accessors[attrs.POSITION].count

            if prim.indices is not None and prim.indices < len(gltf.accessors):
                total_indices += gltf.accessors[prim.indices].count

        # ------------------------------------------------------------------ #
        # 7. Format limits
        # ------------------------------------------------------------------ #
        if total_vertices > _MAX_VERTICES:
            errors.append(AssetError(
                asset_id=asset_id,
                phase="validate",
                message=f"Mesh has {total_vertices} vertices; maximum is {_MAX_VERTICES}",
            ))

        if total_indices > _MAX_INDICES:
            errors.append(AssetError(
                asset_id=asset_id,
                phase="validate",
                message=f"Mesh has {total_indices} indices; maximum is {_MAX_INDICES}",
            ))

        return errors

    def transform(self, record: dict, context: "BuildContext") -> TransformResult:
        """Cook .gltf/.glb to .mesh3d and write to deploy path. deploy() is a no-op."""
        asset_id: str = record["asset_id"] if "asset_id" in record else record.get("id", "")

        # 1. Resolve source path (relative to context.source_root if not absolute)
        raw_path: str = record.get("source_path", "")
        source_path = Path(raw_path)
        if not source_path.is_absolute():
            source_path = context.source_root / source_path

        # 2. Load glTF
        try:
            import pygltflib
            gltf = pygltflib.GLTF2().load(str(source_path))
        except Exception as exc:
            return TransformResult(
                success=False,
                errors=[AssetError(asset_id=asset_id, phase="transform",
                                   message=f"Failed to load glTF: {exc}")],
            )

        # 3. Extract buffers — catch ValueError (material collision) → failure result
        try:
            vertices, indices, submeshes, aabb = _extract_buffers(gltf, source_path)
        except ValueError as exc:
            return TransformResult(
                success=False,
                errors=[AssetError(asset_id=asset_id, phase="transform", message=str(exc))],
            )

        # 4. Pack to binary
        mesh_bytes = _pack_mesh3d(vertices, indices, submeshes, aabb)

        # 5. Resolve deploy path and write
        deploy_path = _resolve_mesh3d_deploy_path(record, context)
        try:
            deploy_path.parent.mkdir(parents=True, exist_ok=True)
            deploy_path.write_bytes(mesh_bytes)
        except OSError as exc:
            return TransformResult(
                success=False,
                errors=[AssetError(asset_id=asset_id, phase="transform",
                                   message=f"Failed to write .mesh3d: {exc}")],
            )

        # 6. Return success
        return TransformResult(success=True, output_path=str(deploy_path))

    def deploy(self, record: dict, context: "BuildContext") -> DeployResult:
        """No-op — transform already wrote the cooked binary to its deploy path."""
        deploy_path = _resolve_mesh3d_deploy_path(record, context)
        return DeployResult(success=True, deploy_path=str(deploy_path))
