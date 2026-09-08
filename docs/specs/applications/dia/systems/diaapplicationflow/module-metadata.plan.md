# Plan: Module Metadata

**Spec:** @docs/specs/applications/dia/systems/diaapplicationflow/module-metadata.md
**Status:** Done

## Session Notes

DiaApplicationFlow modules currently have no type-level metadata beyond `kTypeId`. This plan adds `kAllowedPUs` (PUAffinity bitmask) and `kDescription` (const char*) to each module type. ProcessingUnit validates affinity at AddModule time. TypeRegistry stores metadata for editor/tooling queries.

Key constraints from spec chain:
- Config is sole source of truth for structural wiring (DiaApplicationFlow) — but affinity is a code-level constraint, not config
- One registration macro per module — extend DIA_MODULE, don't replace
- Zero overhead in Release for debug features — assert is debug-only

Design decision: `Module` base class gains `StringCRC mTypeId` + `GetTypeId() const`. Application sets it via `SetTypeId()` during wiring (same path as `SetProcessingUnit()`). This is how `AddModule()` obtains the type ID for affinity lookup. Also fills `ModuleStateInfo::typeId` gap in `IApplicationInspectable`.

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Create PUAffinity.h | Compile | Done | sonnet | New header in DiaApplicationFlow |
| 2 | Extend TypeRegistry with metadata storage | Unit test | Done | sonnet | Add fields + queries |
| 3 | Update DIA_MODULE macro to capture metadata | Compile | Done | sonnet | SFINAE detection of kAllowedPUs/kDescription |
| 4 | Add PU identity to ProcessingUnit | Unit test | Done | sonnet | Map instance_id to affinity |
| 5 | Add affinity check in AddModule | Unit test | Done | sonnet | Assert on mismatch |
| 6 | Extend IApplicationInspectable | Compile | Done | haiku | Add fields to ModuleStateInfo |
| 7 | Annotate CluicheGameBaseline modules | Compile | Done | sonnet | ~12 modules |
| 8 | Annotate CluicheTest modules | Compile | Done | sonnet | ~8 modules |
| 9 | GoogleTest coverage | dia run googletest | Done | sonnet | 14 tests pass; 3 suites: HasAffinityTest, TypeRegistryMetadata, ProcessingUnitAffinity |

## Task Details

### Task 1: Create PUAffinity.h

**File:** `Dia/DiaApplicationFlow/PUAffinity.h`

Create a new header with:
```cpp
#pragma once
#include <cstdint>

namespace Dia::ApplicationFlow {

enum class PUAffinity : uint8_t
{
    kNone   = 0,
    kMain   = 1 << 0,
    kSim    = 1 << 1,
    kRender = 1 << 2,
    kAny    = 0xFF
};

inline constexpr PUAffinity operator|(PUAffinity a, PUAffinity b)
{
    return static_cast<PUAffinity>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}

inline constexpr PUAffinity operator&(PUAffinity a, PUAffinity b)
{
    return static_cast<PUAffinity>(static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
}

inline constexpr bool HasAffinity(PUAffinity mask, PUAffinity flag)
{
    return (mask & flag) != PUAffinity::kNone;
}

} // namespace Dia::ApplicationFlow
```

Add to `DiaApplicationFlow.vcxproj` + `.vcxproj.filters`.

---

### Task 2: Extend TypeRegistry with metadata storage

**File:** `Dia/DiaApplicationFlow/TypeRegistry.h` (modify)

The existing TypeRegistry stores a factory function per type. Extend the entry to also store:
- `PUAffinity allowedPUs = PUAffinity::kAny`
- `const char* description = nullptr`

Add query methods:
```cpp
PUAffinity GetAllowedPUs(const StringCRC& typeId) const;
const char* GetDescription(const StringCRC& typeId) const;
```

The `ModuleRegistration<T>` constructor should detect if `T::kAllowedPUs` and `T::kDescription` exist (via SFINAE or `if constexpr` with a concept/trait) and pass them to the registry.

---

### Task 3: Update DIA_MODULE macro to capture metadata

**File:** `Dia/DiaApplicationFlow/RegistrationMacrosV2.h` (modify)

The `ModuleRegistration<T>` template already captures `T::kTypeId`. Extend its constructor to also capture `T::kAllowedPUs` and `T::kDescription` if they exist as static constexpr members. Use SFINAE:

