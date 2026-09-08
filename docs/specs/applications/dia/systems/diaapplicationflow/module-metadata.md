# Feature Spec: Module Metadata

## Traceability

| Level | Spec | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia/dia.md |
| System | DiaApplicationFlow | @docs/specs/applications/dia/systems/diaapplicationflow/diaapplicationflow.md |
| Feature | Module Metadata | (this document) |

## Summary

Add type-level metadata to DiaApplicationFlow modules: a PU affinity bitmask that declares which ProcessingUnits a module type is allowed to run on, and a human-readable description string for editor display. The framework validates affinity at module creation time (runtime assert in debug, logged warning in release). The editor reads both fields for display in the module inspector.

## Problem

Nothing prevents a human or AI from placing a module on the wrong PU in a manifest. AutomationModule on SimPU silently breaks (it depends on MainPU-only services). Physics2DModule on RenderPU makes no architectural sense. These errors are only discovered at runtime through subtle bugs, not at load/build time. Additionally, modules have no description field, making it hard for editors and tooling to present what a module does.

## Acceptance Criteria

1. A `PUAffinity` enum (bitmask) exists in `DiaApplicationFlow` with flags: `kMain`, `kSim`, `kRender`, `kAny`.
2. Module types can declare `static constexpr PUAffinity kAllowedPUs = PUAffinity::kMain;` (or any combination).
3. Module types can declare `static constexpr const char* kDescription = "...";` for editor/tooling display.
4. `ProcessingUnit::AddModule()` checks the module type's `kAllowedPUs` against the PU's own affinity. On mismatch: `DIA_ASSERT` in debug, `DIA_LOG_ERROR` in release.
5. The `DIA_MODULE` registration macro (or a companion macro) captures `kAllowedPUs` and `kDescription` into the `TypeRegistry` so they're available without instantiating the module.
6. `TypeRegistry` exposes `GetAllowedPUs(typeId)` and `GetDescription(typeId)` queries.
7. All existing CluicheTest and CluicheGameBaseline modules are annotated with correct affinity values.
8. Modules that do not declare `kAllowedPUs` default to `PUAffinity::kAny` (backwards compatible).
9. Modules that do not declare `kDescription` default to `nullptr` (backwards compatible).
10. The `IApplicationInspectable` interface exposes module affinity and description in `ModuleStateInfo`.

## Non-Goals

- Editor UI rendering of the metadata (that's DiaApplicationFlowEditor's concern)
- Manifest-level validation at pipeline/build time (future — this spec only covers runtime)
- Compile-time enforcement (would require manifest parsing at compile time, not feasible)

## Tasks

| # | Task | Scope |
|---|------|-------|
| 1 | Create `PUAffinity.h` with enum and bitwise operators | DiaApplicationFlow |
| 2 | Extend `TypeRegistry` to store affinity + description per type | DiaApplicationFlow |
| 3 | Update `DIA_MODULE` macro (or add `DIA_MODULE_META`) to capture metadata | DiaApplicationFlow |
| 4 | Add affinity check in `ProcessingUnit::AddModule()` | DiaApplicationFlow |
| 5 | Assign PU identity to each ProcessingUnit instance (from manifest `instance_id` or explicit tag) | DiaApplicationFlow |
| 6 | Extend `ModuleStateInfo` in `IApplicationInspectable` with affinity + description | DiaApplicationFlow |
| 7 | Annotate all CluicheGameBaseline modules with `kAllowedPUs` | CluicheGameBaseline |
| 8 | Annotate all CluicheTest modules with `kAllowedPUs` + `kDescription` | CluicheTest |
| 9 | Add GoogleTest coverage for affinity mismatch assertion | GoogleTests |

## Design Notes

### PUAffinity enum

```cpp
// Dia/DiaApplicationFlow/PUAffinity.h
namespace Dia::ApplicationFlow {

enum class PUAffinity : uint8_t
{
    kNone   = 0,
    kMain   = 1 << 0,
    kSim    = 1 << 1,
    kRender = 1 << 2,
    kAny    = 0xFF
};

inline constexpr PUAffinity operator|(PUAffinity a, PUAffinity b) { return static_cast<PUAffinity>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b)); }
inline constexpr PUAffinity operator&(PUAffinity a, PUAffinity b) { return static_cast<PUAffinity>(static_cast<uint8_t>(a) & static_cast<uint8_t>(b)); }
inline constexpr bool HasAffinity(PUAffinity mask, PUAffinity flag) { return (mask & flag) != PUAffinity::kNone; }

} // namespace Dia::ApplicationFlow
```

