# System Spec: DiaBlackboardInspector

## Parent Application
@docs/specs/applications/dia/dia.md

## Purpose

DiaBlackboardInspector provides live runtime visibility into all blackboards active in a running game. It adds a **BlackboardRegistry** to the engine (a non-singleton class owned by a game module), a **BlackboardInspectorSource** debug data source that serialises the registry state over WebSocket, and a **DiaBlackboardInspectorPlugin** dockable panel in CluicheEditor that renders the live view.

The panel shows every registered board by name, its slot keys, per-slot field values (for any type that has an opt-in serializer registered), and which `IBlackboardObserver` instances are currently attached to each board.

**Why:** AI and gameplay logic use blackboards as their primary shared data store. Without a live view, debugging requires log statements or breakpoints. The inspector surfaces board contents and observer state at a glance, turning blackboard debugging into a first-class editor experience.

**Dependency chain:**
```
BlackboardRegistry    (Dia/DiaBlackboard — new class, no new dep)
BlackboardInspectorSource  (CluicheGameBaseline — depends on BlackboardRegistry, DiaDebugServer, DiaCore)
DiaBlackboardInspectorPlugin (Dia/DiaBlackboardInspector — depends on DiaEditor)
```

## Responsibilities

- Add `BlackboardRegistry` to `DiaBlackboard` — a plain (non-singleton) class that holds a flat list of `(id: StringCRC, label: const char*, board: const Blackboard*)` entries; owned by a game Module
- Add `RegisterSerializer<T>` support to `BlackboardRegistry` — opt-in per struct; lambda-based; serializes slot field values to `Json::Value`; stored keyed by TypeTag pointer (same mechanism `Blackboard` uses internally for type identity); slots without a registered serializer emit `"[no serializer]"`
- Amend `IBlackboardObserver` to add `virtual Dia::Core::StringCRC GetId() const = 0;` — allows the inspector source to emit observer names rather than counts
- Implement `BlackboardInspectorSource` in `CluicheGameBaseline` — inherits `ChangeDetectedSourceBase`; topic `StringCRC{"blackboard.state"}`; runs on SimPU; pushes `BlackboardInspectEvent` to a named `EventStream`; `DebugServerHostModule` (MainPU) drains the stream and calls `NotifySubscribers`
- Implement `DiaBlackboardInspectorPlugin` in `Dia/DiaBlackboardInspector/` — inherits `LiveConnectionPluginBase`; subscribes to `"blackboard.state"`; dockable panel rendering board list with collapsible rows, slot keys + field values, and observer name list
- Provide a dockable React/Vite panel under `Dia/DiaBlackboardInspector/UI/` — same tech stack as all other `LiveConnectionPluginBase` inspectors (React 18 + Zustand + `@dia/editor-ui`); built to `dist/` by the pipeline
- Provide `REGISTER_EDITOR_PLUGIN(DiaBlackboardInspectorPlugin, "DiaBlackboardInspector")` registration
- Provide `DiaBlackboardInspector.vcxproj` static library registered in `Cluiche.sln`
- Provide `dia.blackboardinspector.architecture.module.md` YAML module doc

## Non-Responsibilities

- Field value serialization for types without an explicit `RegisterSerializer<T>` call — those show `"[no serializer]"`
- Editing slot field values at runtime — read-only inspector in v1; write-back is a future feature
- Blackboard slot value history / timeline — v1 shows current state only
- Per-frame mutation callbacks — `IBlackboardObserver` remains lifecycle-only; field changes still poll-only
- Thread safety inside `BlackboardRegistry` — single-threaded; caller (SimPU) owns synchronisation
- Debugging `GlobalBlackboard` specially — it can be registered in `BlackboardRegistry` like any other board by the owning module

## Public Interfaces

### BlackboardRegistry

