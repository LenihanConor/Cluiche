# System Spec: DiaBlackboardVisualDebugger

## Parent Application
@docs/specs/applications/dia/dia.md

**Gameplay Domains:** ai

## Purpose

DiaBlackboardVisualDebugger is the `IDebugDomain` implementation that makes blackboard slot state visible in `DiaDebugPanel`. It is a panel-only domain (`HasWorldDrawers() = false`).

The domain card shows a slot table: each registered key, its type tag (as a hex pointer or registered name), and its current value (hex dump by default, or a formatted string if a display callback is registered). The hex fallback is intentional — slots hold arbitrary typed data and a clean display requires per-type registration. The base implementation is fully functional without any registered formatters.

Following the `DiaXxxVisualDebugger` contract, `DiaBlackboard` has zero compile-time dependency on `DiaBlackboardVisualDebugger`.

## Responsibilities

- Implement `IDebugDomain` — all 16-AC contract methods
- `HasWorldDrawers()` returns `false`; `Register`/`Unregister` are no-ops
- One logical drawer: `SlotTable`
- `GetJSONState()` emits: slot array (key, type tag, value string), slot count stat
- `OnCommand("toggle", {drawer: "SlotTable"})` enables/disables the slot table section
- `OnCommand("setScale", ...)` — no-op
- Optional: `RegisterFormatter<T>(fn)` class template to register per-type display callbacks for friendly value strings
- Entire module guarded by `#ifdef DIA_DEBUG`
- `DiaBlackboardVisualDebugger.vcxproj` static library registered in `Cluiche.sln` under `3.1-Gameplay-Tools`
- `dia.diablackboardvisualdebugger.architecture.module.md` YAML module documentation

## Non-Responsibilities

- Blackboard read/write logic — DiaBlackboard
- Per-type value semantics — caller-registered formatters only
- Any runtime behaviour in Release — entire module excluded by `#ifdef DIA_DEBUG`

## Public Interfaces

```cpp
// DiaBlackboardVisualDebugger/BlackboardVisualDebugger.h
#ifdef DIA_DEBUG

#include <DiaVisualDebugger/Domain/IDebugDomain.h>

namespace Dia::Blackboard { class Blackboard; }

namespace Dia::Blackboard
{
    class BlackboardVisualDebugger : public Dia::VisualDebugger::IDebugDomain
    {
    public:
        // blackboard must outlive this object.
        explicit BlackboardVisualDebugger(const Blackboard& blackboard);

        Dia::Core::StringCRC  GetDomainId()      const override; // "blackboard"
        const char*           GetDisplayName()   const override; // "Blackboard"
        const char*           GetDescription()   const override; // see below
        Dia::Core::StringCRC  GetGroup()          const override; // "AIBehavior"
        Dia::Core::ColourRGBA GetAccentColour()   const override; // DebugGroupAccents::kAIBehavior

        bool HasWorldDrawers() const override { return false; }

        void GetJSONState(Dia::Core::JsonWriter& writer) override;
        void OnCommand(Dia::Core::StringCRC cmd, const Dia::Core::JsonValue& args) override;

        // Optional per-type display formatter.
        // fn: const char* (const void* data, char* buf, int bufSize)
        // Returns a pointer to buf with the formatted value string.
        // typeTag must match the tag stored in Blackboard slot registration.
        void RegisterFormatter(const void* typeTag,
                               const char* (*fn)(const void* data, char* buf, int bufSize));

    private:
        const Blackboard& mBlackboard;
        bool mSlotTableEnabled = true;

        static constexpr int kMaxFormatters = 16;
        struct FormatterEntry {
            const void* typeTag;
            const char* (*fn)(const void* data, char* buf, int bufSize);
        };
        Dia::Core::Containers::DynamicArrayC<FormatterEntry, kMaxFormatters> mFormatters;
    };
}
#endif // DIA_DEBUG
```

`GetDescription()` returns: `"Blackboard slots — key, type, and current value for each slot"` (62 chars ≤ 80 ✓)

### JSON State Schema

```json
{
  "drawers": [
    { "name": "SlotTable", "enabled": true }
  ],
  "stats": {
    "slotCount": 4
  },
  "slots": [
    { "key": "threat_target",  "type": "EntityHandle", "value": "id=42"    },
    { "key": "health_pct",     "type": "float",         "value": "0.72"    },
    { "key": "last_seen_pos",  "type": "Vector2D",      "value": "(12, 8)" },
    { "key": "patrol_index",   "type": "0x7FF8A321",    "value": "0x00000003" }
  ]
}
```

`"type"` is the registered type name if a formatter is present, otherwise the `typeTag` pointer rendered as a hex string. `"value"` is the formatter output if registered, otherwise the first 4 bytes of the slot data as `0x????????`. `"key"` is the `StringCRC` rendered as its string if available, otherwise as a hex CRC value.

### Panel Card Specification

- **Group:** AI / Behavior — accent `DebugGroupAccents::kAIBehavior` (`#ef4444`) via `var(--accent)`
- **Domain stat line:** `"Slots: N"` where N = slot count
- **Expanded body:** slot table — key column, type column, value column; unformatted slots shown in a muted/italic style to indicate hex fallback
- **Drawer toggle:** SlotTable

## Tests

`Tests/GoogleTests/DiaBlackboardVisualDebugger/TestBlackboardVisualDebugger.cpp`

### Mandatory shapes (AC-15)

