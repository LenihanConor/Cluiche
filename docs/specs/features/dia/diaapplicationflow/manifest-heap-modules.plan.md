# Plan: Manifest Heap Modules

**Spec:** @docs/specs/features/dia/diaapplicationflow/manifest-heap-modules.md  
**Status:** In Progress

## Implementation Patterns

### Field change (`ApplicationManifestV3.h`)
Replace:
```cpp
Dia::Core::Containers::DynamicArrayC<ModuleDeclaration, 32> modules;
```
With:
```cpp
Dia::Core::Containers::DynamicArray<ModuleDeclaration> modules;
```
Include `<DiaCore/Containers/Arrays/DynamicArray.h>` and remove the `DynamicArrayC` include if it's no longer used in this header.

### Loader pattern (`ApplicationManifestLoaderV2.cpp`)
Before the module-parsing loop, reserve exactly N slots:
```cpp
const Json::Value& modulesJson = puJson["modules"];
pu.modules.Reserve(modulesJson.size());
// ... loop calling pu.modules.Add(mod) as before
```
The loop body (`ModuleDeclaration mod; ... pu.modules.Add(mod);`) is unchanged.

### Copy-semantics safety check
`DynamicArray<T>` has a proper deep-copy constructor. However, `ProcessingUnitDeclaration` is stored inside `DynamicArrayC<ProcessingUnitDeclaration, 4>` (in `ApplicationManifestV3.processingUnits`). Before marking Task 3 done, verify that `DynamicArrayC` calls element copy constructors (not memcpy) when copying/adding elements — otherwise the heap pointer inside `modules` will be shallow-copied.

Known value-copy call sites to check:
- `PUCommands.cpp:51` — `mSavedPU = pus[i];` (undo save)
- `PUCommands.cpp:170,193` — `ProcessingUnitDeclaration saved = pus[i];` (reorder undo)
- `ManifestComposerV2.cpp:227` — `ApplicationManifestV3 stageManifest;` local + merge loop that calls `pu.modules.Add(stagePU.modules[mi])`
- `ApplicationManifestLoaderV2.cpp:165` — `ProcessingUnitDeclaration pu;` (local, safe)

### sizeof verification
Add a comment after the struct closing brace in `ApplicationManifestV3.h`:
```cpp
// sizeof(ApplicationManifestV3) is now O(100s of bytes) — modules array is heap-allocated
// via DynamicArray<ModuleDeclaration>. Previously ~60 KB due to DynamicArrayC<ModuleDeclaration,32>.
```
Or a `static_assert` if a concrete bound is appropriate.

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Change `ProcessingUnitDeclaration.modules` field type in `ApplicationManifestV3.h`; update includes | Compiles | - | haiku | Single line change + include swap |
| 2 | Update `ApplicationManifestLoaderV2.cpp`: add `Reserve(modulesJson.size())` before module-parse loop | Loader tests pass | - | haiku | Loop body unchanged |
| 3 | Audit `PUCommands.cpp` and `ManifestComposerV2.cpp` value-copy call sites; verify `DynamicArrayC` copy-constructs elements correctly; fix any shallow-copy issues | Manual inspect + build | - | sonnet | Risk: DynamicArrayC may use memcpy |
| 4 | Add sizeof comment to `ApplicationManifestV3.h` | n/a | - | haiku | Documents the improvement |
| 5 | `dia run googletest --filter="ManifestLoader*"` — confirm all pass | All green | - | haiku | Report pass/fail + any failures |
