**Spec:** @docs/specs/applications/dia/systems/diaassetpipeline/gltf-mesh-handler.md
**Status:** Done

## Implementation Patterns

### Handler class
- `Mesh3DHandler(AssetHandler)` in `dia_cli/commands/asset/handlers/mesh3d.py`
- `type_id = "mesh3d"` class attribute
- `validate` returns `list[AssetError]` — collects all, no early exit (SD-APIPE-002)
- `transform` writes `.mesh3d` to deploy path (computed by `_resolve_mesh3d_deploy_path`), returns `TransformResult(success=True, output_path=str(deploy_path))`
- `deploy` returns `DeployResult(success=True, deploy_path=result.output_path)` — no-op, file already placed

### Path resolution
- `_resolve_mesh3d_deploy_path(record, context)` calls `resolve_deploy_path` from `layout.py` then replaces `.name` with `Path(source_path).stem + ".mesh3d"`

### Binary packing
- `struct.pack("<4sB3I6f", b"MESH", 1, vertex_count, index_count, submesh_count, min_x, min_y, min_z, max_x, max_y, max_z)` — 41-byte header
- `struct.pack("<3f3f4f2fI", ...)` per vertex — 52 bytes
- `struct.pack(f"<{n}H", *indices)` for index buffer
- `struct.pack("<3I", index_start, index_count, mat_id)` per submesh

### CRC / material ID
- `zlib.crc32(name.encode("utf-8")) & 0xFFFFFFFF` — byte-identical to `Dia::Core::StringCRC`
- Collision guard: track `{mat_id: name}` dict; error if same ID maps to two different names

### pygltflib env wiring
- `External/Python311/requirements.txt` — one package per line
- New `python-packages` step in `setup_orchestrator.py`: `pip install --target <Python311/Lib/site-packages> -r requirements.txt`

### Test patterns
- Follow `test_built_in_handlers.py`: `_make_context(tmp_path)`, `_make_record(...)` helpers, `pytest` with `tmp_path` fixture
- Fixture glTF: minimal valid JSON glTF with one mesh, one triangle primitive, POSITION/NORMAL/TANGENT/TEXCOORD_0

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Add `pygltflib` to `External/Python311/requirements.txt` and wire python-packages install step into `dia env setup` orchestrator | `dia env verify` reports pygltflib present | Done | sonnet | requirements.txt lives at Dia/DiaCLI/requirements.txt (External/Python311 is gitignored) |
| 2 | Implement `Mesh3DHandler.validate` — source exists, single mesh, triangle-only, required attributes, skinned rejected, limit checks; collect all errors | Unit: each failure mode → expected `AssetError`; valid glTF → `[]` | Done | sonnet | 22 tests green; fixtures built programmatically in tests via pygltflib |
| 3 | Implement glTF→buffers extraction + binary packer — concatenate primitive vertices, rebase indices, build submeshes, compute AABB, `struct.pack` 41-byte header + payload; `_material_id` via `zlib.crc32`; collision guard | Unit: packed bytes round-trip; CRC parity vs known values | Done | sonnet | 50 tests green; _extract_buffers + _pack_mesh3d |
| 4 | Implement `transform` + no-op `deploy`; `_resolve_mesh3d_deploy_path` extension override | Unit: transform writes to expected path; deploy returns it | Done | sonnet | 64 tests green |
| 5 | Register `Mesh3DHandler` in `handlers/__init__.py` | Unit: registry resolves `mesh3d` to `Mesh3DHandler` | Done | haiku | 98 tests green |
| 6 | End-to-end test: fixture `.gltf` through validate→transform→deploy yields parseable `.mesh3d` | Integration: assert header counts + AABB + submesh materialIds | Done | sonnet | 76 tests green |