```cpp
template<typename T, typename = void>
struct ModuleMetadataExtractor {
    static constexpr PUAffinity GetAllowedPUs() { return PUAffinity::kAny; }
    static constexpr const char* GetDescription() { return nullptr; }
};

template<typename T>
struct ModuleMetadataExtractor<T, std::void_t<decltype(T::kAllowedPUs)>> {
    static constexpr PUAffinity GetAllowedPUs() { return T::kAllowedPUs; }
    static constexpr const char* GetDescription() { ... }
};
```

This ensures backwards compatibility — modules without these members compile unchanged.

---

### Task 4: Add PU identity to ProcessingUnit

**File:** `Dia/DiaApplicationFlow/ProcessingUnit.h` (modify)

Add a `PUAffinity mAffinity` member. Set it during construction based on `instance_id`:
- `"MainPU"` → `PUAffinity::kMain`
- `"SimPU"` → `PUAffinity::kSim`
- `"RenderPU"` → `PUAffinity::kRender`
- Unknown → `PUAffinity::kAny`

Add `PUAffinity GetAffinity() const { return mAffinity; }`.

The mapping lives in a small lookup table in the .cpp file. If the manifest gains an explicit `"affinity"` field later, it can override.

---

### Task 5: Add affinity check in AddModule

**File:** `Dia/DiaApplicationFlow/ProcessingUnit.cpp` (modify)

In `ProcessingUnit::AddModule(Module* module)`:
```cpp
PUAffinity moduleAffinity = TypeRegistry::Instance().GetAllowedPUs(module->GetTypeId());
if (!HasAffinity(moduleAffinity, mAffinity))
{
    DIA_ASSERT(false, "Module %s not allowed on PU %s (allowed: %u, PU: %u)",
        module->GetInstanceId().GetString(), GetInstanceId().GetString(),
        static_cast<uint8_t>(moduleAffinity), static_cast<uint8_t>(mAffinity));
    DIA_LOG_ERROR("ApplicationFlow", "Module %s placed on wrong PU %s",
        module->GetInstanceId().GetString(), GetInstanceId().GetString());
}
```

Note: Module needs a `GetTypeId()` method or the type info needs to be passed to AddModule. Check current AddModule signature — it may need the TypeRegistry lookup or an additional parameter. The registration already knows the type ID; it may be stored on the Module instance.

---

### Task 6: Extend IApplicationInspectable

**File:** `Dia/DiaApplicationFlow/IApplicationInspectable.h` (modify)

Add to `ModuleStateInfo`:
```cpp
PUAffinity allowedPUs = PUAffinity::kAny;
const char* description = nullptr;
```

Update `GetActiveModules()` implementation to fill these from TypeRegistry.

---

### Task 7: Annotate CluicheGameBaseline modules

For each module in `Cluiche/CluicheGameBaseline/Modules/`, add:
```cpp
static constexpr PUAffinity kAllowedPUs = PUAffinity::kMain; // or kSim, kRender, kAny
```

Known modules and their correct affinity:
- KernelModule → kMain (owns window, creates render context)
- UIModule → kMain
- AssetServiceModule → kMain (or kAny if it runs everywhere)
- ObservationModule → kAny (runs on all PUs)
- ProfilerModule → kAny
- JobSystemModule → kMain
- DebugServerHostModule → kMain
- AutomationModule → kMain
- MainStateProducerModule → kMain
- TimeServerModule → kSim
- InputStreamModule → kSim
- LoadingScreenModule → kSim
- VisualDebuggerModule → kSim
- VisualDebuggerConsoleModule → kRender
- DebugUIModule → kRender
- BootMenuModule → kRender
- RenderModule → kRender
- AssetRuntimeHUDModule → kRender
- AssetRuntimeVisualDebuggerModule → kRender
- TestStageHUDModule → kRender

Verify by reading each module's manifest declaration in cluiche_main.diaapp to confirm which PU it's on.

---

### Task 8: Annotate CluicheTest modules

For each module in `Cluiche/CluicheTest/Modules/TestStages/`:
```cpp
static constexpr PUAffinity kAllowedPUs = PUAffinity::kMain;
static constexpr const char* kDescription = "...";
```

All test stage modules are MainPU. Descriptions should be one-line summaries of what the stage tests.

---

### Task 9: GoogleTest coverage

**File:** `Cluiche/Tests/GoogleTests/ApplicationFlow/TestPUAffinity.cpp` (new)

Test cases:
1. Module with kMain on MainPU → no assert
2. Module with kMain on SimPU → assert fires (use death test or expect log error)
3. Module with kAny on any PU → no assert
4. Module without kAllowedPUs declared → treated as kAny, no assert
5. TypeRegistry returns correct affinity and description for registered type
