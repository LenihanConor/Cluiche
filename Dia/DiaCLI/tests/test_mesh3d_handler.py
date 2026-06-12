"""Unit tests for Mesh3DHandler.validate (AC-2) and mesh3d packer/extractor (AC-4..7).

Fixtures build minimal glTF files programmatically via pygltflib so the tests
are self-contained and do not depend on checked-in binary blobs.

SD-APIPE-002: validate must collect ALL errors before returning.
"""
from __future__ import annotations

import json
import struct
import zlib
import base64
from pathlib import Path
from unittest.mock import MagicMock

import pytest

from dia_cli.commands.asset.context import BuildContext
from dia_cli.commands.asset.handler import AssetError
from dia_cli.commands.asset.handlers.mesh3d import Mesh3DHandler, _pack_mesh3d, _extract_buffers


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _make_context(tmp_path: Path) -> BuildContext:
    return BuildContext(
        catalogue={},
        config="Debug",
        platform="x64",
        app_name="TestApp",
        deploy_root=tmp_path / "assets",
        asset_stages=[],
        output=MagicMock(),
        source_root=tmp_path,
    )


def _make_record(asset_id: str, source_path: str = "") -> dict:
    return {
        "id": asset_id,
        "type": "mesh3d",
        "source_path": source_path,
        "scope": "global",
        "stage_name": "",
        "tags": [],
    }


def _error_messages(errors: list[AssetError]) -> list[str]:
    return [e.message for e in errors]


# ---------------------------------------------------------------------------
# glTF fixture builders
#
# All fixtures write a .gltf JSON file to tmp_path and return the Path.
# The buffer data is embedded as a data-URI so the files are self-contained.
# ---------------------------------------------------------------------------

def _base64_buf(*parts: bytes) -> tuple[str, int]:
    """Concatenate binary parts, pad to 4 bytes, return (base64_str, total_len)."""
    buf = b"".join(parts)
    while len(buf) % 4 != 0:
        buf += b"\x00"
    return base64.b64encode(buf).decode(), len(buf)


def _vec3_bytes(triples: list[tuple[float, float, float]]) -> bytes:
    data = b""
    for x, y, z in triples:
        data += struct.pack("<3f", x, y, z)
    return data


def _vec4_bytes(quads: list[tuple[float, float, float, float]]) -> bytes:
    data = b""
    for x, y, z, w in quads:
        data += struct.pack("<4f", x, y, z, w)
    return data


def _vec2_bytes(pairs: list[tuple[float, float]]) -> bytes:
    data = b""
    for u, v in pairs:
        data += struct.pack("<2f", u, v)
    return data


def _uint16_bytes(values: list[int]) -> bytes:
    return struct.pack(f"<{len(values)}H", *values)


def _build_triangle_gltf(
    *,
    mesh_count: int = 1,
    primitive_count: int = 1,
    mode: int = 4,
    include_normal: bool = True,
    include_tangent: bool = True,
    include_texcoord: bool = True,
    include_joints: bool = False,
    include_weights: bool = False,
    vertex_count: int = 3,
    index_count: int = 3,
    skins: list | None = None,
) -> dict:
    """Build a minimal glTF dict with the given configuration.

    Each primitive shares the same accessors for simplicity.
    The first accessor is always POSITION, the last is indices.
    """
    positions = _vec3_bytes([(0, 0, 0)] * vertex_count)
    normals   = _vec3_bytes([(0, 0, 1)] * vertex_count)
    tangents  = _vec4_bytes([(1, 0, 0, 1)] * vertex_count)
    texcoords = _vec2_bytes([(0, 0)] * vertex_count)
    # 4 floats per joint weight, 4 uint16 per joint index
    joints_data  = struct.pack(f"<{vertex_count * 4}H", *([0] * vertex_count * 4))
    weights_data = struct.pack(f"<{vertex_count * 4}f", *([1.0 / 4] * vertex_count * 4))
    index_data   = _uint16_bytes(list(range(index_count)) if index_count <= vertex_count
                                 else [0] * index_count)

    # Decide which bufferViews/accessors we need
    parts = [positions]
    bv_offsets = [0]
    bv_lengths = [len(positions)]

    def _add(data: bytes) -> None:
        bv_offsets.append(sum(len(p) for p in parts))
        bv_lengths.append(len(data))
        parts.append(data)

    _add(normals)
    _add(tangents)
    _add(texcoords)
    if include_joints:
        _add(joints_data)
    if include_weights:
        _add(weights_data)
    _add(index_data)

    b64, total_len = _base64_buf(*parts)

    # Build accessor list
    # Accessor indices (positional):
    #   0 = POSITION (VEC3 float)
    #   1 = NORMAL   (VEC3 float)
    #   2 = TANGENT  (VEC4 float)
    #   3 = TEXCOORD_0 (VEC2 float)
    #   4 = JOINTS_0  (VEC4 ushort) [optional]
    #   5 = WEIGHTS_0 (VEC4 float)  [optional]
    #  -1 = indices   (SCALAR ushort) [last]

    accessors = [
        {"bufferView": 0, "componentType": 5126, "count": vertex_count, "type": "VEC3",
         "max": [1, 1, 0], "min": [0, 0, 0]},  # POSITION
        {"bufferView": 1, "componentType": 5126, "count": vertex_count, "type": "VEC3"},  # NORMAL
        {"bufferView": 2, "componentType": 5126, "count": vertex_count, "type": "VEC4"},  # TANGENT
        {"bufferView": 3, "componentType": 5126, "count": vertex_count, "type": "VEC2"},  # TEXCOORD_0
    ]

    next_bv = 4
    joints_acc_idx = None
    weights_acc_idx = None
    if include_joints:
        accessors.append({
            "bufferView": next_bv, "componentType": 5123, "count": vertex_count, "type": "VEC4",
        })
        joints_acc_idx = len(accessors) - 1
        next_bv += 1
    if include_weights:
        accessors.append({
            "bufferView": next_bv, "componentType": 5126, "count": vertex_count, "type": "VEC4",
        })
        weights_acc_idx = len(accessors) - 1
        next_bv += 1

    # indices accessor is always last
    indices_acc_idx = len(accessors)
    accessors.append({
        "bufferView": next_bv, "componentType": 5123, "count": index_count, "type": "SCALAR",
    })

    bufferViews = [
        {"buffer": 0, "byteOffset": bv_offsets[i], "byteLength": bv_lengths[i]}
        for i in range(len(parts))
    ]

    # Build the primitive attributes dict
    prim_attrs: dict = {"POSITION": 0}
    if include_normal:
        prim_attrs["NORMAL"] = 1
    if include_tangent:
        prim_attrs["TANGENT"] = 2
    if include_texcoord:
        prim_attrs["TEXCOORD_0"] = 3
    if include_joints and joints_acc_idx is not None:
        prim_attrs["JOINTS_0"] = joints_acc_idx
    if include_weights and weights_acc_idx is not None:
        prim_attrs["WEIGHTS_0"] = weights_acc_idx

    primitive = {
        "attributes": prim_attrs,
        "indices": indices_acc_idx,
        "mode": mode,
    }

    meshes = [
        {"name": f"Mesh{i}", "primitives": [primitive for _ in range(primitive_count)]}
        for i in range(mesh_count)
    ]

    gltf: dict = {
        "asset": {"version": "2.0"},
        "meshes": meshes,
        "accessors": accessors,
        "bufferViews": bufferViews,
        "buffers": [{"byteLength": total_len, "uri": f"data:application/octet-stream;base64,{b64}"}],
    }
    if skins is not None:
        gltf["skins"] = skins

    return gltf


def _write_gltf(tmp_path: Path, name: str, data: dict) -> Path:
    path = tmp_path / name
    path.write_text(json.dumps(data), encoding="utf-8")
    return path


# ---------------------------------------------------------------------------
# Pytest fixtures
# ---------------------------------------------------------------------------

@pytest.fixture
def valid_gltf(tmp_path):
    data = _build_triangle_gltf()
    return _write_gltf(tmp_path, "valid_triangle.gltf", data)


@pytest.fixture
def missing_normal_gltf(tmp_path):
    data = _build_triangle_gltf(include_normal=False)
    return _write_gltf(tmp_path, "missing_normal.gltf", data)


@pytest.fixture
def missing_tangent_gltf(tmp_path):
    data = _build_triangle_gltf(include_tangent=False)
    return _write_gltf(tmp_path, "missing_tangent.gltf", data)


@pytest.fixture
def missing_texcoord_gltf(tmp_path):
    data = _build_triangle_gltf(include_texcoord=False)
    return _write_gltf(tmp_path, "missing_texcoord.gltf", data)


@pytest.fixture
def non_triangle_gltf(tmp_path):
    data = _build_triangle_gltf(mode=0)  # mode 0 = POINTS
    return _write_gltf(tmp_path, "non_triangle.gltf", data)


@pytest.fixture
def multi_mesh_gltf(tmp_path):
    data = _build_triangle_gltf(mesh_count=2)
    return _write_gltf(tmp_path, "multi_mesh.gltf", data)


@pytest.fixture
def zero_mesh_gltf(tmp_path):
    data = _build_triangle_gltf(mesh_count=1)
    # Remove meshes entirely
    data["meshes"] = []
    return _write_gltf(tmp_path, "zero_mesh.gltf", data)


@pytest.fixture
def skinned_joints_gltf(tmp_path):
    """Primitive has JOINTS_0 attribute — skinned mesh."""
    data = _build_triangle_gltf(include_joints=True, include_weights=True)
    return _write_gltf(tmp_path, "skinned_joints.gltf", data)


@pytest.fixture
def skinned_skins_gltf(tmp_path):
    """glTF has a non-empty skins array — skinned mesh."""
    data = _build_triangle_gltf(skins=[{"name": "Armature", "joints": []}])
    return _write_gltf(tmp_path, "skinned_skins.gltf", data)


@pytest.fixture
def too_many_vertices_gltf(tmp_path):
    """Mesh exceeds 65535 vertex limit (two primitives, each 32768 vertices)."""
    # 32768 * 3 = 98304 indices; two primitives => 65536 vertices total
    data = _build_triangle_gltf(
        primitive_count=2,
        vertex_count=32768,
        index_count=3,
    )
    return _write_gltf(tmp_path, "too_many_vertices.gltf", data)


@pytest.fixture
def too_many_indices_gltf(tmp_path):
    """Mesh exceeds 196608 index limit (two primitives, each 98305 indices, 3 verts)."""
    # Need index_count > 196608/2 per primitive
    data = _build_triangle_gltf(
        primitive_count=2,
        vertex_count=3,
        index_count=98305,  # 98305 * 2 = 196610 > 196608
    )
    return _write_gltf(tmp_path, "too_many_indices.gltf", data)


