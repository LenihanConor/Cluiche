# System Spec: E1 Save/Load

**Parent:** @docs/specs/applications/cow/cow.md  
**Design source:** `Output Docs/GDD/10_PrototypeStrategy/E1_SaveContinue.md` (DesignDocs repo)

**Gameplay Domains:** persistence

## Summary

E1 is CoW's save/load orchestration system. It wires CoW's gameplay modules (GameSpeedController, RandomStreamModule, future A1/B/C/D systems) to DiaSaveGame's generic save/load infrastructure, defines CoW's save directory layout and slot naming, and provides the `SaveLoadModule` that exposes DiaAPI commands (`cow.save <slot>`, `cow.load <slot>`) and bus messages for triggering saves/loads. E1 owns *when* and *how* saves happen in CoW; DiaSaveGame owns the serialization mechanism.

## Problem

CoW is a centuries-long simulation where:
- The player must be able to save their progress at any time and resume exactly where they left off (deterministic replay)
- Multiple systems need to persist state: GameSpeedController (day counter), RandomStreamModule (RNG stream state), future A1 ecology state, B-layer dragon data, C-layer clan data, etc.
- Each system knows what data it needs to save, but no system should know about the others' save formats or orchestrate the overall save/load flow
- Saves must be robust: if a save fails mid-write, the previous save must remain valid; if a load fails, the game must not be left in a half-loaded state
- F-2 Core Runtime testbed needs to prove save/load round-trip: save at day N, load, verify day counter is exactly N and the simulation continues deterministically

Without E1, each CoW system would implement its own save/load logic, leading to fragmented save files, no versioning, no migration support, and no way to prove determinism across save/load boundaries.

## Goals

1. **Single entry point** — `SaveLoadModule` exposes `cow.save <slot>` and `cow.load <slot>` DiaAPI commands; all CoW systems use these, never call DiaSaveGame directly
2. **Participant registry** — E1 owns the `SaveRegistry` and registers all CoW systems that implement `ISaveable` (GameSpeedController via an E1 adapter, RandomStreamModule, future systems)
3. **Save directory layout** — CoW saves live under `out/saves/cow/`, slots named `slot_<id>.sav` (JSON format for debuggability in prototyping)
4. **Save orchestration** — Save/load only at the sim-frame boundary, where no system is mid-tick (see Frame-Boundary Save below); after saving or loading, fire a bus message so UI can react
5. **Deterministic replay** — F-2 testbed proves: save at day 1000, load, verify `GameSpeedState::dayCounter == 1000` and `RandomStreamModule::GetState()` matches
6. **Always-paused resume** — On load, GameSpeedController always resumes paused (A2 edge case EC-03, enforced by `GameSpeedController::RestoreState`); player must explicitly unpause
7. **Test 5 coverage** — F-2 Test 5 ("Save/Load Round-Trip") exercises `cow.save quick`, `cow.load quick`, and asserts all registered participants' state matches

## Non-Goals

1. **Autosave** — F-2 saves are manual (via command or bus message); autosave timers are deferred to G-1 or later
2. **Cloud sync** — Local disk only; no Steam Cloud, Xbox Live, or custom cloud backends
3. **Compression or encryption** — Raw JSON for F-2; binary format and compression are post-G-1 optimizations
4. **Save UI** — F-2 testbed uses command-line saves; UI slot picker deferred to V-3 or later
5. **Migration testing** — F-2 proves round-trip of current version only; cross-version migration testing deferred until post-G-1 when save format is stable
6. **Per-system save filtering** — Always full saves; no "quick save just the clock" or "autosave without economy"; all registered participants serialize every time

## Architecture

### Participant Registration

E1 owns the `SaveRegistry` singleton and registers participants in `SaveLoadModule::DoStart()`:

```cpp
void SaveLoadModule::DoStart() {
    // Register CoW systems in save order
    mRegistry.Register("game_speed"_crc,    &mGameSpeedParticipant);
    mRegistry.Register("random_stream"_crc, &mRandomStreamRef);
    // Future: A1, B-layer, C-layer, etc.
    
    mSaveManager.Init(mConfig, mRegistry);
}
```

