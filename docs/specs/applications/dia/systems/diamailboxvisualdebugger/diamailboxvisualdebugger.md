# System Spec: DiaMailboxVisualDebugger

## Parent Application
@docs/specs/applications/dia/dia.md

**Gameplay Domains:** ai

## Purpose

DiaMailboxVisualDebugger is the `IDebugDomain` implementation that makes mailbox queue health visible in `DiaDebugPanel`. It is a panel-only domain (`HasWorldDrawers() = false`).

The domain card shows a per-type queue table: capacity, current fill level, cumulative sends, cumulative drops (highlighting types with non-zero drops), and drain rate. This answers "is anything overflowing?" and "which types are most active?" at a glance.

`Mailbox::GetTypeStats<T>()` is template-based and cannot be called for unknown types. A small type-erased accessor is required in `DiaMailbox` — see Prerequisite.

Following the `DiaXxxVisualDebugger` contract, `DiaMailbox` has zero compile-time dependency on `DiaMailboxVisualDebugger`.

## Responsibilities

- **Prerequisite — add to `Mailbox`**: `GetTypeStatsByIndex(int)` type-erased accessor — see Prerequisite section
- Implement `IDebugDomain` — all 16-AC contract methods
- `HasWorldDrawers()` returns `false`; `Register`/`Unregister` are no-ops
- Two logical drawers: `QueueTable` (per-type stats), `DropAlerts` (types with drops > 0)
- `GetJSONState()` emits: per-type stats array, total registered type count, total drops across all types
- `OnCommand("toggle", {drawer: name})` enables/disables the named section
- `OnCommand("setScale", ...)` — no-op
- Entire module guarded by `#ifdef DIA_DEBUG`
- `DiaMailboxVisualDebugger.vcxproj` static library registered in `Cluiche.sln` under `3.1-Gameplay-Tools`
- `dia.diamailboxvisualdebugger.architecture.module.md` YAML module documentation

## Non-Responsibilities

- Message routing — DiaMailbox
- Type registration — caller
- Any runtime behaviour in Release — entire module excluded by `#ifdef DIA_DEBUG`

## Prerequisite: `Mailbox` Type-Erased Stats Accessor

These changes live in `DiaMailbox`. The existing `GetRegisteredTypeCount()` already exists. The new accessor lets the debugger iterate all registered types without knowing their concrete message types.

```cpp
// DiaMailbox/Mailbox.h (addition)
// Returns TypeStats for the type at position `typeIndex` in registration order.
// typeIndex must be in range [0, GetRegisteredTypeCount()).
// Returns a zeroed TypeStats if typeIndex is out of range.
TypeStats GetTypeStatsByIndex(int typeIndex) const;
```

`Mailbox` stores a parallel internal array of `TypeStats` alongside its typed slot storage, already populated by `Send()`, `Drain()`, and overflow logic. `GetTypeStatsByIndex` reads from that array. The `typeKey` field in `TypeStats` (already present) identifies the type; it is a `uint32_t` CRC of the type's identity.

This accessor is not `DIA_DEBUG`-gated — it is a general-purpose diagnostic accessor.

## Public Interfaces

```cpp
// DiaMailboxVisualDebugger/MailboxVisualDebugger.h
#ifdef DIA_DEBUG

#include <DiaVisualDebugger/Domain/IDebugDomain.h>

namespace Dia::Mailbox { class Mailbox; }

namespace Dia::Mailbox
{
    class MailboxVisualDebugger : public Dia::VisualDebugger::IDebugDomain
    {
    public:
        // mailbox must outlive this object.
        explicit MailboxVisualDebugger(const Mailbox& mailbox);

        Dia::Core::StringCRC  GetDomainId()      const override; // "mailbox"
        const char*           GetDisplayName()   const override; // "Mailbox"
        const char*           GetDescription()   const override; // see below
        Dia::Core::StringCRC  GetGroup()          const override; // "AIBehavior"
        Dia::Core::ColourRGBA GetAccentColour()   const override; // DebugGroupAccents::kAIBehavior

        bool HasWorldDrawers() const override { return false; }

        void GetJSONState(Dia::Core::JsonWriter& writer) override;
        void OnCommand(Dia::Core::StringCRC cmd, const Dia::Core::JsonValue& args) override;

    private:
        const Mailbox& mMailbox;
        bool mQueueTableEnabled = true;
        bool mDropAlertsEnabled = true;
    };
}
#endif // DIA_DEBUG
```