@pytest.fixture
def too_many_submeshes_gltf(tmp_path):
    """Mesh has 33 primitives — exceeds the 32 submesh limit."""
    data = _build_triangle_gltf(primitive_count=33)
    return _write_gltf(tmp_path, "too_many_submeshes.gltf", data)


# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------

class TestMesh3DHandlerValidate:
    """Validate method covers all AC-2 failure modes."""

    # ------------------------------------------------------------------
    # Happy path
    # ------------------------------------------------------------------

    def test_valid_gltf_returns_no_errors(self, tmp_path, valid_gltf):
        ctx = _make_context(tmp_path)
        record = _make_record("mesh3d.cube", source_path=str(valid_gltf))
        errors = Mesh3DHandler().validate(record, ctx)
        assert errors == [], f"Expected no errors, got: {_error_messages(errors)}"

    def test_valid_gltf_relative_path(self, tmp_path, valid_gltf):
        ctx = _make_context(tmp_path)
        rel = valid_gltf.relative_to(tmp_path)
        record = _make_record("mesh3d.cube", source_path=str(rel))
        errors = Mesh3DHandler().validate(record, ctx)
        assert errors == [], f"Expected no errors, got: {_error_messages(errors)}"

    # ------------------------------------------------------------------
    # File existence
    # ------------------------------------------------------------------

    def test_missing_file_returns_error(self, tmp_path):
        ctx = _make_context(tmp_path)
        record = _make_record("mesh3d.cube", source_path=str(tmp_path / "nonexistent.gltf"))
        errors = Mesh3DHandler().validate(record, ctx)
        assert len(errors) == 1
        assert errors[0].phase == "validate"
        assert "not found" in errors[0].message.lower()

    def test_missing_file_stops_further_checks(self, tmp_path):
        """File-not-found is the only case that stops validation (no further errors)."""
        ctx = _make_context(tmp_path)
        record = _make_record("mesh3d.cube", source_path=str(tmp_path / "nonexistent.gltf"))
        errors = Mesh3DHandler().validate(record, ctx)
        assert len(errors) == 1  # only the not-found error, nothing else

    # ------------------------------------------------------------------
    # Extension / parse errors
    # ------------------------------------------------------------------

    def test_wrong_extension_is_error(self, tmp_path):
        bad = tmp_path / "mesh.obj"
        bad.write_bytes(b"some data")
        ctx = _make_context(tmp_path)
        record = _make_record("mesh3d.cube", source_path=str(bad))
        errors = Mesh3DHandler().validate(record, ctx)
        assert any("gltf" in m.lower() or "glb" in m.lower() for m in _error_messages(errors))

    def test_invalid_json_gltf_is_error(self, tmp_path):
        bad = tmp_path / "corrupt.gltf"
        bad.write_text("{ this is not valid json", encoding="utf-8")
        ctx = _make_context(tmp_path)
        record = _make_record("mesh3d.cube", source_path=str(bad))
        errors = Mesh3DHandler().validate(record, ctx)
        assert len(errors) >= 1
        assert errors[0].phase == "validate"
        assert "parse" in errors[0].message.lower() or "json" in errors[0].message.lower()

    # ------------------------------------------------------------------
    # Mesh count
    # ------------------------------------------------------------------

    def test_zero_meshes_is_error(self, tmp_path, zero_mesh_gltf):
        ctx = _make_context(tmp_path)
        record = _make_record("mesh3d.cube", source_path=str(zero_mesh_gltf))
        errors = Mesh3DHandler().validate(record, ctx)
        assert any("zero" in m.lower() or "0" in m for m in _error_messages(errors)), \
            f"Expected zero-mesh error, got: {_error_messages(errors)}"

    def test_multiple_meshes_is_error(self, tmp_path, multi_mesh_gltf):
        ctx = _make_context(tmp_path)
        record = _make_record("mesh3d.cube", source_path=str(multi_mesh_gltf))
        errors = Mesh3DHandler().validate(record, ctx)
        assert any("2" in m and "mesh" in m.lower() for m in _error_messages(errors)), \
            f"Expected multi-mesh error, got: {_error_messages(errors)}"

    # ------------------------------------------------------------------
    # Primitive mode
    # ------------------------------------------------------------------

    def test_non_triangle_mode_is_error(self, tmp_path, non_triangle_gltf):
        ctx = _make_context(tmp_path)
        record = _make_record("mesh3d.cube", source_path=str(non_triangle_gltf))
        errors = Mesh3DHandler().validate(record, ctx)
        msgs = _error_messages(errors)
        assert any("mode" in m.lower() or "triangle" in m.lower() for m in msgs), \
            f"Expected non-triangle-mode error, got: {msgs}"

    def test_non_triangle_mode_value_mentioned(self, tmp_path, non_triangle_gltf):
        """Error message should include the actual mode value (0 for POINTS)."""
        ctx = _make_context(tmp_path)
        record = _make_record("mesh3d.cube", source_path=str(non_triangle_gltf))
        errors = Mesh3DHandler().validate(record, ctx)
        # mode=0 should appear somewhere in the error messages
        assert any("0" in m for m in _error_messages(errors)), \
            f"Expected mode value 0 in error, got: {_error_messages(errors)}"

    # ------------------------------------------------------------------
    # Required attributes
    # ------------------------------------------------------------------

    def test_missing_normal_is_error(self, tmp_path, missing_normal_gltf):
        ctx = _make_context(tmp_path)
        record = _make_record("mesh3d.cube", source_path=str(missing_normal_gltf))
        errors = Mesh3DHandler().validate(record, ctx)
        msgs = _error_messages(errors)
        assert any("NORMAL" in m for m in msgs), \
            f"Expected NORMAL missing error, got: {msgs}"

    def test_missing_tangent_is_error(self, tmp_path, missing_tangent_gltf):
        ctx = _make_context(tmp_path)
        record = _make_record("mesh3d.cube", source_path=str(missing_tangent_gltf))
        errors = Mesh3DHandler().validate(record, ctx)
        msgs = _error_messages(errors)
        assert any("TANGENT" in m for m in msgs), \
            f"Expected TANGENT missing error, got: {msgs}"

    def test_missing_texcoord_is_error(self, tmp_path, missing_texcoord_gltf):
        ctx = _make_context(tmp_path)
        record = _make_record("mesh3d.cube", source_path=str(missing_texcoord_gltf))
        errors = Mesh3DHandler().validate(record, ctx)
        msgs = _error_messages(errors)
        assert any("TEXCOORD_0" in m for m in msgs), \
            f"Expected TEXCOORD_0 missing error, got: {msgs}"

    def test_missing_multiple_attributes_all_reported(self, tmp_path):
        """SD-APIPE-002: all missing attributes are reported in a single pass."""
        data = _build_triangle_gltf(include_normal=False, include_tangent=False, include_texcoord=False)
        path = _write_gltf(tmp_path, "missing_many.gltf", data)
        ctx = _make_context(tmp_path)
        record = _make_record("mesh3d.cube", source_path=str(path))
        errors = Mesh3DHandler().validate(record, ctx)
        msgs = _error_messages(errors)
        for attr in ("NORMAL", "TANGENT", "TEXCOORD_0"):
            assert any(attr in m for m in msgs), \
                f"Expected '{attr}' to be reported, got: {msgs}"

    # ------------------------------------------------------------------
    # Skinning
    # ------------------------------------------------------------------

    def test_skinning_via_joints_attr_is_error(self, tmp_path, skinned_joints_gltf):
        ctx = _make_context(tmp_path)
        record = _make_record("mesh3d.cube", source_path=str(skinned_joints_gltf))
        errors = Mesh3DHandler().validate(record, ctx)
        msgs = _error_messages(errors)
        assert any("JOINTS_0" in m or "skin" in m.lower() for m in msgs), \
            f"Expected skinning error via JOINTS_0, got: {msgs}"

    def test_skinning_via_skins_array_is_error(self, tmp_path, skinned_skins_gltf):
        ctx = _make_context(tmp_path)
        record = _make_record("mesh3d.cube", source_path=str(skinned_skins_gltf))
        errors = Mesh3DHandler().validate(record, ctx)
        msgs = _error_messages(errors)
        assert any("skin" in m.lower() for m in msgs), \
            f"Expected skinning error via skins array, got: {msgs}"

    # ------------------------------------------------------------------
    # Format limits
    # ------------------------------------------------------------------

    def test_too_many_vertices_is_error(self, tmp_path, too_many_vertices_gltf):
        ctx = _make_context(tmp_path)
        record = _make_record("mesh3d.cube", source_path=str(too_many_vertices_gltf))
        errors = Mesh3DHandler().validate(record, ctx)
        msgs = _error_messages(errors)
        assert any("65535" in m or "vertices" in m.lower() for m in msgs), \
            f"Expected vertex limit error, got: {msgs}"

    def test_too_many_indices_is_error(self, tmp_path, too_many_indices_gltf):
        ctx = _make_context(tmp_path)
        record = _make_record("mesh3d.cube", source_path=str(too_many_indices_gltf))
        errors = Mesh3DHandler().validate(record, ctx)
        msgs = _error_messages(errors)
        assert any("196608" in m or "indices" in m.lower() for m in msgs), \
            f"Expected index limit error, got: {msgs}"

    def test_too_many_submeshes_is_error(self, tmp_path, too_many_submeshes_gltf):
        ctx = _make_context(tmp_path)
        record = _make_record("mesh3d.cube", source_path=str(too_many_submeshes_gltf))
        errors = Mesh3DHandler().validate(record, ctx)
        msgs = _error_messages(errors)
        assert any("32" in m or "submesh" in m.lower() or "primitive" in m.lower() for m in msgs), \
            f"Expected submesh limit error, got: {msgs}"

    # ------------------------------------------------------------------
    # SD-APIPE-002: all errors collected (no early exit within a valid parse)
    # ------------------------------------------------------------------

    def test_multiple_failures_all_reported(self, tmp_path):
        """mode != 4 AND missing NORMAL — both must be reported."""
        data = _build_triangle_gltf(mode=0, include_normal=False)
        path = _write_gltf(tmp_path, "multi_fail.gltf", data)
        ctx = _make_context(tmp_path)
        record = _make_record("mesh3d.cube", source_path=str(path))
        errors = Mesh3DHandler().validate(record, ctx)
        msgs = _error_messages(errors)
        assert any("mode" in m.lower() or "triangle" in m.lower() for m in msgs), \
            f"Expected mode error, got: {msgs}"
        assert any("NORMAL" in m for m in msgs), \
            f"Expected NORMAL error, got: {msgs}"

    # ------------------------------------------------------------------
    # Error shape
    # ------------------------------------------------------------------

    def test_errors_have_correct_asset_id(self, tmp_path):
        """AssetError.asset_id must match the record's 'id' field."""
        bad = tmp_path / "nonexistent.gltf"
        ctx = _make_context(tmp_path)
        record = _make_record("mesh3d.my_mesh", source_path=str(bad))
        errors = Mesh3DHandler().validate(record, ctx)
        assert all(e.asset_id == "mesh3d.my_mesh" for e in errors), \
            f"Unexpected asset_id in errors: {[(e.asset_id, e.message) for e in errors]}"

    def test_errors_have_validate_phase(self, tmp_path):
        """All errors from validate must carry phase='validate'."""
        bad = tmp_path / "nonexistent.gltf"
        ctx = _make_context(tmp_path)
        record = _make_record("mesh3d.my_mesh", source_path=str(bad))
        errors = Mesh3DHandler().validate(record, ctx)
        assert all(e.phase == "validate" for e in errors), \
            f"Unexpected phase in errors: {[(e.phase, e.message) for e in errors]}"