**Registration order = serialization order.** GameSpeedController goes first (calendar is foundational); RNG second (seeded from clock); gameplay systems after that. Game time itself is saved by DiaSimTime's own `SimTimeSaveState`.

### Save Directory Layout

```
out/saves/cow/
    slot_autosave.sav
    slot_quick.sav
    slot_001.sav
    slot_002.sav
    ...
```

**SaveConfig:**
```cpp
SaveConfig config;
config.baseDirectory = "out/saves/cow/";
config.slotPattern   = "slot_{id}.sav";
config.maxSlots      = 0;  // unlimited for prototyping
config.format        = SaveFormat::Json;
```

### Save Orchestration (Sync)

**Save flow (F-2 simple case, no async I/O yet):**

```
1. User calls `cow.save quick`
2. SaveLoadModule receives DiaAPI command
3. Publish SaveRequested{slotId="quick"}
4. Defer to the next frame boundary (see Frame-Boundary Save below)
5. Call SaveManager::Save("quick"_crc)
   - SaveManager calls Serialize() on each registered participant in order
   - Each participant writes its state to SaveContext
   - SaveManager writes manifest + data to disk as slot_quick.sav
6. Publish SaveCompleted{slotId="quick", result=Success}
7. Log "Save completed: slot_quick.sav"
```

**Load flow:**

```
1. User calls `cow.load quick`
2. SaveLoadModule receives DiaAPI command
3. Publish LoadRequested{slotId="quick"}
4. Pause the sim (publish SpeedChangeRequested{Paused})
5. Call SaveManager::Load("quick"_crc)
   - SaveManager reads manifest + data from slot_quick.sav
   - SaveManager calls Deserialize() on each registered participant in order
   - Each participant restores its state from LoadContext
6. Publish LoadCompleted{slotId="quick", result=Success}
7. Log "Load completed: slot_quick.sav"
8. GameSpeedController stays Paused (A2 rule: always resume paused)
```

### Frame-Boundary Save

GameSpeedController publishes at most one `SimulationTick` per sim frame, and the bus fully dispatches it (Primary and Reaction passes) within that frame. So the start of the next sim frame, before GameSpeedController publishes the next day, is a point where no system is mid-tick.

- Save/load requests (DiaAPI command or `SaveRequested` / `LoadRequested`) are **recorded as pending**, never executed in the handler.
- `SaveLoadModule::DoUpdate` executes the pending request. In `cow_main.diaapp`, `SaveLoadModule` is ordered **before** `GameSpeedControllerModule` on SimPU.
- No `TickCompleted` message is needed. It was dropped because Reaction-pass handlers can't post, so a same-update "all subscribers finished" message is impossible.

**Sync vs Async:** F-2 uses synchronous I/O (save/load block the sim thread). DiaSaveGame supports async I/O (Feature 7), but E1 defers it until S-7 Endurance testbed (when save times matter).

### ISaveable Implementations

**GameSpeedController (example):** an E1-owned adapter. GameSpeedController exposes a plain `GameSpeedState` and knows nothing about save files. Only the day counter needs saving: speed, pause source and modal depth are always reset on load (A2 EC-03).

```cpp
class GameSpeedSaveParticipant : public ISaveable {
public:
    explicit GameSpeedSaveParticipant(GameSpeedControllerModule& module) : mModule(module) {}

    void Serialize(SaveContext& ctx) const override {
        ctx.Write("day_counter"_crc, mModule.GetController().GetState().dayCounter);
    }
    
    void Deserialize(LoadContext& ctx) override {
        GameSpeedState saved{};
        ctx.Read("day_counter"_crc, saved.dayCounter);
        mModule.RestoreState(saved);   // always resumes Paused (A2 EC-03)
    }
    
    uint32_t GetVersion() const override { return 1; }

private:
    GameSpeedControllerModule& mModule;
};
```

