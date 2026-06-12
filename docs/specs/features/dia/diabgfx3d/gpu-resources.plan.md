**Spec:** @docs/specs/features/dia/diabgfx3d/gpu-resources.md
**Status:** Done

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | `Resources/MaterialRegistry.h/.cpp` — Register/Resolve/GetDefault, DynamicArrayC backing, default descriptor | Unit tests (task 3) | Done | sonnet | Already implemented in prior canvas3d scaffold |
| 2 | `Resources/MeshGpuCache.h/.cpp` — GetOrUpload (lazy upload when Ready, nullptr if not), DestroyAll, internal unordered_map | Unit tests (task 3) | Done | sonnet | Pimpl keeps STL out of header; bgfx types in .cpp only; StringCRC.Value() fixed from GetCRC() |
| 3 | GoogleTests: `DiaBgfx3D_MaterialRegistryTest`, `DiaBgfx3D_MeshGpuCacheTest` — add to GoogleTests.vcxproj | `dia run googletest --filter="DiaBgfx3D*"` green | Done | sonnet | 9 tests passing |