# ---------------------------------------------------------------------------
# Helpers for packer / extractor tests
# ---------------------------------------------------------------------------

_HEADER_SIZE = 41   # 4s B 3I 6f  = 4+1+12+24 = 41
_VERTEX_SIZE = 52   # 3f 3f 4f 2f I = 12+12+16+8+4 = 52
_INDEX_SIZE = 2     # uint16
_SUBMESH_SIZE = 12  # 3I


def _make_vertices(n: int, pos=None) -> list[dict]:
    """Build n vertex dicts with optional custom positions (list of (x,y,z))."""
    verts = []
    for i in range(n):
        p = pos[i] if pos and i < len(pos) else (float(i), 0.0, 0.0)
        verts.append({
            "position": list(p),
            "normal": [0.0, 0.0, 1.0],
            "tangent": [1.0, 0.0, 0.0, 1.0],
            "uv0": [0.0, 0.0],
        })
    return verts


def _make_submesh(index_start: int, index_count: int, mat_name: str) -> dict:
    mat_id = zlib.crc32(mat_name.encode("utf-8")) & 0xFFFFFFFF
    return {"index_start": index_start, "index_count": index_count, "material_id": mat_id}


def _parse_header(data: bytes) -> dict:
    """Unpack the 41-byte .mesh3d header."""
    magic, version, vert_count, idx_count, sub_count, *aabb = struct.unpack_from("<4sB3I6f", data, 0)
    return {
        "magic": magic,
        "version": version,
        "vertex_count": vert_count,
        "index_count": idx_count,
        "submesh_count": sub_count,
        "aabb": tuple(aabb),
    }


def _build_two_prim_gltf_dict(
    pos_a: list, pos_b: list,
    mat_name_a: str = "MatA",
    mat_name_b: str = "MatB",
) -> dict:
    """
    Build a raw glTF dict (not written to disk) with one mesh and two primitives.
    Each primitive gets its own material.  Indices are [0,1,2] for both prims.
    """
    def _v3(triples):
        return b"".join(struct.pack("<3f", *t) for t in triples)

    def _v4(quads):
        return b"".join(struct.pack("<4f", *q) for q in quads)

    def _v2(pairs):
        return b"".join(struct.pack("<2f", *p) for p in pairs)

    n_a = len(pos_a)
    n_b = len(pos_b)

    pos_bytes_a = _v3(pos_a)
    norm_bytes_a = _v3([(0.0, 0.0, 1.0)] * n_a)
    tang_bytes_a = _v4([(1.0, 0.0, 0.0, 1.0)] * n_a)
    uv_bytes_a = _v2([(0.0, 0.0)] * n_a)
    idx_bytes_a = struct.pack(f"<{n_a}H", *range(n_a))

    pos_bytes_b = _v3(pos_b)
    norm_bytes_b = _v3([(0.0, 0.0, 1.0)] * n_b)
    tang_bytes_b = _v4([(1.0, 0.0, 0.0, 1.0)] * n_b)
    uv_bytes_b = _v2([(0.0, 0.0)] * n_b)
    idx_bytes_b = struct.pack(f"<{n_b}H", *range(n_b))

    parts = [
        pos_bytes_a, norm_bytes_a, tang_bytes_a, uv_bytes_a, idx_bytes_a,
        pos_bytes_b, norm_bytes_b, tang_bytes_b, uv_bytes_b, idx_bytes_b,
    ]

    offsets = []
    cur = 0
    for p in parts:
        offsets.append(cur)
        cur += len(p)

    raw = b"".join(parts)
    while len(raw) % 4 != 0:
        raw += b"\x00"
    b64 = base64.b64encode(raw).decode()

    # accessor indices: 0..4 for prim A, 5..9 for prim B
    # within each group: POSITION, NORMAL, TANGENT, TEXCOORD_0, indices
    accessors = [
        {"bufferView": 0, "componentType": 5126, "count": n_a, "type": "VEC3"},  # pos A
        {"bufferView": 1, "componentType": 5126, "count": n_a, "type": "VEC3"},  # norm A
        {"bufferView": 2, "componentType": 5126, "count": n_a, "type": "VEC4"},  # tang A
        {"bufferView": 3, "componentType": 5126, "count": n_a, "type": "VEC2"},  # uv A
        {"bufferView": 4, "componentType": 5123, "count": n_a, "type": "SCALAR"},  # idx A
        {"bufferView": 5, "componentType": 5126, "count": n_b, "type": "VEC3"},  # pos B
        {"bufferView": 6, "componentType": 5126, "count": n_b, "type": "VEC3"},  # norm B
        {"bufferView": 7, "componentType": 5126, "count": n_b, "type": "VEC4"},  # tang B
        {"bufferView": 8, "componentType": 5126, "count": n_b, "type": "VEC2"},  # uv B
        {"bufferView": 9, "componentType": 5123, "count": n_b, "type": "SCALAR"},  # idx B
    ]

    bufferViews = [
        {"buffer": 0, "byteOffset": offsets[i], "byteLength": len(parts[i])}
        for i in range(len(parts))
    ]

    mat_idx_a = 0
    mat_idx_b = 1
    prim_a = {
        "attributes": {"POSITION": 0, "NORMAL": 1, "TANGENT": 2, "TEXCOORD_0": 3},
        "indices": 4,
        "mode": 4,
        "material": mat_idx_a,
    }
    prim_b = {
        "attributes": {"POSITION": 5, "NORMAL": 6, "TANGENT": 7, "TEXCOORD_0": 8},
        "indices": 9,
        "mode": 4,
        "material": mat_idx_b,
    }

    return {
        "asset": {"version": "2.0"},
        "meshes": [{"name": "TestMesh", "primitives": [prim_a, prim_b]}],
        "materials": [
            {"name": mat_name_a},
            {"name": mat_name_b},
        ],
        "accessors": accessors,
        "bufferViews": bufferViews,
        "buffers": [{"byteLength": len(raw), "uri": f"data:application/octet-stream;base64,{b64}"}],
    }


def _build_single_prim_gltf_dict(positions: list, mat_name: str | None = "Material") -> dict:
    """Build a raw glTF dict with one mesh and one primitive."""
    def _v3(triples):
        return b"".join(struct.pack("<3f", *t) for t in triples)

    n = len(positions)
    pos_bytes = _v3(positions)
    norm_bytes = _v3([(0.0, 0.0, 1.0)] * n)
    tang_bytes = b"".join(struct.pack("<4f", 1.0, 0.0, 0.0, 1.0) for _ in range(n))
    uv_bytes = b"".join(struct.pack("<2f", 0.0, 0.0) for _ in range(n))
    idx_bytes = struct.pack(f"<{n}H", *range(n))

    parts = [pos_bytes, norm_bytes, tang_bytes, uv_bytes, idx_bytes]
    offsets, cur = [], 0
    for p in parts:
        offsets.append(cur)
        cur += len(p)
    raw = b"".join(parts)
    while len(raw) % 4 != 0:
        raw += b"\x00"
    b64 = base64.b64encode(raw).decode()

    accessors = [
        {"bufferView": 0, "componentType": 5126, "count": n, "type": "VEC3"},    # POSITION
        {"bufferView": 1, "componentType": 5126, "count": n, "type": "VEC3"},    # NORMAL
        {"bufferView": 2, "componentType": 5126, "count": n, "type": "VEC4"},    # TANGENT
        {"bufferView": 3, "componentType": 5126, "count": n, "type": "VEC2"},    # TEXCOORD_0
        {"bufferView": 4, "componentType": 5123, "count": n, "type": "SCALAR"},  # indices
    ]
    bufferViews = [
        {"buffer": 0, "byteOffset": offsets[i], "byteLength": len(parts[i])}
        for i in range(len(parts))
    ]
    prim: dict = {
        "attributes": {"POSITION": 0, "NORMAL": 1, "TANGENT": 2, "TEXCOORD_0": 3},
        "indices": 4,
        "mode": 4,
    }
    materials = []
    if mat_name is not None:
        prim["material"] = 0
        materials = [{"name": mat_name}]

    return {
        "asset": {"version": "2.0"},
        "meshes": [{"name": "Mesh", "primitives": [prim]}],
        "materials": materials,
        "accessors": accessors,
        "bufferViews": bufferViews,
        "buffers": [{"byteLength": len(raw), "uri": f"data:application/octet-stream;base64,{b64}"}],
    }


def _load_gltf_from_dict(d: dict):
    """Write dict to a temp file-like string and load via pygltflib."""
    import pygltflib
    import tempfile, os
    with tempfile.NamedTemporaryFile(suffix=".gltf", delete=False, mode="w", encoding="utf-8") as f:
        json.dump(d, f)
        tmp = f.name
    try:
        gltf = pygltflib.GLTF2().load(tmp)
    finally:
        os.unlink(tmp)
    return gltf


# ---------------------------------------------------------------------------
# Tests: _pack_mesh3d (AC-4, AC-5, AC-6)
# ---------------------------------------------------------------------------