**RandomStreamModule (example):**

```cpp
class RandomStreamModule : public SimModule, public ISaveable {
public:
    void Serialize(SaveContext& ctx) const override {
        ctx.Write("master_seed"_crc, mMasterSeed);
        ctx.Write("stream_count"_crc, mStreams.Size());
        
        ctx.BeginArray("streams"_crc);
        for (const auto& stream : mStreams) {
            ctx.BeginObject("stream"_crc);
            ctx.Write("id"_crc, stream.id.GetCRC());
            ctx.Write("state"_crc, stream.generator.GetState());
            ctx.EndObject();
        }
        ctx.EndArray();
    }
    
    void Deserialize(LoadContext& ctx) override {
        ctx.Read("master_seed"_crc, mMasterSeed);
        
        uint32_t count = 0;
        if (ctx.BeginArray("streams"_crc, count)) {
            mStreams.Clear();
            for (uint32_t i = 0; i < count; ++i) {
                if (ctx.BeginObject("stream"_crc)) {
                    uint64_t id = 0;
                    uint64_t state = 0;
                    ctx.Read("id"_crc, id);
                    ctx.Read("state"_crc, state);
                    
                    // Reconstruct stream
                    auto& stream = mStreams.PushBack();
                    stream.id = StringCRC(id);
                    stream.generator.SetState(state);
                    
                    ctx.EndObject();
                }
            }
            ctx.EndArray();
        }
    }
    
    uint32_t GetVersion() const override { return 1; }
};
```

### Bus Messages

**Published by SaveLoadModule:**

```cpp
// Player or automated system requests a save
struct SaveRequested {
    StringCRC  slotId;  // e.g. "quick", "autosave", "001"
};

// Save completed (success or failure)
struct SaveCompleted {
    StringCRC   slotId;
    SaveResult  result;  // Success, DiskFull, PermissionDenied, etc.
};

// Player or automated system requests a load
struct LoadRequested {
    StringCRC  slotId;
};

// Load completed (success or failure)
struct LoadCompleted {
    StringCRC   slotId;
    LoadResult  result;  // Success, FileNotFound, VersionMismatch, etc.
};
```

**Subscribed by SaveLoadModule:** `SaveRequested`, `LoadRequested` (recorded as pending; executed at the frame boundary).

### DiaAPI Commands

**SaveLoadModule registers:**

```cpp
DiaAPI::RegisterCommand("cow.save", "Save game to slot (usage: cow.save <slotId>)", 
    [this](const CommandArgs& args) {
        if (args.argc < 2) {
            return "Error: usage: cow.save <slotId>";
        }
        mPendingSave = StringCRC(args.argv[1]);   // executed at the next frame boundary
        return "Save queued";
    });

DiaAPI::RegisterCommand("cow.load", "Load game from slot (usage: cow.load <slotId>)",
    [this](const CommandArgs& args) {
        if (args.argc < 2) {
            return "Error: usage: cow.load <slotId>";
        }
        mPendingLoad = StringCRC(args.argv[1]);   // executed at the next frame boundary
        return "Load queued";
    });

DiaAPI::RegisterCommand("cow.save.list", "List all save slots",
    [this](const CommandArgs& args) {
        DynamicArrayC<SlotInfo, 32> slots;
        mSaveManager.EnumerateSlots(slots);
        
        std::stringstream ss;
        ss << "Save slots (" << slots.Size() << "):\n";
        for (const auto& slot : slots) {
            ss << "  " << slot.slotId.GetDebugName() << " - " 
               << slot.timestamp << " (" << slot.sizeBytes << " bytes)\n";
        }
        return ss.str();
    });
```

## Public Interfaces

### SaveLoadModule

