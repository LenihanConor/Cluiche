# System Spec: DiaSaveGame

## Parent Application
@docs/specs/applications/dia/dia.md

**Gameplay Domains:** persistence, simulation

## Purpose

DiaSaveGame is a generic, application-agnostic save/load system for the Dia engine. It coordinates serialization across any number of registered participants, manages versioned save slots on disk, and orchestrates the sim lifecycle around I/O operations. It knows nothing about what is being saved — it only knows how to call each participant in a defined order, pass them typed read/write contexts, and assemble the result into a versioned manifest on disk.

Any game built on Dia registers its systems with `SaveRegistry` and calls `Save(slotId)` / `Load(slotId)`. DiaSaveGame handles the rest.

**Dependency chain:**
`DiaSaveGame → DiaSerializer (typed read/write primitives), DiaCore (containers, StringCRC, Observer, file I/O, threading)`

## Responsibilities

- Define `ISaveable` — contract any participant implements: `Serialize(SaveContext&)`, `Deserialize(LoadContext&)`, and a compile-time participant version number
- Define `SaveRegistry` — participants self-register by string ID; registration order defines serialization order; DiaSaveGame never references participant types directly
- Define `SaveContext` / `LoadContext` — typed read/write handles passed to each participant during a save/load pass; backed by DiaSerializer primitives; format-agnostic (participant never touches raw bytes or JSON keys directly)
- Define `SaveManifest` — header stored alongside save data: engine version, participant list with per-participant version numbers, format tag, and timestamp
- Provide `Save(slotId)` / `Load(slotId)` as the only public API surface — all path construction, ordering, and I/O owned internally
- Implement save orchestration — fire `OnSaveStarted`, call `Serialize` on each registered participant in registration order, write manifest + data, fire `OnSaveCompleted`
- Implement load orchestration — fire `OnLoadStarted`, read manifest, validate compatibility, call `Deserialize` on each participant in registration order, fire `OnLoadCompleted`
- Implement async I/O — serialize all participants to an in-memory buffer on the sim thread (no stall); flush buffer to disk off-thread; load reads from disk off-thread then deserializes on sim thread
- Implement versioning + migration — each participant declares a version number; when loading a save whose participant version differs from the current registration, registered migration callbacks run in sequence to bring the data forward before `Deserialize` is called
- Implement slot management — enumerate slots, delete slot, rename slot, query slot metadata (manifest header); enforce max-slot cap if configured
- Define `SaveConfig` — application-provided at init: base directory, slot naming pattern (e.g. `slot_{id}.sav`), max slots (optional), format (JSON or binary)
- Publish Observer events (SimPU, same-thread) — `OnSaveStarted`, `OnSaveCompleted`, `OnLoadStarted`, `OnLoadCompleted`, `OnMigrationApplied`; cross-PU relay is the caller's concern
- Emit `DIA_LOG_INFO` on save/load start and completion; `DIA_LOG_WARN` on missing participant at load time (participant registered in manifest but not in current registry — skip with warning); `DIA_LOG_ERROR` on manifest version mismatch that blocks load
- Provide test utilities under `DiaSaveGame/Testing/` — `InMemorySaveBackend` (bypasses disk for unit tests), `MockSaveable`, `AssertSlotExists`, `AssertEventFired`; shipped with library, consumer opt-in via include
- Use DiaCore containers exclusively in all public APIs (AD-002 / PD-004)
- Provide `dia.savegame.architecture.module.md` YAML module documentation
- Provide `DiaSaveGame.vcxproj` static library registered in `Cluiche.sln` under `domain/gameplay/core`

## Non-Responsibilities

- What data each participant saves — DiaSaveGame passes a context and calls the contract; content is the participant's concern
- Triggering saves — autosave timers, save-on-quit, checkpoint triggers are application concerns; DiaSaveGame exposes `Save()` / `Load()` and nothing else
- Save UI — slot picker, save-in-progress overlay, error dialogs belong to the application
- Compression or encryption — out of scope; raw JSON or binary only
- Network/cloud sync — local disk only
- DiaEntity, DiaEconomy, or any other domain type — participants register themselves; DiaSaveGame has no imports from domain systems
- IModule / PU wiring — DiaSaveGame does not provide an IModule; the application wires `SaveRegistry` init and `Save()`/`Load()` calls into its own SimPU module

## Public Interfaces

### ISaveable

```cpp
namespace Dia::SaveGame {

class ISaveable {
public:
    virtual ~ISaveable() = default;
    virtual void Serialize(SaveContext& ctx) const = 0;
    virtual void Deserialize(LoadContext& ctx) = 0;
    virtual uint32_t GetVersion() const = 0;
};

} // namespace Dia::SaveGame
```

### SaveRegistry

```cpp
namespace Dia::SaveGame {

class SaveRegistry {
public:
    void Register(StringCRC id, ISaveable* participant);
    void RegisterMigration(StringCRC id, uint32_t fromVersion, MigrationFn fn);
    void Unregister(StringCRC id);
};

} // namespace Dia::SaveGame
```

### SaveManager (primary API)