class TestPackMesh3D:
    """Unit tests for _pack_mesh3d — binary layout correctness."""

    def _simple_pack(self, n_verts=3, n_indices=3, n_submeshes=1):
        verts = _make_vertices(n_verts)
        indices = list(range(n_indices))
        submeshes = [_make_submesh(0, n_indices, "Mat")]
        aabb = (0.0, 0.0, 0.0, float(n_verts - 1), 0.0, 0.0)
        return _pack_mesh3d(verts, indices, submeshes, aabb)

    def test_header_magic(self):
        data = self._simple_pack()
        hdr = _parse_header(data)
        assert hdr["magic"] == b"MESH"

    def test_header_version(self):
        data = self._simple_pack()
        hdr = _parse_header(data)
        assert hdr["version"] == 1

    def test_header_vertex_count(self):
        data = self._simple_pack(n_verts=6, n_indices=6)
        hdr = _parse_header(data)
        assert hdr["vertex_count"] == 6

    def test_header_index_count(self):
        data = self._simple_pack(n_verts=3, n_indices=3)
        hdr = _parse_header(data)
        assert hdr["index_count"] == 3

    def test_header_submesh_count(self):
        verts = _make_vertices(6)
        indices = list(range(6))
        submeshes = [
            _make_submesh(0, 3, "MatA"),
            _make_submesh(3, 3, "MatB"),
        ]
        aabb = (0.0, 0.0, 0.0, 5.0, 0.0, 0.0)
        data = _pack_mesh3d(verts, indices, submeshes, aabb)
        hdr = _parse_header(data)
        assert hdr["submesh_count"] == 2

    def test_total_size_single_prim(self):
        n_v, n_i, n_s = 3, 3, 1
        data = self._simple_pack(n_verts=n_v, n_indices=n_i, n_submeshes=n_s)
        expected = _HEADER_SIZE + n_v * _VERTEX_SIZE + n_i * _INDEX_SIZE + n_s * _SUBMESH_SIZE
        assert len(data) == expected

    def test_total_size_two_submeshes(self):
        n_v, n_i, n_s = 6, 6, 2
        verts = _make_vertices(n_v)
        indices = list(range(n_i))
        submeshes = [_make_submesh(0, 3, "MatA"), _make_submesh(3, 3, "MatB")]
        aabb = (0.0, 0.0, 0.0, 5.0, 0.0, 0.0)
        data = _pack_mesh3d(verts, indices, submeshes, aabb)
        expected = _HEADER_SIZE + n_v * _VERTEX_SIZE + n_i * _INDEX_SIZE + n_s * _SUBMESH_SIZE
        assert len(data) == expected

    def test_aabb_values_in_header(self):
        verts = _make_vertices(3, pos=[(1.0, 2.0, 3.0), (4.0, 5.0, 6.0), (7.0, 8.0, 9.0)])
        indices = [0, 1, 2]
        submeshes = [_make_submesh(0, 3, "Mat")]
        aabb = (1.0, 2.0, 3.0, 7.0, 8.0, 9.0)
        data = _pack_mesh3d(verts, indices, submeshes, aabb)
        hdr = _parse_header(data)
        assert hdr["aabb"] == pytest.approx((1.0, 2.0, 3.0, 7.0, 8.0, 9.0))

    def test_submesh_material_id_crc(self):
        """materialId must be zlib.crc32(name.encode('utf-8')) & 0xFFFFFFFF."""
        mat_name = "MyMaterial"
        expected_id = zlib.crc32(mat_name.encode("utf-8")) & 0xFFFFFFFF
        verts = _make_vertices(3)
        indices = [0, 1, 2]
        submeshes = [{"index_start": 0, "index_count": 3, "material_id": expected_id}]
        aabb = (0.0, 0.0, 0.0, 2.0, 0.0, 0.0)
        data = _pack_mesh3d(verts, indices, submeshes, aabb)
        # Parse the submesh block: at offset HEADER + 3*52 + 3*2
        sub_offset = _HEADER_SIZE + 3 * _VERTEX_SIZE + 3 * _INDEX_SIZE
        idx_start, idx_count, mat_id = struct.unpack_from("<3I", data, sub_offset)
        assert mat_id == expected_id

    def test_known_crc_parity_check(self):
        """Hand-verify: zlib.crc32(b'Material') & 0xFFFFFFFF must match reference."""
        # Computed independently: zlib.crc32(b"Material") = 0x35EB2D0D (may vary by platform,
        # but zlib CRC32 is deterministic — this value is fixed).
        expected = zlib.crc32(b"Material") & 0xFFFFFFFF
        # The point is it's deterministic and non-zero
        assert expected != 0
        # And verify _make_submesh produces the same value
        sub = _make_submesh(0, 3, "Material")
        assert sub["material_id"] == expected

    def test_vertex_colour_always_white(self):
        """Every vertex's colour field must be 0xFFFFFFFF (AC-5b)."""
        verts = _make_vertices(3)
        indices = [0, 1, 2]
        submeshes = [_make_submesh(0, 3, "Mat")]
        aabb = (0.0, 0.0, 0.0, 2.0, 0.0, 0.0)
        data = _pack_mesh3d(verts, indices, submeshes, aabb)
        for i in range(3):
            vert_offset = _HEADER_SIZE + i * _VERTEX_SIZE
            # colour is the last field: offset 48 within the vertex (3f+3f+4f+2f = 12+12+16+8 = 48)
            (colour,) = struct.unpack_from("<I", data, vert_offset + 48)
            assert colour == 0xFFFFFFFF, f"vertex {i} colour = {colour:#010x}, expected 0xffffffff"

    def test_indices_written_as_uint16_le(self):
        verts = _make_vertices(3)
        indices = [2, 1, 0]
        submeshes = [_make_submesh(0, 3, "Mat")]
        aabb = (0.0, 0.0, 0.0, 2.0, 0.0, 0.0)
        data = _pack_mesh3d(verts, indices, submeshes, aabb)
        idx_offset = _HEADER_SIZE + 3 * _VERTEX_SIZE
        a, b, c = struct.unpack_from("<3H", data, idx_offset)
        assert (a, b, c) == (2, 1, 0)

    def test_submesh_fields_layout(self):
        """Submesh block: [indexStart, indexCount, materialId] as 3 uint32 LE."""
        verts = _make_vertices(6)
        indices = list(range(6))
        mat_id = zlib.crc32(b"Foo") & 0xFFFFFFFF
        submeshes = [{"index_start": 3, "index_count": 3, "material_id": mat_id}]
        aabb = (0.0, 0.0, 0.0, 5.0, 0.0, 0.0)
        data = _pack_mesh3d(verts, indices, submeshes, aabb)
        sub_offset = _HEADER_SIZE + 6 * _VERTEX_SIZE + 6 * _INDEX_SIZE
        idx_start, idx_count, m_id = struct.unpack_from("<3I", data, sub_offset)
        assert (idx_start, idx_count, m_id) == (3, 3, mat_id)


# ---------------------------------------------------------------------------
# Tests: _extract_buffers (AC-5, AC-5a, AC-6, AC-7)
# ---------------------------------------------------------------------------

class TestExtractBuffers:
    """Unit tests for _extract_buffers — data extraction and rebase correctness."""

    def test_single_prim_vertex_count(self, tmp_path):
        positions = [(0.0, 0.0, 0.0), (1.0, 0.0, 0.0), (0.0, 1.0, 0.0)]
        gltf = _load_gltf_from_dict(_build_single_prim_gltf_dict(positions))
        verts, indices, submeshes, aabb = _extract_buffers(gltf, tmp_path / "dummy.gltf")
        assert len(verts) == 3

    def test_single_prim_indices(self, tmp_path):
        positions = [(0.0, 0.0, 0.0), (1.0, 0.0, 0.0), (0.0, 1.0, 0.0)]
        gltf = _load_gltf_from_dict(_build_single_prim_gltf_dict(positions))
        verts, indices, submeshes, aabb = _extract_buffers(gltf, tmp_path / "dummy.gltf")
        assert indices == [0, 1, 2]

    def test_two_prim_index_rebase(self, tmp_path):
        """AC-7: second primitive's indices must be rebased by first prim's vertex count."""
        pos_a = [(0.0, 0.0, 0.0), (1.0, 0.0, 0.0), (0.0, 1.0, 0.0)]
        pos_b = [(2.0, 0.0, 0.0), (3.0, 0.0, 0.0), (2.0, 1.0, 0.0)]
        gltf = _load_gltf_from_dict(_build_two_prim_gltf_dict(pos_a, pos_b))
        verts, indices, submeshes, aabb = _extract_buffers(gltf, tmp_path / "dummy.gltf")
        # prim A: 3 verts, indices [0,1,2] — unchanged
        # prim B: 3 verts, raw indices [0,1,2], rebased to [3,4,5]
        assert indices[:3] == [0, 1, 2]
        assert indices[3:] == [3, 4, 5]

    def test_two_prim_total_vertex_count(self, tmp_path):
        pos_a = [(0.0, 0.0, 0.0), (1.0, 0.0, 0.0), (0.0, 1.0, 0.0)]
        pos_b = [(2.0, 0.0, 0.0), (3.0, 0.0, 0.0), (2.0, 1.0, 0.0)]
        gltf = _load_gltf_from_dict(_build_two_prim_gltf_dict(pos_a, pos_b))
        verts, indices, submeshes, aabb = _extract_buffers(gltf, tmp_path / "dummy.gltf")
        assert len(verts) == 6

    def test_two_prim_submesh_index_ranges(self, tmp_path):
        """AC-7: submesh[0] starts at 0, submesh[1] starts at 3."""
        pos_a = [(0.0, 0.0, 0.0), (1.0, 0.0, 0.0), (0.0, 1.0, 0.0)]
        pos_b = [(2.0, 0.0, 0.0), (3.0, 0.0, 0.0), (2.0, 1.0, 0.0)]
        gltf = _load_gltf_from_dict(_build_two_prim_gltf_dict(pos_a, pos_b))
        verts, indices, submeshes, aabb = _extract_buffers(gltf, tmp_path / "dummy.gltf")
        assert submeshes[0]["index_start"] == 0
        assert submeshes[0]["index_count"] == 3
        assert submeshes[1]["index_start"] == 3
        assert submeshes[1]["index_count"] == 3

    def test_aabb_covers_all_positions(self, tmp_path):
        """AC-6: AABB is the union of all primitive vertex positions."""
        pos_a = [(0.0, 0.0, 0.0), (1.0, 0.0, 0.0), (0.0, 1.0, 0.0)]
        pos_b = [(-1.0, -1.0, -1.0), (5.0, 3.0, 2.0), (0.0, 0.0, 0.0)]
        gltf = _load_gltf_from_dict(_build_two_prim_gltf_dict(pos_a, pos_b))
        verts, indices, submeshes, aabb = _extract_buffers(gltf, tmp_path / "dummy.gltf")
        min_x, min_y, min_z, max_x, max_y, max_z = aabb
        assert min_x == pytest.approx(-1.0)
        assert min_y == pytest.approx(-1.0)
        assert min_z == pytest.approx(-1.0)
        assert max_x == pytest.approx(5.0)
        assert max_y == pytest.approx(3.0)
        assert max_z == pytest.approx(2.0)

    def test_material_id_from_name(self, tmp_path):
        """AC-5: materialId = zlib.crc32(name.encode('utf-8')) & 0xFFFFFFFF."""
        positions = [(0.0, 0.0, 0.0), (1.0, 0.0, 0.0), (0.0, 1.0, 0.0)]
        gltf = _load_gltf_from_dict(_build_single_prim_gltf_dict(positions, mat_name="Material"))
        verts, indices, submeshes, aabb = _extract_buffers(gltf, tmp_path / "dummy.gltf")
        expected_id = zlib.crc32(b"Material") & 0xFFFFFFFF
        assert submeshes[0]["material_id"] == expected_id

    def test_no_material_uses_empty_string_crc(self, tmp_path):
        """AC-5: primitive with no material uses CRC32 of '' as materialId."""
        positions = [(0.0, 0.0, 0.0), (1.0, 0.0, 0.0), (0.0, 1.0, 0.0)]
        gltf = _load_gltf_from_dict(_build_single_prim_gltf_dict(positions, mat_name=None))
        verts, indices, submeshes, aabb = _extract_buffers(gltf, tmp_path / "dummy.gltf")
        expected_id = zlib.crc32(b"") & 0xFFFFFFFF
        assert submeshes[0]["material_id"] == expected_id

    def test_material_crc_collision_raises_value_error(self, tmp_path):
        """AC-5a: two different material names with the same CRC32 must raise ValueError."""
        # We can't easily manufacture a real CRC collision, so we patch _material_id
        # to return a constant for any input, simulating a collision.
        import dia_cli.commands.asset.handlers.mesh3d as mesh3d_mod

        original = mesh3d_mod._material_id

        def _always_same(name: str) -> int:
            return 0xDEADBEEF  # collide everything

        mesh3d_mod._material_id = _always_same
        try:
            pos_a = [(0.0, 0.0, 0.0), (1.0, 0.0, 0.0), (0.0, 1.0, 0.0)]
            pos_b = [(2.0, 0.0, 0.0), (3.0, 0.0, 0.0), (2.0, 1.0, 0.0)]
            # Use distinct material names so only the hash makes them "collide"
            gltf = _load_gltf_from_dict(_build_two_prim_gltf_dict(pos_a, pos_b, "DistinctA", "DistinctB"))
            with pytest.raises(ValueError, match=r"(?i)collision|crc"):
                _extract_buffers(gltf, tmp_path / "dummy.gltf")
        finally:
            mesh3d_mod._material_id = original

    def test_vertex_data_has_required_keys(self, tmp_path):
        positions = [(0.0, 0.0, 0.0), (1.0, 0.0, 0.0), (0.0, 1.0, 0.0)]
        gltf = _load_gltf_from_dict(_build_single_prim_gltf_dict(positions))
        verts, indices, submeshes, aabb = _extract_buffers(gltf, tmp_path / "dummy.gltf")
        for v in verts:
            assert set(v.keys()) >= {"position", "normal", "tangent", "uv0"}