```cpp
namespace CoW {

class SaveLoadModule : public Dia::ApplicationFlow::SimModule {
public:
    SaveLoadModule(GameSpeedControllerModule& gameSpeed, RandomStreamModule& randomStream);
    
    void DoStart() override;
    void DoStop() override;
    
    // Direct API (alternative to DiaAPI commands)
    SaveResult Save(Dia::Core::StringCRC slotId);
    LoadResult Load(Dia::Core::StringCRC slotId);
    
    void EnumerateSlots(Dia::Core::Containers::DynamicArrayC<SlotInfo, 32>& out) const;
    void DeleteSlot(Dia::Core::StringCRC slotId);

private:
    Dia::SaveGame::SaveConfig   mConfig;
    Dia::SaveGame::SaveRegistry mRegistry;
    Dia::SaveGame::SaveManager  mSaveManager;
    
    GameSpeedSaveParticipant  mGameSpeedParticipant;
    RandomStreamModule&       mRandomStreamRef;
};

} // namespace CoW
```

## Features

| # | Feature | Description | Status |
|---|---------|-------------|--------|
| 1 | SaveLoadModule Core | SaveConfig, SaveRegistry, participant registration, DiaAPI commands | Approved |
| 2 | GameSpeedController Serialization | `GameSpeedSaveParticipant` adapter (day counter only), version 1; always-paused resume enforced by `GameSpeedController::RestoreState` | Approved |
| 3 | RandomStream Serialization | `RandomStreamModule::Serialize/Deserialize`, version 1, master seed + per-stream state | Approved |
| 4 | Save/Load Bus Messages | SaveRequested, SaveCompleted, LoadRequested, LoadCompleted | Approved |
| 5 | Frame-Boundary Save | Save/load requests recorded as pending and executed in `SaveLoadModule::DoUpdate`, ordered before `GameSpeedControllerModule` (replaces `TickCompleted` synchronization) | Approved |

> **Note:** Features 1-5 ship together as one unit for F-2 testbed. Future systems (A1, B-layer, etc.) add themselves to the registry as they land.

## Inherited Binding Decisions

| ID | Decision | How it applies to E1 |
|----|----------|----------------------|
| PD-001 | Use StringCRC for entity/component IDs | All slot IDs, participant IDs, save/load context keys use StringCRC |
| PD-002 | ProcessingUnit/Phase/Module architecture | SaveLoadModule is a proper SimModule on SimPU; no singletons or static state |
| PD-004, AD-002 | No STL containers in public APIs | Uses DynamicArrayC for slot enumeration; DiaSaveGame already enforces this |
| PD-007 | C++20 required language standard | `[[nodiscard]]` on Save/Load return values |
| AD-001 | CoW develops on cow branch with ask-permission for Dia edits | E1 is pure CoW code (Cluiche/CoW/); no Dia edits needed (DiaSaveGame is Done) |
| AD-002 | Four-tier dependency model | E1 is CoW-Shared tier (used by all testbeds that need persistence) |
| AD-003 | GDD owns design intent, spec tree owns implementation | GDD E1_SaveContinue owns *what* must be proven (deterministic replay, always-paused resume); this spec owns *how* it's built |

## Open Design Questions

1. **Async I/O timing** — DiaSaveGame supports async I/O (serialize to buffer on sim thread, flush off-thread). Should E1 use it in F-2, or defer until S-7?
   - **Recommendation:** Defer. F-2 saves are small (< 1KB for GameSpeedController + RNG); sync I/O adds no perceptible latency. S-7 Endurance testbed (large saves, frequent autosaves) is where async pays off.

2. **Slot naming convention** — F-2 uses `slot_quick.sav` and `slot_autosave.sav`. Should numbered slots be `slot_001.sav` or `slot_1.sav`?
   - **Recommendation:** `slot_{id}.sav` with no padding. DiaSaveGame's SlotManager sorts by timestamp, not filename, so lexical order doesn't matter.

3. **Save confirmation on overwrite** — Should `cow.save quick` prompt "Overwrite existing save?" or silently overwrite?
   - **Recommendation:** Silent overwrite for F-2 (command-line only). V-3 UI adds confirmation dialogs.