`GetDescription()` returns: `"Mailbox — per-type queue fill, send/drop/drain counters"` (56 chars ≤ 80 ✓)

### JSON State Schema

```json
{
  "drawers": [
    { "name": "QueueTable",  "enabled": true },
    { "name": "DropAlerts",  "enabled": true }
  ],
  "stats": {
    "typeCount":    3,
    "totalDropped": 12
  },
  "types": [
    {
      "typeKey":      "0xA1B2C3D4",
      "capacity":     64,
      "currentCount": 7,
      "fillPct":      11,
      "totalSent":    128,
      "totalDropped": 0,
      "totalDrained": 121,
      "hasDrops":     false
    },
    {
      "typeKey":      "0xDEADBEEF",
      "capacity":     16,
      "currentCount": 16,
      "fillPct":      100,
      "totalSent":    200,
      "totalDropped": 12,
      "totalDrained": 172,
      "hasDrops":     true
    }
  ]
}
```

`fillPct` = `round(currentCount / capacity × 100)`, clamped 0–100. `hasDrops` = `totalDropped > 0`. `stats.totalDropped` = sum of `totalDropped` across all types. `typeKey` is `TypeStats.typeKey` rendered as `"0x%08X"`.

### Panel Card Specification

- **Group:** AI / Behavior — accent `DebugGroupAccents::kAIBehavior` (`#ef4444`) via `var(--accent)`
- **Domain stat line:** `"Types: N, Drops: D"` where N = type count, D = total dropped
- **QueueTable section:** per-type table; typeKey, fill bar (currentCount/capacity), sent/drained counts; rows with `hasDrops == true` highlighted in warning colour
- **DropAlerts section:** only shows types where `hasDrops == true`; highlighted with drop count and overflow direction indicator
- **Drawer toggles:** QueueTable, DropAlerts

## Tests

`Tests/GoogleTests/DiaMailboxVisualDebugger/TestMailboxVisualDebugger.cpp`

### Mandatory shapes (AC-15)

| Shape | Description |
|-------|-------------|
| DrawerGate | Disabling `QueueTable` removes `types` key from JSON |
| PrimitiveType | `HasWorldDrawers()` returns `false`; `GetDrawerCount()` returns 0 |
| ScaleSensitivity | N/A — `OnCommand("setScale", ...)` is no-op, does not crash |
| JSONRoundTrip | `GetJSONState()` drawer entries match current enabled state |
| OnCommandRoundTrip | `OnCommand("toggle", {drawer:"QueueTable"})` toggles; second call reverts |

### Domain-specific shapes

| Shape | Description |
|-------|-------------|
| `Types_CountMatchesGetRegisteredTypeCount` | `stats.typeCount` == `Mailbox::GetRegisteredTypeCount()` |
| `Types_ArrayLengthMatchesTypeCount` | `types` array length == `stats.typeCount` |
| `Types_CapacityFromTypeStats` | Each entry `capacity` matches `TypeStats.capacity` from `GetTypeStatsByIndex()` |
| `Types_CurrentCountFromTypeStats` | Each entry `currentCount` matches `TypeStats.currentCount` |
| `Types_FillPct_Computed` | `fillPct == round(currentCount / capacity × 100)` |
| `Types_FillPct_ClampedAt100` | `currentCount > capacity` (defensive) clamps to 100 |
| `Types_TotalDropped_FromTypeStats` | Each entry `totalDropped` matches `TypeStats.totalDropped` |
| `Types_HasDrops_TrueWhenDropped` | `hasDrops == true` iff `totalDropped > 0` |
| `Stats_TotalDropped_SumOfAll` | `stats.totalDropped` == sum of `totalDropped` across all type entries |
| `DrawerGate_DropAlerts` | Disabling `DropAlerts` removes drop-alert entries (or section) from JSON |
| `Toggle_DropAlerts` | `OnCommand("toggle", {drawer:"DropAlerts"})` toggles it |
| `NoTypes_EmptyArray_NoAssert` | Zero registered types → `types` is empty, `typeCount` = 0, no crash |
| `CapacityZero_NoAssert` | `capacity == 0` does not cause divide-by-zero; `fillPct` = 0 |

## Dependencies on Other Systems