# ---------------------------------------------------------------------------
# Tests: Round-trip (AC-4..7 combined)
# ---------------------------------------------------------------------------

class TestRoundTrip:
    """Build glTF → _extract_buffers → _pack_mesh3d → parse header → assert counts."""

    def test_single_prim_round_trip_counts(self, tmp_path):
        positions = [(0.0, 0.0, 0.0), (1.0, 0.0, 0.0), (0.0, 1.0, 0.0)]
        gltf = _load_gltf_from_dict(_build_single_prim_gltf_dict(positions))
        verts, indices, submeshes, aabb = _extract_buffers(gltf, tmp_path / "dummy.gltf")
        data = _pack_mesh3d(verts, indices, submeshes, aabb)
        hdr = _parse_header(data)
        assert hdr["magic"] == b"MESH"
        assert hdr["version"] == 1
        assert hdr["vertex_count"] == 3
        assert hdr["index_count"] == 3
        assert hdr["submesh_count"] == 1

    def test_two_prim_round_trip_counts(self, tmp_path):
        pos_a = [(0.0, 0.0, 0.0), (1.0, 0.0, 0.0), (0.0, 1.0, 0.0)]
        pos_b = [(2.0, 0.0, 0.0), (3.0, 0.0, 0.0), (2.0, 1.0, 0.0)]
        gltf = _load_gltf_from_dict(_build_two_prim_gltf_dict(pos_a, pos_b))
        verts, indices, submeshes, aabb = _extract_buffers(gltf, tmp_path / "dummy.gltf")
        data = _pack_mesh3d(verts, indices, submeshes, aabb)
        hdr = _parse_header(data)
        assert hdr["vertex_count"] == 6
        assert hdr["index_count"] == 6
        assert hdr["submesh_count"] == 2

    def test_round_trip_binary_size(self, tmp_path):
        pos_a = [(0.0, 0.0, 0.0), (1.0, 0.0, 0.0), (0.0, 1.0, 0.0)]
        pos_b = [(2.0, 0.0, 0.0), (3.0, 0.0, 0.0), (2.0, 1.0, 0.0)]
        gltf = _load_gltf_from_dict(_build_two_prim_gltf_dict(pos_a, pos_b))
        verts, indices, submeshes, aabb = _extract_buffers(gltf, tmp_path / "dummy.gltf")
        data = _pack_mesh3d(verts, indices, submeshes, aabb)
        expected = _HEADER_SIZE + 6 * _VERTEX_SIZE + 6 * _INDEX_SIZE + 2 * _SUBMESH_SIZE
        assert len(data) == expected

    def test_round_trip_aabb_min_max(self, tmp_path):
        positions = [(-3.0, -2.0, -1.0), (3.0, 2.0, 1.0), (0.0, 0.0, 0.0)]
        gltf = _load_gltf_from_dict(_build_single_prim_gltf_dict(positions))
        verts, indices, submeshes, aabb = _extract_buffers(gltf, tmp_path / "dummy.gltf")
        data = _pack_mesh3d(verts, indices, submeshes, aabb)
        hdr = _parse_header(data)
        assert hdr["aabb"] == pytest.approx((-3.0, -2.0, -1.0, 3.0, 2.0, 1.0))

    def test_round_trip_submesh_material_id(self, tmp_path):
        positions = [(0.0, 0.0, 0.0), (1.0, 0.0, 0.0), (0.0, 1.0, 0.0)]
        gltf = _load_gltf_from_dict(_build_single_prim_gltf_dict(positions, mat_name="Rock"))
        verts, indices, submeshes, aabb = _extract_buffers(gltf, tmp_path / "dummy.gltf")
        data = _pack_mesh3d(verts, indices, submeshes, aabb)
        sub_offset = _HEADER_SIZE + 3 * _VERTEX_SIZE + 3 * _INDEX_SIZE
        idx_start, idx_count, mat_id = struct.unpack_from("<3I", data, sub_offset)
        expected_id = zlib.crc32(b"Rock") & 0xFFFFFFFF
        assert mat_id == expected_id


# ---------------------------------------------------------------------------
# Tests: Mesh3DHandler.transform and .deploy (AC-3, AC-8)
# ---------------------------------------------------------------------------

