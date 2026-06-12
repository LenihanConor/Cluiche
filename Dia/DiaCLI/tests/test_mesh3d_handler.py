"""Unit tests for Mesh3DHandler.validate (AC-2).

Fixtures build minimal glTF files programmatically via pygltflib so the tests
are self-contained and do not depend on checked-in binary blobs.

SD-APIPE-002: validate must collect ALL errors before returning.
"""
from __future__ import annotations

import json
import struct
import base64
from pathlib import Path
from unittest.mock import MagicMock

import pytest

from dia_cli.commands.asset.context import BuildContext
from dia_cli.commands.asset.handler import AssetError
from dia_cli.commands.asset.handlers.mesh3d import Mesh3DHandler


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