**Required:**
- **DiaMailbox** — `Mailbox`, `TypeStats`, `GetRegisteredTypeCount()`, `GetTypeStatsByIndex()` (prereq)
- **DiaVisualDebugger** — `IDebugDomain`, `DebugGroupAccents`
- **DiaCore** — `StringCRC`, `ColourRGBA`, `JsonWriter`, `JsonValue`, `DynamicArrayC`

**Explicitly excluded:**
- **ImGui**, **DiaEntity**, **DiaApplicationFlow**, **DiaAIBudget**, **DiaHTN**

## Contract Compliance

All 16 ACs from `debugger-contract.md` apply.

| AC | Notes |
|----|-------|
| AC-1 | Standalone `DiaMailboxVisualDebugger.vcxproj` |
| AC-2 | `DiaMailbox` has zero dep on `DiaMailboxVisualDebugger` |
| AC-3 | Deps: DiaMailbox + DiaVisualDebugger + DiaCore only |
| AC-4 | Implements all `IDebugDomain` pure virtuals |
| AC-5 | `GetDescription()` = 56 chars ✓ |
| AC-6 | No world drawers — palette rule N/A |
| AC-7 | No world drawers — scale rule N/A |
| AC-8 | No `ImGui::*` calls |
| AC-9 | `const Mailbox&` read-only via `GetTypeStatsByIndex()` |
| AC-10 | Emits `drawers` + `stats` minimum |
| AC-11 | `OnCommand("toggle", {drawer})` handled for both drawers |
| AC-12 | `OnCommand("setScale", ...)` is no-op; drawer enables use `std::atomic<bool>` |
| AC-13/14 | `Tests/GoogleTests/DiaMailboxVisualDebugger/TestMailboxVisualDebugger.cpp` |
| AC-15 | All 5 mandatory shapes present — see Tests section |
| AC-16 | Panel card complies with standard spacing; accent via `var(--accent)` |

## Decisions

| ID | Decision | Rationale | Status | Binding |
|----|----------|-----------|--------|---------|
| SD-001 | `GetTypeStatsByIndex(int)` added to `Mailbox` as non-debug accessor | Enables type-erased iteration without requiring the debugger to know message types. Non-debug because it is useful for testing and monitoring regardless of build config. | Accepted | Yes |
| SD-002 | Two drawers: QueueTable (all types) + DropAlerts (dropped only) | Queue health and overflow alerts are different questions. Separating them lets users hide the full table while keeping drop alerts visible. | Accepted | Yes |
| SD-003 | `typeKey` rendered as hex in JSON | Type keys are `uint32_t` CRC values with no guaranteed string mapping. Hex is always accurate; human-readable type names require a separate registration step out of scope for v1. | Accepted | Yes |

## Inherited Binding Decisions

| ID | Source | Decision | Implication |
|----|--------|----------|-------------|
| PD-001 | Platform | StringCRC for all IDs | Domain ID, group, drawer names, command keys use `StringCRC` |
| PD-004 | Platform | No STL in public APIs | `DynamicArrayC` used; no `std::vector` |
| PD-005 | Platform | x64 only | vcxproj targets x64 |
| PD-006 | Platform | vcxproj is source of truth | `.vcxproj` + `.vcxproj.filters` maintained |
| PD-007 | Platform | C++20 | `/std:c++20` |
| PD-008 | Platform | Directory.Build.props owns toolchain | vcxproj does not override OutDir/IntDir/toolset |
| AD-001 | Dia App | Module YAML docs | `dia.diamailboxvisualdebugger.architecture.module.md` required |
| AD-003 | Dia App | `Dia::<Module>::` namespace | All code in `Dia::Mailbox::` namespace |

## Open Design Questions

1. **Type name registration** — `typeKey` is a `uint32_t` CRC. If `DiaMailbox` maintains a `typeKey → const char*` name map (populated at `RegisterType<T>()` via `typeid(T).name()` or a custom name), the panel could show readable type names. Consider adding this as a v2 enhancement; document the hex display as the v1 default.

2. **Router stats** — `Mailbox` has routers (`GetRouterCount()`). Should the panel also show per-router stats (e.g. entity router miss count)? Deferred to v2 — router telemetry requires `IMailboxRouter` to expose its own stats interface, which is out of scope here.

## Status

**Status:** Approved