| Shape | Description |
|-------|-------------|
| DrawerGate | Disabling `SlotTable` removes `slots` key from JSON |
| PrimitiveType | `HasWorldDrawers()` returns `false`; `GetDrawerCount()` returns 0 |
| ScaleSensitivity | N/A — `OnCommand("setScale", ...)` is no-op, does not crash |
| JSONRoundTrip | `GetJSONState()` drawer entries match current enabled state |
| OnCommandRoundTrip | `OnCommand("toggle", {drawer:"SlotTable"})` toggles; second call reverts |

### Domain-specific shapes

| Shape | Description |
|-------|-------------|
| `Slots_CountMatchesRegistered` | `stats.slotCount` equals number of registered slots |
| `Slots_KeyPresentForEachSlot` | `slots` array has one entry per slot; each has a `key` field |
| `Slots_HexFallback_NoFormatter` | Slot with no registered formatter emits hex type tag and hex value |
| `Slots_FormattedValue_WithFormatter` | After `RegisterFormatter(typeTag, fn)`, slot value uses formatter output |
| `Slots_FormatterOutput_TruncatedToBuf` | Formatter respects `bufSize`; output does not overflow |
| `EmptyBlackboard_EmptySlots_NoAssert` | Zero slots → `slots` is empty array, `slotCount` = 0, no crash |
| `MultipleFormatters_CorrectDispatch` | Two formatters registered; each type tag dispatches to the correct fn |
| `FormatterNotCalled_WhenDrawerDisabled` | Formatters not invoked when `SlotTable` is disabled (performance guard) |

## Dependencies on Other Systems

**Required:**
- **DiaBlackboard** — `Blackboard`, `VisitSlots()`
- **DiaVisualDebugger** — `IDebugDomain`, `DebugGroupAccents`
- **DiaCore** — `StringCRC`, `ColourRGBA`, `JsonWriter`, `JsonValue`, `DynamicArrayC`

**Explicitly excluded:**
- **ImGui**, **DiaEntity**, **DiaApplicationFlow**, **DiaAIBudget**, **DiaHTN**

## Contract Compliance

All 16 ACs from `debugger-contract.md` apply.

| AC | Notes |
|----|-------|
| AC-1 | Standalone `DiaBlackboardVisualDebugger.vcxproj` |
| AC-2 | `DiaBlackboard` has zero dep on `DiaBlackboardVisualDebugger` |
| AC-3 | Deps: DiaBlackboard + DiaVisualDebugger + DiaCore only |
| AC-4 | Implements all `IDebugDomain` pure virtuals |
| AC-5 | `GetDescription()` = 62 chars ✓ |
| AC-6 | No world drawers — palette rule N/A |
| AC-7 | No world drawers — scale rule N/A |
| AC-8 | No `ImGui::*` calls |
| AC-9 | `const Blackboard&` read-only via `VisitSlots()` |
| AC-10 | Emits `drawers` + `stats` minimum |
| AC-11 | `OnCommand("toggle", {drawer:"SlotTable"})` handled |
| AC-12 | `OnCommand("setScale", ...)` is no-op; `mSlotTableEnabled` uses `std::atomic<bool>` |
| AC-13/14 | `Tests/GoogleTests/DiaBlackboardVisualDebugger/TestBlackboardVisualDebugger.cpp` |
| AC-15 | All 5 mandatory shapes present — see Tests section |
| AC-16 | Panel card complies with standard spacing; accent via `var(--accent)` |

## Decisions

| ID | Decision | Rationale | Status | Binding |
|----|----------|-----------|--------|---------|
| SD-001 | Hex fallback for unregistered types | Blackboard holds arbitrary typed data; requiring formatters for all types is too restrictive. Hex is always correct even if unreadable. | Accepted | Yes |
| SD-002 | `RegisterFormatter` uses C-style function pointer, not `std::function` | `std::function` pulls in STL and may allocate. The formatter registry holds at most `kMaxFormatters` entries; a raw function pointer and a type-tag key is sufficient. | Accepted | Yes |
| SD-003 | Formatters not invoked when drawer disabled | Calling formatters has cost proportional to slot count; skip entirely when the section is hidden. | Accepted | Yes |

## Inherited Binding Decisions

| ID | Source | Decision | Implication |
|----|--------|----------|-------------|
| PD-001 | Platform | StringCRC for all IDs | Domain ID, group, drawer name, command keys use `StringCRC` |
| PD-004 | Platform | No STL in public APIs | `DynamicArrayC` used; no `std::vector` |
| PD-005 | Platform | x64 only | vcxproj targets x64 |
| PD-006 | Platform | vcxproj is source of truth | `.vcxproj` + `.vcxproj.filters` maintained |
| PD-007 | Platform | C++20 | `/std:c++20` |
| PD-008 | Platform | Directory.Build.props owns toolchain | vcxproj does not override OutDir/IntDir/toolset |
| AD-001 | Dia App | Module YAML docs | `dia.diablackboardvisualdebugger.architecture.module.md` required |
| AD-003 | Dia App | `Dia::<Module>::` namespace | All code in `Dia::Blackboard::` namespace |

## Open Design Questions

1. **`VisitSlots` key-to-string mapping** — `StringCRC` does not store the original string. The panel will show a hex CRC for most keys unless the engine maintains a global CRC-to-string reverse map. If a `StringCRC::GetSourceString()` or similar exists, use it; otherwise, document that key display requires pre-registration of string values in the CRC table.

2. **Slot value size cap** — `VisitSlots` provides a `const void* data` pointer. Formatting arbitrary-size values (e.g. a `Vector3D`) from raw memory is unsafe without knowing the type's size. Formatters are passed the data pointer; they must know their own type size. Ensure the formatter signature is clear that `data` is untyped and formatter is responsible for correct cast.

## Status

**Status:** Approved