```cpp
namespace Dia::Blackboard {

    using SerializeFn = std::function<void(const void* data, Json::Value& out)>;

    struct BlackboardEntry {
        Dia::Core::StringCRC            id;
        const char*                     label;   // human-readable owner name
        const Blackboard*               board;   // non-owning ptr; must outlive entry
    };

    class BlackboardRegistry {
    public:
        // Board registration
        void Register(Dia::Core::StringCRC id, const char* label,
                      const Blackboard& board);
        void Unregister(Dia::Core::StringCRC id);

        // Board enumeration (read-only)
        const Dia::Core::Containers::DynamicArrayC<BlackboardEntry, 16>& GetAll() const;
        int  GetCount() const;

        // Opt-in field value serialization per struct type
        template<typename T>
        void RegisterSerializer(SerializeFn fn);

        // Internal: called by BlackboardInspectorSource
        bool HasSerializer(const void* typeTag) const;
        void Serialize(const void* typeTag, const void* data, Json::Value& out) const;

    private:
        struct SerializerEntry {
            const void* typeTag;
            SerializeFn fn;
        };

        Dia::Core::Containers::DynamicArrayC<BlackboardEntry,   16> mEntries;
        Dia::Core::Containers::DynamicArrayC<SerializerEntry,   32> mSerializers;
    };
}
```

### IBlackboardObserver amendment

```cpp
namespace Dia::Blackboard {
    class IBlackboardObserver {
    public:
        virtual ~IBlackboardObserver() = default;
        virtual Dia::Core::StringCRC GetId() const = 0;            // NEW — observer identity
        virtual void OnSlotRegistered(Dia::Core::StringCRC key) = 0;
        virtual void OnSlotUnregistered(Dia::Core::StringCRC key) = 0;
    };
}
```

**Migration:** All existing implementations (`MockBlackboardObserver` in test helpers, any game observers) must add a trivial `GetId()` override returning a stable `StringCRC`. This is a breaking API change — all consumers are within this repo.

### BlackboardInspectorSource (game-side, not in Dia lib)

```cpp
// CluicheGameBaseline/Modules/InspectorSources/BlackboardInspectorSource.h
class BlackboardInspectorSource final
    : public Dia::DebugServer::ChangeDetectedSourceBase {
public:
    explicit BlackboardInspectorSource(
        const Dia::Blackboard::BlackboardRegistry& registry,
        Dia::Core::EventStream<BlackboardInspectEvent>& pushStream);

    Dia::Core::StringCRC  GetTopic()  const override;   // "blackboard.state"
    Dia::DebugServer::SourcePolicy GetPolicy() const override;

protected:
    unsigned int CollectAndHash(Json::Value& payload) override;
};
```

### Wire payload format

```json
{
  "boards": [
    {
      "id":    "player",
      "label": "PlayerModule",
      "slots": [
        {
          "key":   "health",
          "type":  "HealthBoard",
          "value": { "current": 80.0, "max": 100.0 }
        },
        {
          "key":   "threat",
          "type":  "ThreatBoard",
          "value": "[no serializer]"
        }
      ],
      "observers": ["HealthUI", "DamageSystem"]
    }
  ]
}
```

### Log channel

```cpp
static constexpr Dia::Core::StringCRC kLogChannel{"BlackboardInspector"};
// DIA_LOG_INFO on registry Register/Unregister calls
// DIA_LOG_INFO when the inspector source activates/deactivates
```

## UI Mockup

**Mockup:** @docs/specs/applications/dia/systems/diablackboardinspector/mockup/blackboard-inspector.html

The panel has three visual states:
1. **Disconnected** — gray status bar, "Waiting for game connection", empty list
2. **Connected, no boards registered** — green status, "No blackboards registered"
3. **Connected, boards present** — collapsible board rows, slot table, observer chips

## Features

| Feature | Description | Spec | Status |
|---------|-------------|------|--------|
| BlackboardRegistry | Engine-side registry class + opt-in `RegisterSerializer<T>` + `IBlackboardObserver::GetId()` amendment | [blackboard-registry.md](blackboard-registry.md) | Draft |
| BlackboardInspectorSource | Game-side `ChangeDetectedSourceBase`; SimPU → EventStream push; `"blackboard.state"` topic | [blackboard-inspector-source.md](blackboard-inspector-source.md) | Draft |
| DiaBlackboardInspectorPlugin | Editor plugin + dockable HTML/JS panel; `LiveConnectionPluginBase`; collapsible board list | [blackboard-inspector-plugin.md](blackboard-inspector-plugin.md) | Draft |