```cpp
namespace Dia::SaveGame {

class SaveManager {
public:
    void Init(const SaveConfig& config, SaveRegistry& registry);
    void Shutdown();

    SaveResult Save(StringCRC slotId);
    SaveResult Load(StringCRC slotId);

    void DeleteSlot(StringCRC slotId);
    void RenameSlot(StringCRC fromId, StringCRC toId);
    void EnumerateSlots(DynamicArrayC<SlotInfo>& out) const;

    ObserverSubject<OnSaveStarted>    saveStarted;
    ObserverSubject<OnSaveCompleted>  saveCompleted;
    ObserverSubject<OnLoadStarted>    loadStarted;
    ObserverSubject<OnLoadCompleted>  loadCompleted;
    ObserverSubject<OnMigrationApplied> migrationApplied;
};

} // namespace Dia::SaveGame
```

### SaveConfig

```cpp
namespace Dia::SaveGame {

struct SaveConfig {
    const char* baseDirectory;          // e.g. "out/saves/" or "%AppData%/MyGame/saves/"
    const char* slotPattern;            // e.g. "slot_{id}.sav"
    uint32_t    maxSlots;               // 0 = unlimited
    SaveFormat  format;                 // SaveFormat::Json | SaveFormat::Binary
};

} // namespace Dia::SaveGame
```

### SaveContext / LoadContext

```cpp
namespace Dia::SaveGame {

class SaveContext {
public:
    void Write(StringCRC key, int32_t value);
    void Write(StringCRC key, float value);
    void Write(StringCRC key, bool value);
    void Write(StringCRC key, const char* value);
    void BeginObject(StringCRC key);
    void EndObject();
    void BeginArray(StringCRC key);
    void EndArray();
};

class LoadContext {
public:
    bool Read(StringCRC key, int32_t& out) const;
    bool Read(StringCRC key, float& out) const;
    bool Read(StringCRC key, bool& out) const;
    bool Read(StringCRC key, Dia::Core::Containers::DynamicArrayC<char>& out) const;
    bool BeginObject(StringCRC key);
    void EndObject();
    bool BeginArray(StringCRC key, uint32_t& countOut);
    void EndArray();
};

} // namespace Dia::SaveGame
```

### Observer Events

```cpp
namespace Dia::SaveGame {

struct OnSaveStarted    { StringCRC slotId; };
struct OnSaveCompleted  { StringCRC slotId; SaveResult result; };
struct OnLoadStarted    { StringCRC slotId; };
struct OnLoadCompleted  { StringCRC slotId; SaveResult result; };
struct OnMigrationApplied { StringCRC participantId; uint32_t fromVersion; uint32_t toVersion; };

} // namespace Dia::SaveGame
```

## Features

| # | Feature | Description | Status |
|---|---------|-------------|--------|
| 1 | ISaveable + SaveRegistry | Participant contract, registration, ordering, unregistration | Draft |
| 2 | SaveConfig + Slot Management | Config struct, base directory, slot naming, enumeration, delete, rename, max-slot cap | Draft |
| 3 | SaveContext / LoadContext | Typed read/write handles backed by DiaSerializer; format-agnostic participant API | Draft |
| 4 | SaveManifest | Header with engine version, participant list + versions, format tag, timestamp | Draft |
| 5 | Save / Load Orchestration | Sim pause/resume, participant ordering, manifest write/read, compatibility check | Draft |
| 6 | Versioning + Migration | Per-participant versions, migration callback registration, forward-migration on load | Draft |
| 7 | Async I/O | In-memory serialize on sim thread, off-thread disk flush/read | Draft |
| 8 | Observer Events | OnSaveStarted/Completed, OnLoadStarted/Completed, OnMigrationApplied | Draft |
| 9 | Test Utilities | InMemorySaveBackend, MockSaveable, AssertSlotExists, AssertEventFired | Draft |

## Inherited Binding Decisions

| ID | Decision | How it applies to DiaSaveGame |
|----|----------|-------------------------------|
| PD-001 | Use StringCRC for all entity/component IDs | All slot IDs, participant IDs, context read/write keys, and event field names use StringCRC |
| PD-002 | ProcessingUnit/Phase/Module architecture | DiaSaveGame does not provide an IModule — PU wiring and lifecycle integration is the application's responsibility |
| PD-004 | No STL containers in public APIs | DynamicArrayC for participant lists, slot enumeration, and in-memory buffers; HashTable for registry lookup |
| PD-007 | C++20 required language standard | `[[nodiscard]]` on `SaveResult` returns; concepts used for internal template constraints |
| AD-001 | Module system with YAML frontmatter | `dia.savegame.architecture.module.md` required alongside implementation |
| AD-002 | No STL containers in public APIs | Redundant with PD-004; both apply |
| AD-003 | Namespace convention `Dia::<Module>::` | All public types live in `Dia::SaveGame::` |

## Open Design Questions

_None — all resolved._

| # | Question | Resolution |
|---|----------|------------|
| 1 | Partial saves | Always full saves. No participant tagging. Revisit only if a game requires frequent lightweight checkpoints. |
| 2 | Migration failure policy | Abort the load entirely. Return a descriptive `LoadResult` error code. Partial loads leave the game in undefined hybrid state — a clean failure is safer. |
| 3 | Sim pause ownership | `SaveManager` does not own pause. Pause and save are orthogonal concerns. `SaveManager` fires `OnSaveStarted` / `OnLoadStarted`; any system that needs to pause (e.g. SimPU) subscribes to those events and acts independently. `SaveManager` has no knowledge of PU lifecycle. |

## Status

**Status:** `Done`
**Plan:** @docs/specs/applications/dia/systems/diasavegame/diasavegame.plan.md
