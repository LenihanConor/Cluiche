**Spec:** @docs/specs/applications/dia/systems/diamesh3dvisualdebugger/mesh3d-stats.md
**Status:** Done

---

## Implementation Patterns

- **MeshStatsDrawer** follows exact pattern of `MeshBoundsDrawer`/`MeshOriginDrawer`: `#ifdef DIA_DEBUG` guards entire file, `Dia::Mesh3D::` namespace, const refs at construction.
- **Draw()** is a strict no-op — no `static_cast`, no primitives emitted, empty body.
- **DrawImGui()** computes all stats inline from live refs each call; no caching. Uses `ImGui::TextDisabled()` for headers, `ImGui::TextColored()` for dropped count when > 0.
- **Per-layer bucketing**: local `int layerCounts[32]` + `int16_t seenLayers[32]`, iterate draws, accumulate; render only non-zero entries.
- **Asset state dedup**: local `StringCRC seen[256]`, skip duplicate meshIds before calling `LookupMesh()`.
- **vcxproj**: add via `dia docs vcxproj-add DiaMesh3DVisualDebugger` or manual edit.
- **Tests**: only 3 structural tests (DrawImGui not testable without ImGui context).

---

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Implement `MeshStatsDrawer.h/.cpp`; add to `DiaMesh3DVisualDebugger.vcxproj` | Build passes | Done | sonnet | MeshStatsDrawer.h/.cpp created; Draw() is strict no-op; DrawImGui() has all 4 stat sections |
| 2 | Write `TestMeshStatsDrawer.cpp`; add to `GoogleTests.vcxproj` | `dia run googletest --filter="MeshStatsDrawerTest*"` all pass | Done | sonnet | 3/3 pass; fix: test had Dia::Debug:: namespace instead of Dia::Mesh3D:: |