4. **Migration testing strategy** — When should CoW test cross-version migration (e.g., GameSpeedController v1 → v2)?
   - **Recommendation:** Post-G-1. F-2 proves round-trip of current version only. Once the save format stabilizes (post-G-1), add a `tests/fixtures/` directory with old-version save files and a migration test suite.

5. **Error handling on partial load** — If GameSpeedController deserializes successfully but RandomStream fails, should E1 abort the entire load or leave the game in a hybrid state?
   - **Recommendation:** Abort. DiaSaveGame's LoadResult returns on first failure; E1 logs the error and leaves the game unchanged. Partial loads create undefined state.

## Dependencies

**Requires:**
- **DiaSaveGame** (ISaveable, SaveManager, SaveRegistry, SaveConfig, SaveContext/LoadContext)
- **GameSpeedController** (exposes `GameSpeedState` / `RestoreState`; at most one `SimulationTick` per sim frame makes the frame boundary a safe save point)
- **RandomStreamModule** (implements ISaveable)
- **DiaMessageBus** (SaveRequested, LoadCompleted, etc.)
- **DiaAPI** (command registration for `cow.save`, `cow.load`, `cow.save.list`)
- **DiaCore** (StringCRC, DynamicArrayC, file I/O, DIA_LOG_*)

**Required by:**
- **F-2 Core Runtime testbed** (Test 5: Save/Load Round-Trip)
- **All future testbeds** (S-1 through G-1 all assume saves work)
- **V-3 UI** (slot picker UI depends on EnumerateSlots)
- **S-7 Endurance** (history paging and long-run saves depend on robust save/load)

**Circular dependency risk:** None. E1 depends on GameSpeedController (state struct, module ordering); GameSpeedController knows nothing about E1.

## Test Strategy

E1 correctness is proven primarily by **F-2 Core Runtime testbed, Test 5: Save/Load Round-Trip**.

### Unit Tests (GoogleTests/CoW/SaveLoad)

1. **SaveConfig initialization** — Verify baseDirectory, slotPattern, format defaults
2. **Participant registration order** — Register GameSpeedSaveParticipant, RNG, verify SaveRegistry::GetParticipants() returns them in insertion order
3. **Serialize/Deserialize GameSpeedController** — Serialize, then Deserialize, verify day counter matches and state is Paused/Player/modalDepth 0
4. **Serialize/Deserialize RandomStream** — Manually call RandomStream::Serialize, then Deserialize, verify master seed and all stream states match
5. **SlotInfo enumeration** — Create 3 saves, call EnumerateSlots, verify all 3 appear with correct timestamps
6. **DeleteSlot** — Create a save, delete it, verify file is gone and EnumerateSlots no longer lists it

### Integration Tests (F-2 testbed, Test 5)

7. **Round-trip determinism** — Run sim to day 1000, call `cow.save quick`, reset sim, call `cow.load quick`, verify:
   - `GameSpeedState::dayCounter == 1000`
   - `RandomStreamModule::GetState("default") == savedState`
   - Next 10 ticks produce identical RNG values as the original run
8. **Always-paused resume** — Save while speed tier is Fast, load, verify speed tier is Paused
9. **Frame-boundary save** — Request a save from inside a `SimulationTick` handler, verify SaveManager::Save runs at the next `SaveLoadModule::DoUpdate` and before the next `SimulationTick`
10. **Bus messages** — Subscribe to SaveCompleted and LoadCompleted, call save/load, verify events fire with correct slotId and result

### Error Cases

11. **Load nonexistent slot** — Call `cow.load missing`, verify LoadResult.error == FileNotFound
12. **Save to read-only directory** — Change baseDirectory permissions, call `cow.save readonly`, verify SaveResult.error == PermissionDenied
13. **Load corrupted save** — Manually corrupt a .sav file, call `cow.load corrupted`, verify LoadResult.error == ParseError

## Status

**Status:** `Approved`  
**Plan:** TBD (create when implementation starts)