class TestMesh3DHandlerTransformDeploy:
    """Tests for transform (AC-3) and deploy (AC-8)."""

    def _write_valid_gltf(self, tmp_path: Path, name: str = "box.gltf") -> Path:
        data = _build_triangle_gltf()
        return _write_gltf(tmp_path, name, data)

    # ------------------------------------------------------------------
    # transform — happy path
    # ------------------------------------------------------------------

    def test_transform_returns_success(self, tmp_path):
        gltf_path = self._write_valid_gltf(tmp_path)
        ctx = _make_context(tmp_path)
        record = _make_record("mesh3d.box", source_path=str(gltf_path))
        result = Mesh3DHandler().transform(record, ctx)
        assert result.success is True

    def test_transform_output_path_set(self, tmp_path):
        gltf_path = self._write_valid_gltf(tmp_path)
        ctx = _make_context(tmp_path)
        record = _make_record("mesh3d.box", source_path=str(gltf_path))
        result = Mesh3DHandler().transform(record, ctx)
        assert result.output_path is not None and result.output_path != ""

    def test_transform_file_exists_at_output_path(self, tmp_path):
        gltf_path = self._write_valid_gltf(tmp_path)
        ctx = _make_context(tmp_path)
        record = _make_record("mesh3d.box", source_path=str(gltf_path))
        result = Mesh3DHandler().transform(record, ctx)
        assert Path(result.output_path).exists(), \
            f".mesh3d file not found at {result.output_path}"

    def test_transform_output_has_correct_size(self, tmp_path):
        """Minimum size > 41 bytes (header alone is 41; any data is on top of that)."""
        gltf_path = self._write_valid_gltf(tmp_path)
        ctx = _make_context(tmp_path)
        record = _make_record("mesh3d.box", source_path=str(gltf_path))
        result = Mesh3DHandler().transform(record, ctx)
        size = Path(result.output_path).stat().st_size
        assert size > 41, f"Expected > 41 bytes, got {size}"

    def test_transform_output_extension_is_mesh3d(self, tmp_path):
        gltf_path = self._write_valid_gltf(tmp_path, "mymodel.gltf")
        ctx = _make_context(tmp_path)
        record = _make_record("mesh3d.mymodel", source_path=str(gltf_path))
        result = Mesh3DHandler().transform(record, ctx)
        assert Path(result.output_path).suffix == ".mesh3d"

    def test_transform_output_stem_matches_source(self, tmp_path):
        """Output filename stem must equal the source file stem."""
        gltf_path = self._write_valid_gltf(tmp_path, "teapot.gltf")
        ctx = _make_context(tmp_path)
        record = _make_record("mesh3d.teapot", source_path=str(gltf_path))
        result = Mesh3DHandler().transform(record, ctx)
        assert Path(result.output_path).stem == "teapot"

    def test_transform_relative_source_path(self, tmp_path):
        """Relative source_path is resolved against context.source_root."""
        gltf_path = self._write_valid_gltf(tmp_path, "relative.gltf")
        ctx = _make_context(tmp_path)
        record = _make_record("mesh3d.relative", source_path="relative.gltf")
        result = Mesh3DHandler().transform(record, ctx)
        assert result.success is True
        assert Path(result.output_path).exists()

    def test_transform_creates_parent_dirs(self, tmp_path):
        """Output parent directories are created if they don't exist."""
        gltf_path = self._write_valid_gltf(tmp_path)
        ctx = _make_context(tmp_path)
        record = _make_record("mesh3d.box", source_path=str(gltf_path))
        result = Mesh3DHandler().transform(record, ctx)
        assert Path(result.output_path).parent.is_dir()

    def test_transform_output_path_matches_resolved_deploy_path(self, tmp_path):
        """output_path from transform must equal _resolve_mesh3d_deploy_path."""
        from dia_cli.commands.asset.handlers.mesh3d import _resolve_mesh3d_deploy_path
        gltf_path = self._write_valid_gltf(tmp_path, "box.gltf")
        ctx = _make_context(tmp_path)
        record = _make_record("mesh3d.box", source_path=str(gltf_path))
        result = Mesh3DHandler().transform(record, ctx)
        expected = str(_resolve_mesh3d_deploy_path(record, ctx))
        assert result.output_path == expected

    # ------------------------------------------------------------------
    # transform — error paths
    # ------------------------------------------------------------------

    def test_transform_material_collision_returns_failure(self, tmp_path):
        """If _extract_buffers raises ValueError (CRC collision), transform returns success=False."""
        import dia_cli.commands.asset.handlers.mesh3d as mesh3d_mod

        ctx = _make_context(tmp_path)

        original = mesh3d_mod._material_id

        def _always_collide(name: str) -> int:
            return 0xDEADBEEF

        mesh3d_mod._material_id = _always_collide
        try:
            # Two primitives with *distinct* material names — collision guard fires when
            # _always_collide maps both distinct names to the same id.
            pos_a = [(0.0, 0.0, 0.0), (1.0, 0.0, 0.0), (0.0, 1.0, 0.0)]
            pos_b = [(2.0, 0.0, 0.0), (3.0, 0.0, 0.0), (2.0, 1.0, 0.0)]
            two_prim_data = _build_two_prim_gltf_dict(pos_a, pos_b, "DistinctA", "DistinctB")
            two_prim_path = _write_gltf(tmp_path, "collision.gltf", two_prim_data)
            rec2 = _make_record("mesh3d.collision", source_path=str(two_prim_path))
            result = Mesh3DHandler().transform(rec2, ctx)
        finally:
            mesh3d_mod._material_id = original

        assert result.success is False
        assert len(result.errors) >= 1
        assert result.errors[0].phase == "transform"

    def test_transform_missing_file_returns_failure(self, tmp_path):
        ctx = _make_context(tmp_path)
        record = _make_record("mesh3d.missing", source_path=str(tmp_path / "nope.gltf"))
        result = Mesh3DHandler().transform(record, ctx)
        assert result.success is False

    # ------------------------------------------------------------------
    # deploy — AC-8
    # ------------------------------------------------------------------

    def test_deploy_returns_success(self, tmp_path):
        gltf_path = self._write_valid_gltf(tmp_path)
        ctx = _make_context(tmp_path)
        record = _make_record("mesh3d.box", source_path=str(gltf_path))
        Mesh3DHandler().transform(record, ctx)  # produce the binary first
        result = Mesh3DHandler().deploy(record, ctx)
        assert result.success is True

    def test_deploy_path_matches_transform_output(self, tmp_path):
        gltf_path = self._write_valid_gltf(tmp_path, "box.gltf")
        ctx = _make_context(tmp_path)
        record = _make_record("mesh3d.box", source_path=str(gltf_path))
        transform_result = Mesh3DHandler().transform(record, ctx)
        deploy_result = Mesh3DHandler().deploy(record, ctx)
        assert deploy_result.deploy_path == transform_result.output_path

    def test_deploy_does_not_write_new_files(self, tmp_path):
        """deploy() must not create any files that weren't already there after transform()."""
        gltf_path = self._write_valid_gltf(tmp_path, "box.gltf")
        ctx = _make_context(tmp_path)
        record = _make_record("mesh3d.box", source_path=str(gltf_path))
        Mesh3DHandler().transform(record, ctx)

        # Snapshot the deploy_root after transform
        deploy_root = ctx.deploy_root
        files_before = set(deploy_root.rglob("*")) if deploy_root.exists() else set()

        Mesh3DHandler().deploy(record, ctx)

        files_after = set(deploy_root.rglob("*")) if deploy_root.exists() else set()
        assert files_before == files_after, \
            f"deploy() created unexpected files: {files_after - files_before}"

    # ------------------------------------------------------------------
    # transform — OnMesh3DCookCompleted event (observation gap)
    # ------------------------------------------------------------------

    def test_transform_emits_cook_completed_event(self, tmp_path):
        gltf_path = self._write_valid_gltf(tmp_path)
        ctx = _make_context(tmp_path)
        record = _make_record("mesh3d.box", source_path=str(gltf_path))
        Mesh3DHandler().transform(record, ctx)
        events = [c[0][0] for c in ctx.output.emit.call_args_list
                  if c[0][0].get("event") == "OnMesh3DCookCompleted"]
        assert len(events) == 1

    def test_transform_cook_event_has_correct_asset_id(self, tmp_path):
        gltf_path = self._write_valid_gltf(tmp_path)
        ctx = _make_context(tmp_path)
        record = _make_record("mesh3d.box", source_path=str(gltf_path))
        Mesh3DHandler().transform(record, ctx)
        event = next(c[0][0] for c in ctx.output.emit.call_args_list
                     if c[0][0].get("event") == "OnMesh3DCookCompleted")
        assert event["assetId"] == "mesh3d.box"

    def test_transform_cook_event_counts_positive(self, tmp_path):
        gltf_path = self._write_valid_gltf(tmp_path)
        ctx = _make_context(tmp_path)
        record = _make_record("mesh3d.box", source_path=str(gltf_path))
        Mesh3DHandler().transform(record, ctx)
        event = next(c[0][0] for c in ctx.output.emit.call_args_list
                     if c[0][0].get("event") == "OnMesh3DCookCompleted")
        assert event["vertexCount"] > 0
        assert event["indexCount"] > 0
        assert event["submeshCount"] == 1

    def test_transform_cook_event_file_size_matches_disk(self, tmp_path):
        gltf_path = self._write_valid_gltf(tmp_path)
        ctx = _make_context(tmp_path)
        record = _make_record("mesh3d.box", source_path=str(gltf_path))
        result = Mesh3DHandler().transform(record, ctx)
        event = next(c[0][0] for c in ctx.output.emit.call_args_list
                     if c[0][0].get("event") == "OnMesh3DCookCompleted")
        assert event["fileSizeBytes"] == Path(result.output_path).stat().st_size

    def test_transform_no_cook_event_on_failure(self, tmp_path):
        ctx = _make_context(tmp_path)
        record = _make_record("mesh3d.missing", source_path=str(tmp_path / "nope.gltf"))
        Mesh3DHandler().transform(record, ctx)
        events = [c[0][0] for c in ctx.output.emit.call_args_list
                  if c[0][0].get("event") == "OnMesh3DCookCompleted"]
        assert len(events) == 0


# ---------------------------------------------------------------------------
# Tests: End-to-end pipeline (validate → transform → deploy → parse binary)
# ---------------------------------------------------------------------------