### PU identity assignment

Each ProcessingUnit needs to know its own affinity. Options:
- Derive from `instance_id`: "MainPU" → kMain, "SimPU" → kSim, "RenderPU" → kRender (convention-based)
- Add `"affinity"` field to manifest ProcessingUnit declaration (explicit)
- Recommend: convention-based with manifest override. The framework maps known PU names to affinity flags. Unknown names default to kAny unless manifest specifies.

### Type ID surfacing in AddModule

`ProcessingUnit::AddModule()` needs the module's type ID to query `TypeRegistry::GetAllowedPUs()`. The recommended approach is to store `mTypeId` on the Module base class:

- `Module` gains a `StringCRC mTypeId` member and `GetTypeId() const`
- Application sets it via `SetTypeId(typeId)` during wiring (same path as `SetProcessingUnit`)
- `ModuleRegistration<T>` already knows `T::kTypeId` at static init — passes it through
- Bonus: fills the `typeId` field in `ModuleStateInfo` (which currently has no source)

`AddModule` then calls `TypeRegistry::GetAllowedPUs(module->GetTypeId())` directly.

### Backwards compatibility

Modules without `kAllowedPUs` compile and run unchanged. The TypeRegistry stores `kAny` as default. No existing code breaks.

## Binding Decisions (from parent specs)

| # | Decision | Source | How Honored |
|---|----------|--------|-------------|
| SD-001 | Config is sole source of truth for structural wiring | DiaApplicationFlow | Affinity is type-level metadata in code, not config. Config declares placement; code declares constraints. Runtime validates the combination. No config schema change required. |
| SD-008 | One-liner DIA_MODULE macro | DiaApplicationFlow | `DIA_MODULE` is extended via `ModuleRegistration<T>` SFINAE to capture `kAllowedPUs`/`kDescription` if present. Macro call sites are unchanged. |
| SD-016 | IApplicationInspectable exposes runtime state | DiaApplicationFlow | `ModuleStateInfo` gains `allowedPUs` and `description` fields. Editor/test consumers can query without coupling to internals. |
| PD-001 | Use StringCRC for all entity/component IDs | Platform | `kAllowedPUs` is a bitmask (uint8_t), not a StringCRC — that is correct; it is not an identity. `kDescription` is `const char*` — also not an identity. No violation. |
| PD-004 | No STL containers in public APIs | Platform | `PUAffinity.h` adds no containers. `TypeRegistry` extension uses existing DiaCore `DynamicArrayC`. No STL in any new public API surface. |
| PD-007 | C++20 required | Platform | SFINAE detection of `kAllowedPUs`/`kDescription` uses `std::void_t` (C++17) or `if constexpr` (C++17). Both are valid under C++20. |

## AI Review Questions

1. **Q: Should affinity be enforced at manifest load time (preventing Application::Start) or only at AddModule time?** A: At AddModule time (which is called from Application::Start). This means the error surfaces at the earliest possible runtime moment. Future work could add CLI/pipeline validation that checks manifests against compiled metadata.

2. **Q: How does the PU know its own affinity if the manifest just says "MainPU"?** A: Convention-based: Application maps instance_id to affinity using a lookup table (MainPU→kMain, SimPU→kSim, RenderPU→kRender). Unknown PU names get kAny. Manifest can override with an explicit "affinity" field if needed.

3. **Q: What if a module legitimately runs on multiple PUs (e.g. ObservationModule on all)?** A: `kAllowedPUs = PUAffinity::kAny` or `PUAffinity::kMain | PUAffinity::kSim | PUAffinity::kRender`. The bitmask supports any combination.

4. **Q: How does AddModule() obtain the module's type ID to query TypeRegistry?** A: `Module` base class gains `StringCRC mTypeId` and `GetTypeId() const`. Application sets it via `SetTypeId()` during wiring (same path as `SetProcessingUnit()`). This also provides the source for `ModuleStateInfo::typeId` in `IApplicationInspectable`.

## Status: Approved
