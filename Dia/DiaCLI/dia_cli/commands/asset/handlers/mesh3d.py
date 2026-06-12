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


def _resolve_mesh3d_deploy_path(record: dict, context: "BuildContext") -> Path:
    ...  # implement in Task 4


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
        ...  # implement in Task 3/4

    def deploy(self, record: dict, context: "BuildContext") -> DeployResult:
        ...  # implement in Task 4