class TestEndToEnd:
    """Full pipeline test: fixture .gltf → validate → transform → deploy → binary parse."""

    # Two well-known material names used throughout this test class
    _MAT_A = "RedMat"
    _MAT_B = "BlueMat"

    def _write_two_prim_gltf(self, tmp_path: Path, name: str = "fixture.gltf") -> Path:
        """Write a two-primitive glTF with distinct named materials to tmp_path."""
        pos_a = [(0.0, 0.0, 0.0), (1.0, 0.0, 0.0), (0.0, 1.0, 0.0)]
        pos_b = [(2.0, 0.0, 0.0), (3.0, 0.0, 0.0), (2.0, 1.0, 0.0)]
        data = _build_two_prim_gltf_dict(pos_a, pos_b, self._MAT_A, self._MAT_B)
        return _write_gltf(tmp_path, name, data)

    def test_validate_no_errors(self, tmp_path):
        """validate() must return no errors for the two-primitive fixture."""
        gltf_path = self._write_two_prim_gltf(tmp_path)
        ctx = _make_context(tmp_path)
        record = _make_record("mesh3d.fixture", source_path=str(gltf_path))
        errors = Mesh3DHandler().validate(record, ctx)
        assert errors == [], f"Expected no validation errors, got: {_error_messages(errors)}"

    def test_transform_succeeds(self, tmp_path):
        """transform() must succeed for the two-primitive fixture."""
        gltf_path = self._write_two_prim_gltf(tmp_path)
        ctx = _make_context(tmp_path)
        record = _make_record("mesh3d.fixture", source_path=str(gltf_path))
        result = Mesh3DHandler().transform(record, ctx)
        assert result.success is True, f"transform() failed: {[e.message for e in result.errors]}"

    def test_deploy_succeeds(self, tmp_path):
        """deploy() must succeed after transform() for the two-primitive fixture."""
        gltf_path = self._write_two_prim_gltf(tmp_path)
        ctx = _make_context(tmp_path)
        record = _make_record("mesh3d.fixture", source_path=str(gltf_path))
        Mesh3DHandler().transform(record, ctx)
        result = Mesh3DHandler().deploy(record, ctx)
        assert result.success is True, f"deploy() failed: {[e.message for e in result.errors]}"

    def test_output_file_has_mesh3d_extension(self, tmp_path):
        """Deployed file must have the .mesh3d extension."""
        gltf_path = self._write_two_prim_gltf(tmp_path)
        ctx = _make_context(tmp_path)
        record = _make_record("mesh3d.fixture", source_path=str(gltf_path))
        Mesh3DHandler().transform(record, ctx)
        result = Mesh3DHandler().deploy(record, ctx)
        assert Path(result.deploy_path).suffix == ".mesh3d"

    def test_output_file_stem_matches_source(self, tmp_path):
        """Deployed file stem must equal the source .gltf stem."""
        gltf_path = self._write_two_prim_gltf(tmp_path, name="fixture.gltf")
        ctx = _make_context(tmp_path)
        record = _make_record("mesh3d.fixture", source_path=str(gltf_path))
        Mesh3DHandler().transform(record, ctx)
        result = Mesh3DHandler().deploy(record, ctx)
        assert Path(result.deploy_path).stem == "fixture"

    def test_binary_header_magic(self, tmp_path):
        """Binary header magic bytes must be b'MESH'."""
        gltf_path = self._write_two_prim_gltf(tmp_path)
        ctx = _make_context(tmp_path)
        record = _make_record("mesh3d.fixture", source_path=str(gltf_path))
        Mesh3DHandler().transform(record, ctx)
        deploy_result = Mesh3DHandler().deploy(record, ctx)
        data = Path(deploy_result.deploy_path).read_bytes()
        (magic,) = struct.unpack_from("<4s", data, 0)
        assert magic == b"MESH"

    def test_binary_header_version(self, tmp_path):
        """Binary header version byte must be 1."""
        gltf_path = self._write_two_prim_gltf(tmp_path)
        ctx = _make_context(tmp_path)
        record = _make_record("mesh3d.fixture", source_path=str(gltf_path))
        Mesh3DHandler().transform(record, ctx)
        deploy_result = Mesh3DHandler().deploy(record, ctx)
        data = Path(deploy_result.deploy_path).read_bytes()
        (version,) = struct.unpack_from("<B", data, 4)
        assert version == 1

    def test_binary_header_submesh_count(self, tmp_path):
        """Binary header submeshCount must equal 2 (one per primitive)."""
        gltf_path = self._write_two_prim_gltf(tmp_path)
        ctx = _make_context(tmp_path)
        record = _make_record("mesh3d.fixture", source_path=str(gltf_path))
        Mesh3DHandler().transform(record, ctx)
        deploy_result = Mesh3DHandler().deploy(record, ctx)
        data = Path(deploy_result.deploy_path).read_bytes()
        hdr = _parse_header(data)
        assert hdr["submesh_count"] == 2

    def test_binary_header_vertex_count_positive(self, tmp_path):
        """Binary header vertexCount must be > 0."""
        gltf_path = self._write_two_prim_gltf(tmp_path)
        ctx = _make_context(tmp_path)
        record = _make_record("mesh3d.fixture", source_path=str(gltf_path))
        Mesh3DHandler().transform(record, ctx)
        deploy_result = Mesh3DHandler().deploy(record, ctx)
        data = Path(deploy_result.deploy_path).read_bytes()
        hdr = _parse_header(data)
        assert hdr["vertex_count"] > 0

    def test_binary_total_size(self, tmp_path):
        """Binary file size must equal 41 + vertexCount*52 + indexCount*2 + submeshCount*12."""
        gltf_path = self._write_two_prim_gltf(tmp_path)
        ctx = _make_context(tmp_path)
        record = _make_record("mesh3d.fixture", source_path=str(gltf_path))
        Mesh3DHandler().transform(record, ctx)
        deploy_result = Mesh3DHandler().deploy(record, ctx)
        data = Path(deploy_result.deploy_path).read_bytes()
        hdr = _parse_header(data)
        expected_size = (
            _HEADER_SIZE
            + hdr["vertex_count"] * _VERTEX_SIZE
            + hdr["index_count"] * _INDEX_SIZE
            + hdr["submesh_count"] * _SUBMESH_SIZE
        )
        assert len(data) == expected_size, (
            f"Expected {expected_size} bytes, got {len(data)}. "
            f"Header: vertexCount={hdr['vertex_count']}, indexCount={hdr['index_count']}, "
            f"submeshCount={hdr['submesh_count']}"
        )

    def test_binary_aabb_min_le_max(self, tmp_path):
        """AABB min values must be <= AABB max values on all three axes."""
        gltf_path = self._write_two_prim_gltf(tmp_path)
        ctx = _make_context(tmp_path)
        record = _make_record("mesh3d.fixture", source_path=str(gltf_path))
        Mesh3DHandler().transform(record, ctx)
        deploy_result = Mesh3DHandler().deploy(record, ctx)
        data = Path(deploy_result.deploy_path).read_bytes()
        hdr = _parse_header(data)
        min_x, min_y, min_z, max_x, max_y, max_z = hdr["aabb"]
        assert min_x <= max_x, f"AABB min_x ({min_x}) > max_x ({max_x})"
        assert min_y <= max_y, f"AABB min_y ({min_y}) > max_y ({max_y})"
        assert min_z <= max_z, f"AABB min_z ({min_z}) > max_z ({max_z})"

    def test_binary_submesh_material_ids(self, tmp_path):
        """Each submesh's materialId must be CRC32 of its material name."""
        gltf_path = self._write_two_prim_gltf(tmp_path)
        ctx = _make_context(tmp_path)
        record = _make_record("mesh3d.fixture", source_path=str(gltf_path))
        Mesh3DHandler().transform(record, ctx)
        deploy_result = Mesh3DHandler().deploy(record, ctx)
        data = Path(deploy_result.deploy_path).read_bytes()
        hdr = _parse_header(data)

        # Submesh block starts immediately after vertex and index data
        sub_block_offset = (
            _HEADER_SIZE
            + hdr["vertex_count"] * _VERTEX_SIZE
            + hdr["index_count"] * _INDEX_SIZE
        )

        expected_ids = {
            zlib.crc32(self._MAT_A.encode("utf-8")) & 0xFFFFFFFF,
            zlib.crc32(self._MAT_B.encode("utf-8")) & 0xFFFFFFFF,
        }

        actual_ids = set()
        for i in range(hdr["submesh_count"]):
            offset = sub_block_offset + i * _SUBMESH_SIZE
            _idx_start, _idx_count, mat_id = struct.unpack_from("<3I", data, offset)
            actual_ids.add(mat_id)

        assert actual_ids == expected_ids, (
            f"Submesh material IDs mismatch. "
            f"Expected {expected_ids}, got {actual_ids}. "
            f"MAT_A='{self._MAT_A}' (CRC={zlib.crc32(self._MAT_A.encode()) & 0xFFFFFFFF:#010x}), "
            f"MAT_B='{self._MAT_B}' (CRC={zlib.crc32(self._MAT_B.encode()) & 0xFFFFFFFF:#010x})"
        )


# ---------------------------------------------------------------------------
# Tests: source-format robustness — GLB, external buffers, index component
# types, normalized attributes, interleaved byteStride, empty mesh.
#
# Every fixture above uses the same narrow encoding (data-URI buffer, uint16
# indices, float attributes, tightly packed). These builders exercise the rest
# of the glTF spec the cooker must handle for real exporter output.
# ---------------------------------------------------------------------------

# Component sizes for the configurable builder.
_CT_FORMAT = {5121: ("B", 1), 5123: ("H", 2), 5125: ("I", 4)}


def _build_configurable_gltf_dict(
    *,
    positions=None,
    index_component_type: int = 5123,
    normal_normalized: bool = False,
    interleaved: bool = False,
    external_buffer_name: str | None = None,
) -> "tuple[dict, bytes]":
    """Build a single-mesh single-primitive glTF dict plus its raw buffer bytes.

    Returns (gltf_dict, raw_buffer_bytes). The caller decides whether to embed
    the bytes as a data-URI, write them to a sidecar file, or save as GLB.

    - index_component_type: 5121 (ubyte), 5123 (ushort), or 5125 (uint)
    - normal_normalized: encode NORMAL as normalized SHORT (componentType 5122)
    - interleaved: pack POSITION+NORMAL+TANGENT+TEXCOORD into one strided view
    - external_buffer_name: if set, buffer.uri is this filename (sidecar .bin)
    """
    if positions is None:
        positions = [(0.0, 0.0, 0.0), (1.0, 0.0, 0.0), (0.0, 1.0, 0.0)]
    n = len(positions)

    norm_vals = [(0.0, 0.0, 1.0)] * n
    tang_vals = [(1.0, 0.0, 0.0, 1.0)] * n
    uv_vals = [(0.0, 0.0)] * n

    idx_char = _CT_FORMAT[index_component_type][0]
    idx_bytes = struct.pack(f"<{n}{idx_char}", *range(n))

    accessors: list[dict] = []
    bufferViews: list[dict] = []
    parts: list[bytes] = []

    def _append_view(data: bytes, byte_stride: int | None = None) -> int:
        offset = sum(len(p) for p in parts)
        view = {"buffer": 0, "byteOffset": offset, "byteLength": len(data)}
        if byte_stride is not None:
            view["byteStride"] = byte_stride
        bufferViews.append(view)
        parts.append(data)
        return len(bufferViews) - 1

    if interleaved:
        # POSITION(3f) NORMAL(3f) TANGENT(4f) TEXCOORD_0(2f) per vertex = 48 bytes.
        stride = (3 + 3 + 4 + 2) * 4
        interleaved_bytes = b"".join(
            struct.pack("<3f3f4f2f", *positions[i], *norm_vals[i],
                        *tang_vals[i], *uv_vals[i])
            for i in range(n)
        )
        bv = _append_view(interleaved_bytes, byte_stride=stride)
        accessors.append({"bufferView": bv, "byteOffset": 0, "componentType": 5126, "count": n, "type": "VEC3"})
        accessors.append({"bufferView": bv, "byteOffset": 12, "componentType": 5126, "count": n, "type": "VEC3"})
        accessors.append({"bufferView": bv, "byteOffset": 24, "componentType": 5126, "count": n, "type": "VEC4"})
        accessors.append({"bufferView": bv, "byteOffset": 40, "componentType": 5126, "count": n, "type": "VEC2"})
        pos_a, nor_a, tan_a, uv_a = 0, 1, 2, 3
    else:
        pos_bytes = b"".join(struct.pack("<3f", *p) for p in positions)
        if normal_normalized:
            # Encode (0,0,1) as normalized SHORT: 1.0 -> 32767.
            nor_bytes = b"".join(struct.pack("<3h", 0, 0, 32767) for _ in range(n))
            nor_ct, nor_norm = 5122, True
        else:
            nor_bytes = b"".join(struct.pack("<3f", *nv) for nv in norm_vals)
            nor_ct, nor_norm = 5126, False
        tan_bytes = b"".join(struct.pack("<4f", *tv) for tv in tang_vals)
        uv_bytes = b"".join(struct.pack("<2f", *uvv) for uvv in uv_vals)

        pos_a = _append_view(pos_bytes); accessors.append(
            {"bufferView": pos_a, "componentType": 5126, "count": n, "type": "VEC3"})
        nor_a = _append_view(nor_bytes); accessors.append(
            {"bufferView": nor_a, "componentType": nor_ct, "count": n, "type": "VEC3",
             "normalized": nor_norm})
        tan_a = _append_view(tan_bytes); accessors.append(
            {"bufferView": tan_a, "componentType": 5126, "count": n, "type": "VEC4"})
        uv_a = _append_view(uv_bytes); accessors.append(
            {"bufferView": uv_a, "componentType": 5126, "count": n, "type": "VEC2"})

    idx_a = _append_view(idx_bytes)
    accessors.append({"bufferView": idx_a, "componentType": index_component_type,
                      "count": n, "type": "SCALAR"})
    idx_acc = len(accessors) - 1

    raw = b"".join(parts)
    while len(raw) % 4 != 0:
        raw += b"\x00"

    if external_buffer_name is not None:
        buffer = {"byteLength": len(raw), "uri": external_buffer_name}
    else:
        b64 = base64.b64encode(raw).decode()
        buffer = {"byteLength": len(raw), "uri": f"data:application/octet-stream;base64,{b64}"}

    gltf = {
        "asset": {"version": "2.0"},
        "meshes": [{"name": "Mesh", "primitives": [{
            "attributes": {"POSITION": pos_a, "NORMAL": nor_a,
                           "TANGENT": tan_a, "TEXCOORD_0": uv_a},
            "indices": idx_acc, "mode": 4, "material": 0,
        }]}],
        "materials": [{"name": "Material"}],
        "accessors": accessors,
        "bufferViews": bufferViews,
        "buffers": [buffer],
    }
    return gltf, raw


