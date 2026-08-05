# Plan: DiaEntitySpatial

**Spec:** @docs/specs/applications/dia/systems/diaentityspatial/diaentityspatial.md  
**Status:** In Progress

---

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Create `DiaEntitySpatial` static library: vcxproj, filters, module doc, Cluiche.sln entry | Build passes | Pending | sonnet | New module under `Dia/DiaEntitySpatial/` |
| 2 | Implement `SpatialComponent` (header + cpp): `Vec2 position`, `float radius`, `uint32_t mLayerMask`, DIA_COMPONENT/FIELD macros, dirty-flag assert | Reflection + dirty-flag assert tests | Pending | sonnet | Namespace `Dia::EntitySpatial::` |
| 3 | Implement `EntitySpatialIndex` (header): wraps `ISpatialStructure<Entity>`, topology enum (SquareGrid/HexGrid), owns the concrete structure | Compiles clean | Pending | sonnet | Template storage inside concrete types; header-only OK |
| 4 | Implement `EntitySpatialModule` (header + cpp): `Update()` with dirty sweep + detach/destroy sweep; five query methods | Build + domain update tests | Pending | sonnet | Owns `mSpatialHandles` + `mResolveBuffer` |
| 5 | Add test utilities to `Dia/DiaEntitySpatial/Testing/SpatialTestHelpers.h` | Helpers compile | Pending | haiku | Factory to populate domain with N positioned entities |
| 6 | Write Google Tests for DiaEntitySpatial in `GoogleTests/DiaEntitySpatial/` and wire into GoogleTests.vcxproj + link `DiaEntitySpatial.lib` | All tests pass | Pending | sonnet | Covers all 5 queries, mask, dirty update, detach, hex vs square, stale handle |
| 7 | `dia docs registry` + update spec features to Done + commit | Spec + registry updated | Pending | haiku | Final bookkeeping |
