# Feature Spec: DiaMessageBus Visual Debugger

## Parent System
@docs/specs/applications/dia/systems/diamessagebus/diamessagebus.md

## Problem Statement

`MessageBusModule` exposes `Bus::RegisterProducer<T>`, `Bus::Subscribe<T>`, and `GetLastTickLedger()` — but there is no way to see the live routing graph or per-tick message activity without attaching a debugger. Developers can't tell which types are flowing, whether reaction-pass messages are spiking, or whether a producer is registered but has no subscribers. `MessageBusDebugDomain` makes all of this visible in the in-game debug panel (`~` key) without touching Release builds.

This is an **in-game Visual Debugger** (not the Editor tier and not the Inspector tier — see SD-MBX2-011). It reads the `Bus` and the ledger **in-process** on the sim thread; it does NOT use `DiaDebugServer` or any cross-PU connection. The **Live tab** reads the last-tick ledger (`GetLastTickLedger()`, delivered by core-bus); the **History tab** reads the ledger history ring buffer (delivered by frame-ledger); the **Schema tab** reads the bus routing/producer table.

## Acceptance Criteria

| # | Criterion | Verification |
|---|-----------|--------------|
| AC-1 | `MessageBusDebugDomain` is registered with `VisualDebuggerModule` at stage start; pressing `~` shows a "Message Bus" panel under the "Systems" group | Manual run of MessageBusTestStage |
| AC-2 | **Schema tab** lists all registered types in alphabetical order; each row shows `typeId`, producer IDs, router (`broadcast` or `entity`), and subscriber IDs | Construct domain, register 3 types (1 broadcast + 1 entity + 1 both), call `GetJSONState()` — schema array has 3 entries with correct fields |
| AC-3 | **Live tab** shows the last completed tick's `LedgerMessageEntry` rows, sorted by `count` descending; totals row at bottom shows sum of `count` and `deliveries` | Feed a mock `LedgerSnapshot` with 3 entries, call `GetJSONState()` — live array matches; totals correct |
| AC-4 | **History tab** shows per-tick totals (count, deliveries, dropped) for the last `windowTicks` snapshots from the ring buffer; `droppedCount > 0` ticks are highlighted | Inject 10 mock snapshots (2 with dropped > 0), call `GetJSONState()` — history array has 10 entries; dropped indicator present on correct ticks |
| AC-5 | `OnCommand("selectTab", {tab:"history"})` switches the active tab; subsequent `GetJSONState()` reflects `activeTab:"history"` | Unit test |
| AC-6 | `OnCommand("setHistoryWindow", {ticks:30})` changes the history window; `GetJSONState()` reflects `historyWindowTicks:30` | Unit test |
| AC-7 | Entire domain class is compiled only when `DIA_DEBUG` is defined; `#include` of the header in a Release-mode TU must not require `IDebugDomain` | Compiler test: `#ifdef DIA_DEBUG` guard on header |
| AC-8 | `GetDomainId()` returns `StringCRC{"MessageBus"}`; `GetGroup()` returns `StringCRC{"Systems"}`; `HasWorldDrawers()` returns `false` | Unit test |

## Design

### Public Interface

```cpp
// MessageBusDebugDomain.h — guarded by #ifdef DIA_DEBUG

namespace Dia::MessageBus {

    class MessageBusDebugDomain : public Dia::VisualDebugger::IDebugDomain
    {
    public:
        explicit MessageBusDebugDomain(const MessageBusModule& module);

        Dia::Core::StringCRC  GetDomainId()    const override; // "MessageBus"
        const char*           GetDisplayName() const override; // "Message Bus"
        const char*           GetDescription() const override;
        Dia::Core::StringCRC  GetGroup()       const override; // "Systems"
        Dia::Core::ColourRGBA GetAccentColour()const override;

        bool HasWorldDrawers() const override { return false; }

        void Register  (Dia::VisualDebugger::DebugLayerManager& manager) override;
        void Unregister(Dia::VisualDebugger::DebugLayerManager& manager) override;

        // IDebugDomain data
        Dia::Core::Json::Value GetJSONState()               const override;
        void                   OnCommand(const char* cmd, const Dia::Core::Json::Value& args) override;

    private:
        const MessageBusModule& mModule;
        enum class Tab : uint8_t { Schema, Live, History };
        Tab      mActiveTab        = Tab::Live;
        uint16_t mHistoryWindowTicks = 60;   // last N ticks shown in History tab
    };

}
```

### JSON State Shape

```json
{
  "activeTab": "live",
  "historyWindowTicks": 60,

  "schema": [
    {
      "typeId": "HitEvent",
      "producerIds": ["DamageSystem"],
      "routerId": "entity",
      "subscriberIds": ["HealthComponent"]
    }
  ],

  "live": {
    "tickIndex": 1234,
    "totalMessages": 7,
    "totalDeliveries": 14,
    "entries": [
      { "typeId": "HitEvent", "routerId": "entity", "count": 4, "deliveries": 4, "pass": "Primary" },
      { "typeId": "DeathEvent", "routerId": "broadcast", "count": 3, "deliveries": 10, "pass": "Reaction" }
    ]
  },

  "history": [
    { "tickIndex": 1230, "count": 5, "deliveries": 9, "dropped": 0 },
    { "tickIndex": 1231, "count": 12, "deliveries": 24, "dropped": 2 }
  ]
}
```

### Commands

| Command | Args | Effect |
|---------|------|--------|
| `selectTab` | `{"tab": "schema"\|"live"\|"history"}` | Switch active tab |
| `setHistoryWindow` | `{"ticks": N}` (1–3600) | Change history window size |

### Dependencies (debug-only)

- `DiaMessageBus` — `MessageBusModule`, `Bus`, `LedgerSnapshot`, routing table
- `DiaVisualDebugger` — `IDebugDomain`, `DebugLayerManager`
- `DiaCore` — `StringCRC`, JSON serialisation

`DiaMessageBus` has zero compile-time dependency on `DiaVisualDebugger` outside `#ifdef DIA_DEBUG` blocks.

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Implement `MessageBusDebugDomain`: constructor, all `IDebugDomain` overrides, `GetJSONState()` Schema + Live + History sections, `OnCommand` tab/window dispatch | AC-2 through AC-8 (GoogleTest) | Not Started | sonnet | Prereq: frame-ledger (task 4 in system plan); `#ifdef DIA_DEBUG` guard |
| 2 | Wire `MessageBusDebugDomain` into `MessageBusTestStage` (`OnStart` register, `OnStop` unregister) | AC-1 — panel visible on `~` | Not Started | haiku | Prereq: 1, test stage scaffold |

## Status

**Status:** Approved