## Dependencies on Other Systems

**Required:**
- **DiaBlackboard** — `Blackboard`, `IBlackboardObserver`, `BlackboardRegistry` (new); no new external dep for the library itself
- **DiaDebugServer** — `ChangeDetectedSourceBase`, `DebugServerHostModule` (wiring); `EventStream` push pattern
- **DiaEditor** — `LiveConnectionPluginBase`, `EditorPluginBase`, `GameConnectionManager`; plugin infrastructure
- **DiaCore** — `StringCRC`, `DynamicArrayC`, `DIA_LOG_*`, `DIA_ASSERT`
- **jsoncpp** — `Json::Value` payload serialization (already a DiaDebugServer dep)

**Explicitly excluded:**
- **DiaEntity / diaentitytemplate** — blackboards are not entity-specific; `BlackboardComponent` is already an entity integration layer in DiaBlackboard; the inspector needs none of it
- **DiaObservation** — metrics and traces are not wired into v1 of this inspector; `DIA_LOG_INFO` via `DiaObservation/Log/` is sufficient

## Inherited Binding Decisions

| ID | Source | Decision | Implication for this system |
|----|--------|----------|-----------------------------|
| PD-001 | Platform | StringCRC for all IDs | `BlackboardRegistry` entries keyed by `StringCRC`; `IBlackboardObserver::GetId()` returns `StringCRC`; topic constant is a `StringCRC` |
| PD-002 | Platform | PU/Phase/Module architecture | `BlackboardInspectorSource` runs on SimPU; `DebugServerHostModule` drains the EventStream on MainPU |
| PD-004 | Platform | No STL containers in public APIs | `BlackboardRegistry::GetAll()` returns `const DynamicArrayC<BlackboardEntry, 16>&`; `mEntries` and `mSerializers` are `DynamicArrayC`; internal `std::function` for serializer lambdas is acceptable (private implementation detail) |
| PD-005 | Platform | x64 only | `DiaBlackboardInspector.vcxproj` targets x64 exclusively |
| PD-006 | Platform | Visual Studio project files are source of truth | `DiaBlackboardInspector.vcxproj` + `.filters` created and maintained; registered in `Cluiche.sln` |
| PD-007 | Platform | C++20 required | Compiled under `/std:c++20`; template `RegisterSerializer<T>` uses C++20 concepts if needed |
| PD-008 | Platform | `Directory.Build.props` owns OutDir/IntDir/toolchain | `DiaBlackboardInspector.vcxproj` must NOT override `OutDir`, `IntDir`, `PlatformToolset`, `WindowsTargetPlatformVersion`, or `LanguageStandard` |
| PD-009 | Platform | Generated output under `Cluiche/out/` | Plugin `dist/` assets copied to `Cluiche/out/CluicheEditor/` by the editor pipeline |
| ED-REACT | Editor | React + Vite + `@dia/editor-ui` required for all `LiveConnectionPluginBase` UIs | Consistent with `DiaEntityInspector`, `DiaAssetRuntimeInspector`, `DiaApplicationFlowInspector`; disconnected overlay is a React component, not the host frame |
| AD-001 | Dia App | Module system with YAML frontmatter | `dia.blackboardinspector.architecture.module.md` required with full public API, responsibilities, and dependency declarations |
| AD-002 | Dia App | No STL containers in public APIs | Reinforces PD-004; `std::function` for `SerializeFn` is private; no `std::vector` or `std::map` in any public header |
| AD-003 | Dia App | Namespace `Dia::<Module>::` | `BlackboardRegistry` in `Dia::Blackboard::`; plugin in `Dia::Editor::` (matches existing plugin convention) |

## Open Design Questions

_None — all three design questions resolved during planning:_
1. _SimPU owns the registry; EventStream push pattern used_
2. _Observer identity via `StringCRC GetId()` on `IBlackboardObserver`_
3. _Field values opt-in via `RegisterSerializer<T>` on `BlackboardRegistry`_

## Status

`Done` — Plan: @docs/specs/applications/dia/systems/diablackboardinspector/diablackboardinspector.plan.md