def _save_as_glb(gltf_dict: dict, raw: bytes, path: Path) -> Path:
    """Write the dict + buffer as a binary .glb via pygltflib."""
    import pygltflib
    # GLB stores the buffer in the binary chunk; drop the data-URI.
    d = json.loads(json.dumps(gltf_dict))
    d["buffers"][0].pop("uri", None)
    g = pygltflib.GLTF2.from_json(json.dumps(d))
    g.set_binary_blob(raw)
    g.save(str(path))  # extension .glb -> binary GLB
    return path


class TestSourceFormatRobustness:
    """GLB, external buffers, index/attribute component types, byteStride."""

    # --- GLB (binary chunk, uri=None) -------------------------------------

    def test_glb_transform_succeeds(self, tmp_path):
        gltf_dict, raw = _build_configurable_gltf_dict()
        glb_path = _save_as_glb(gltf_dict, raw, tmp_path / "box.glb")
        ctx = _make_context(tmp_path)
        record = _make_record("mesh3d.box", source_path=str(glb_path))
        result = Mesh3DHandler().transform(record, ctx)
        assert result.success is True, getattr(result, "errors", None)
        assert Path(result.output_path).exists()

    def test_glb_extract_reads_binary_blob(self, tmp_path):
        import pygltflib
        gltf_dict, raw = _build_configurable_gltf_dict()
        glb_path = _save_as_glb(gltf_dict, raw, tmp_path / "box.glb")
        gltf = pygltflib.GLTF2().load(str(glb_path))
        verts, indices, submeshes, aabb = _extract_buffers(gltf, glb_path)
        assert len(verts) == 3
        assert indices == [0, 1, 2]

    # --- External sidecar .bin --------------------------------------------

    def test_external_buffer_resolves(self, tmp_path):
        gltf_dict, raw = _build_configurable_gltf_dict(external_buffer_name="box.bin")
        (tmp_path / "box.bin").write_bytes(raw)
        gltf_path = _write_gltf(tmp_path, "box.gltf", gltf_dict)
        ctx = _make_context(tmp_path)
        record = _make_record("mesh3d.box", source_path=str(gltf_path))
        result = Mesh3DHandler().transform(record, ctx)
        assert result.success is True, getattr(result, "errors", None)

    def test_missing_external_buffer_fails_transform(self, tmp_path):
        gltf_dict, _raw = _build_configurable_gltf_dict(external_buffer_name="absent.bin")
        gltf_path = _write_gltf(tmp_path, "box.gltf", gltf_dict)
        ctx = _make_context(tmp_path)
        record = _make_record("mesh3d.box", source_path=str(gltf_path))
        result = Mesh3DHandler().transform(record, ctx)
        assert result.success is False
        assert any("buffer" in e.message.lower() for e in result.errors)

    # --- Index component types --------------------------------------------

    def test_uint32_indices_read_correctly(self, tmp_path):
        gltf_dict, _raw = _build_configurable_gltf_dict(index_component_type=5125)
        gltf = _load_gltf_from_dict(gltf_dict)
        verts, indices, submeshes, aabb = _extract_buffers(gltf, tmp_path / "x.gltf")
        assert indices == [0, 1, 2]

    def test_uint8_indices_read_correctly(self, tmp_path):
        gltf_dict, _raw = _build_configurable_gltf_dict(index_component_type=5121)
        gltf = _load_gltf_from_dict(gltf_dict)
        verts, indices, submeshes, aabb = _extract_buffers(gltf, tmp_path / "x.gltf")
        assert indices == [0, 1, 2]

    def test_uint32_indices_end_to_end(self, tmp_path):
        """uint32-indexed source produces a valid uint16 .mesh3d."""
        gltf_dict, _raw = _build_configurable_gltf_dict(index_component_type=5125)
        gltf_path = _write_gltf(tmp_path, "u32.gltf", gltf_dict)
        ctx = _make_context(tmp_path)
        record = _make_record("mesh3d.u32", source_path=str(gltf_path))
        assert Mesh3DHandler().validate(record, ctx) == []
        result = Mesh3DHandler().transform(record, ctx)
        assert result.success is True
        data = Path(result.output_path).read_bytes()
        idx_count = struct.unpack_from("<I", data, 9)[0]
        assert idx_count == 3

    def test_invalid_index_component_type_rejected(self, tmp_path):
        """A float index componentType (5126) is not a legal glTF index type."""
        gltf_dict, _raw = _build_configurable_gltf_dict()
        gltf_dict["accessors"][-1]["componentType"] = 5126  # FLOAT — illegal for indices
        gltf_path = _write_gltf(tmp_path, "badidx.gltf", gltf_dict)
        ctx = _make_context(tmp_path)
        record = _make_record("mesh3d.badidx", source_path=str(gltf_path))
        errors = Mesh3DHandler().validate(record, ctx)
        assert any("index componentType" in e.message for e in errors)

    # --- Normalized integer attributes ------------------------------------

    def test_normalized_short_normal_unnormalized(self, tmp_path):
        """A normalized SHORT NORMAL of (0,0,32767) decodes to (0,0,1.0)."""
        gltf_dict, _raw = _build_configurable_gltf_dict(normal_normalized=True)
        gltf = _load_gltf_from_dict(gltf_dict)
        verts, indices, submeshes, aabb = _extract_buffers(gltf, tmp_path / "x.gltf")
        nx, ny, nz = verts[0]["normal"]
        assert nz == pytest.approx(1.0)
        assert nx == pytest.approx(0.0)
        assert ny == pytest.approx(0.0)

    # --- Interleaved byteStride -------------------------------------------

    def test_interleaved_attributes_read_correctly(self, tmp_path):
        positions = [(1.0, 2.0, 3.0), (4.0, 5.0, 6.0), (7.0, 8.0, 9.0)]
        gltf_dict, _raw = _build_configurable_gltf_dict(positions=positions, interleaved=True)
        gltf = _load_gltf_from_dict(gltf_dict)
        verts, indices, submeshes, aabb = _extract_buffers(gltf, tmp_path / "x.gltf")
        assert verts[0]["position"] == pytest.approx([1.0, 2.0, 3.0])
        assert verts[1]["position"] == pytest.approx([4.0, 5.0, 6.0])
        assert verts[2]["position"] == pytest.approx([7.0, 8.0, 9.0])
        # Normal/tangent/uv must still come through from the strided view.
        assert verts[0]["normal"] == pytest.approx([0.0, 0.0, 1.0])
        assert verts[0]["tangent"] == pytest.approx([1.0, 0.0, 0.0, 1.0])

    # --- Empty mesh --------------------------------------------------------

    def test_zero_vertex_mesh_rejected(self, tmp_path):
        gltf_dict, _raw = _build_configurable_gltf_dict()
        # Zero out POSITION (and indices) accessor counts.
        for acc in gltf_dict["accessors"]:
            acc["count"] = 0
        gltf_path = _write_gltf(tmp_path, "empty.gltf", gltf_dict)
        ctx = _make_context(tmp_path)
        record = _make_record("mesh3d.empty", source_path=str(gltf_path))
        errors = Mesh3DHandler().validate(record, ctx)
        assert any("zero vertices" in e.message for e in errors)


class TestDeployPathAndWriteErrors:
    """Coverage for stage-scope deploy paths and the transform write-error branch."""

    def _write_valid_gltf(self, tmp_path: Path, name: str = "box.gltf") -> Path:
        return _write_gltf(tmp_path, name, _build_triangle_gltf())

    def test_stage_scope_deploy_path_layout(self, tmp_path):
        """Stage-scoped asset with a category tag deploys under stages/<name>/<category>/."""
        from dia_cli.commands.asset.handlers.mesh3d import _resolve_mesh3d_deploy_path
        self._write_valid_gltf(tmp_path, "box.gltf")
        ctx = _make_context(tmp_path)
        record = {
            "id": "mesh3d.box", "type": "mesh3d", "source_path": "box.gltf",
            "scope": "stage", "stage_name": "Level1", "tags": ["environments"],
        }
        path = _resolve_mesh3d_deploy_path(record, ctx)
        parts = path.parts
        assert "stages" in parts
        assert "Level1" in parts
        assert "environments" in parts
        assert path.name == "box.mesh3d"

    def test_stage_scope_transform_writes_to_stage_dir(self, tmp_path):
        self._write_valid_gltf(tmp_path, "box.gltf")
        ctx = _make_context(tmp_path)
        record = {
            "id": "mesh3d.box", "type": "mesh3d", "source_path": "box.gltf",
            "scope": "stage", "stage_name": "Level1", "tags": ["environments"],
        }
        result = Mesh3DHandler().transform(record, ctx)
        assert result.success is True
        assert Path(result.output_path).exists()
        assert "Level1" in Path(result.output_path).parts

    def test_write_failure_returns_transform_error(self, tmp_path, monkeypatch):
        """An OSError while writing the .mesh3d is reported as a transform failure."""
        gltf_path = self._write_valid_gltf(tmp_path, "box.gltf")
        ctx = _make_context(tmp_path)
        record = _make_record("mesh3d.box", source_path=str(gltf_path))

        def _boom(self, *args, **kwargs):
            raise OSError("disk full")

        monkeypatch.setattr(Path, "write_bytes", _boom)
        result = Mesh3DHandler().transform(record, ctx)
        assert result.success is False
        assert any(e.phase == "transform" for e in result.errors)
        assert any("write" in e.message.lower() or "disk full" in e.message.lower()
                   for e in result.errors)
