# System Spec: E1 Save/Load

**Parent:** @docs/specs/applications/cow/cow.md  
**Design source:** `Output Docs/GDD/10_PrototypeStrategy/E1_SaveContinue.md` (DesignDocs repo)

**Gameplay Domains:** persistence

## Summary

E1 is CoW's save/load orchestration shell. It provides the `SaveLoadModule`, which owns DiaSaveGame's `SaveManager` and `SaveRegistry`, defines CoW's save directory layout and slot naming, exposes DiaAPI commands (`cow.save <slot>`, `cow.load <slot>`, `cow.save.list`), and executes saves/loads at the sim-frame boundary. E1 owns *when* and *how* saves happen in CoW; DiaSaveGame owns the serialization mechanism.

**E1 does not decide what is saved.** The module ships empty. Each gameplay feature (GameSpeedController, RandomStreamModule, future A1/B/C/D systems) implements `ISaveable` and registers itself with `SaveLoadModule` in its own `DoStart`. E1 therefore has no dependency on the systems it saves, and nothing in it changes when a new participant lands. (Amended: previously E1 took GameSpeedController and RandomStreamModule in its constructor and owned their adapters. We do not yet know what A1/S-1/B/C will need to persist, so E1 is scoped to the shell.)

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
2. **Participant registry** — E1 owns the `SaveRegistry` and exposes `RegisterParticipant` / `UnregisterParticipant`; each CoW system registers its own `ISaveable` (no singleton, no service locator, no E1 knowledge of the participant types)
3. **Save directory layout** — CoW saves live under `out/saves/cow/`, slots named `slot_<id>.sav` (JSON format for debuggability in prototyping)
4. **Save orchestration** — Save/load only at the sim-frame boundary, where no system is mid-tick (see Frame-Boundary Save below); after saving or loading, fire a bus message so UI can react
5. **Atomic load** — If any participant fails to load, the whole load is aborted and the game is left unchanged (see Open Design Question 5)
6. **Supports F-2 Test 5** — F-2 Test 5 ("Save/Load Round-Trip") exercises `cow.save quick` / `cow.load quick` once GameSpeedController and RandomStream have registered their participants. Always-paused resume (A2 EC-03) and RNG continuation are proven by those features' own tests, not E1's.

## Non-Goals

1. **Autosave** — F-2 saves are manual (via command or bus message); autosave timers are deferred to G-1 or later
2. **Cloud sync** — Local disk only; no Steam Cloud, Xbox Live, or custom cloud backends
3. **Compression or encryption** — Raw JSON for F-2; binary format and compression are post-G-1 optimizations
4. **Save UI** — F-2 testbed uses command-line saves; UI slot picker deferred to V-3 or later
5. **Migration testing** — F-2 proves round-trip of current version only; cross-version migration testing deferred until post-G-1 when save format is stable
6. **Per-system save filtering** — Always full saves; no "quick save just the clock" or "autosave without economy"; all registered participants serialize every time

## Architecture

### Participant Registration

`SaveLoadModule` owns the `SaveRegistry` (as a member, not a singleton) and initialises the `SaveManager` in `DoStart()`. Participants register themselves:

```cpp
// SaveLoadModule
void SaveLoadModule::DoStart() { mSaveManager.Init(mConfig, mRegistry); }
void RegisterParticipant(StringCRC id, ISaveable* p)  { mRegistry.Register(id, p); }
void UnregisterParticipant(StringCRC id)              { mRegistry.Unregister(id); }

// A participating module (e.g. GameSpeedControllerModule, owned by that feature)
void GameSpeedControllerModule::DoStart() { mSaveLoad.RegisterParticipant("game_speed"_crc, &mSaveParticipant); }
void GameSpeedControllerModule::DoStop()  { mSaveLoad.UnregisterParticipant("game_speed"_crc); }
```

`SaveManager` holds the registry by pointer, so registration after `Init` is valid. Cap: `SaveRegistry::kMaxParticipants` (32).

**Registration order = serialization order**, which here means module start order on SimPU. GameSpeedController should start before RNG, and RNG before gameplay systems; `cow_main.diaapp` module ordering is the contract. If an explicit priority is ever needed, add it to `RegisterParticipant` then. Game time itself is saved by DiaSimTime's own `SimTimeSaveState`.

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
3. Record as pending; defer to the next frame boundary (see Frame-Boundary Save below)
4. Call SaveManager::Save("quick"_crc)
   - SaveManager calls Serialize() on each registered participant in order
   - Each participant writes its state to SaveContext
   - SaveManager writes manifest + data to disk as slot_quick.sav
5. Log "Save completed: slot_quick.sav" (or the SaveResult error)
```

**Load flow:**

```
1. User calls `cow.load quick`
2. SaveLoadModule receives DiaAPI command
3. Record as pending; defer to the next frame boundary
4. Call SaveManager::Load("quick"_crc)
   - SaveManager reads manifest + data from slot_quick.sav
   - SaveManager calls Deserialize() on each registered participant in order
   - Each participant restores its state from LoadContext
5. Log "Load completed: slot_quick.sav" (or the LoadResult error)
```

Resuming paused after a load (A2 EC-03) is GameSpeedController's own responsibility, enforced in its `RestoreState`. E1 does not publish speed-change messages.

**GameSpeedController participant (open item):** GameSpeedController is built (2026-10-13) but has no save participant yet; it waits on this module. When `SaveLoadModule` lands, its plan gets the task: register an `ISaveable` over `GameSpeedControllerModule::GetController().GetState()` (the day counter is the only persisted field) and restore through `GameSpeedControllerModule::RestoreState`. **Ordering rule:** the load must call `RestoreState` in the same frame as DiaSimTime's `SimTimeSaveState` clock restore, before the module's next update. If the world clock jumps without it, the module replays the jump as one `SimulationTick` per frame, even while paused. See the plan's "Known behaviours".

### Frame-Boundary Save

GameSpeedController publishes at most one `SimulationTick` per sim frame, and the bus fully dispatches it (Primary and Reaction passes) within that frame. So the start of the next sim frame, before GameSpeedController publishes the next day, is a point where no system is mid-tick.

- Save/load requests (DiaAPI command or the direct `Save`/`Load` API) are **recorded as pending**, never executed in the handler.
- `SaveLoadModule::DoUpdate` executes the pending request. In `cow_main.diaapp`, `SaveLoadModule` is ordered **before** `GameSpeedControllerModule` on SimPU.
- No `TickCompleted` message is needed. It was dropped because Reaction-pass handlers can't post, so a same-update "all subscribers finished" message is impossible.

**Sync vs Async:** F-2 uses synchronous I/O (save/load block the sim thread). DiaSaveGame supports async I/O (Feature 7), but E1 defers it until S-7 Endurance testbed (when save times matter).

### Participants (owned by other features)

E1 ships no `ISaveable` implementations. The features that own the state add them, each registering via `RegisterParticipant` (see above):

- **GameSpeedController**: day counter, sub-day accumulator (`acc_us`) and `preModalSpeedTier` (participant version 2; v1 saves with the day only still load). Speed tier, pause source and modal depth reset on load (A2 EC-03); `RestoreState` always resumes paused.
- **RandomStreamModule**: master seed + per-stream state.
- **Future (A1, S-1 world, B/C layers)**: decided when those systems are specced. Save granularity, format and size for the A1 world are open (see Open Design Questions).

### Bus Messages (deferred)

`SaveRequested` / `SaveCompleted` / `LoadRequested` / `LoadCompleted` are not built. Nothing consumes them until a UI exists (V-4); the commands cover F-2, and `SaveManager`'s own observers (`OnSaveCompleted`, `OnLoadCompleted`) are available if a test needs a completion signal. Add them when a consumer appears.

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
    SaveLoadModule() = default;

    void DoStart() override;
    void DoUpdate() override;   // executes pending save/load at the frame boundary
    void DoStop() override;

    // Participant registration (called by the owning feature's module)
    void RegisterParticipant  (Dia::Core::StringCRC id, Dia::SaveGame::ISaveable* participant);
    void UnregisterParticipant(Dia::Core::StringCRC id);

    // Direct API (alternative to DiaAPI commands); queued to the next DoUpdate
    void RequestSave(Dia::Core::StringCRC slotId);
    void RequestLoad(Dia::Core::StringCRC slotId);

    void EnumerateSlots(Dia::Core::Containers::DynamicArrayC<Dia::SaveGame::SlotInfo, 32>& out) const;

private:
    Dia::SaveGame::SaveConfig   mConfig;
    Dia::SaveGame::SaveRegistry mRegistry;
    Dia::SaveGame::SaveManager  mSaveManager;
    Dia::Core::StringCRC        mPendingSave;
    Dia::Core::StringCRC        mPendingLoad;
};

} // namespace CoW
```

## Features

| # | Feature | Description | Status |
|---|---------|-------------|--------|
| 1 | SaveLoadModule Core | SaveConfig, SaveRegistry, `RegisterParticipant`/`UnregisterParticipant`, `cow.save`/`cow.load`/`cow.save.list`, atomic-load error handling | Approved |
| 2 | Frame-Boundary Save | Requests recorded as pending and executed in `SaveLoadModule::DoUpdate`, ordered before `GameSpeedControllerModule` (replaces `TickCompleted` synchronization) | Approved |
| 3 | Save/Load Bus Messages | SaveRequested, SaveCompleted, LoadRequested, LoadCompleted | Deferred (no consumer until V-4) |
| 4 | GameSpeedController Serialization | Day counter participant, version 1 | Moved to GameSpeedController's plan |
| 5 | RandomStream Serialization | Master seed + per-stream state, version 1 | Moved to RandomStreamModule's plan |

> **Note:** Features 1-2 are E1's build scope and ship as one unit. Features 4-5 are owned by the systems that hold the state and register through E1; F-2 Test 5 depends on them. Future systems (A1, B-layer, etc.) add their own participants as they land.

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

0. **Save contents, granularity and format for the A1 world** — "Always full saves" and JSON hold for the day counter and RNG, but a 200x200-cell A1 world may not fit. S-1 already has its own JSON export/import, so whether the world lives inside the save or in a separate file is undecided.
   - **Recommendation:** Leave open. These are `SaveConfig` choices, so the shell does not change. Decide when A1 is specced; S-7 is the stress point.

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
   - **Amended (built 2026-10-14):** atomic load is only partly met. `ISaveable::Deserialize` returns `void` and `SaveManager::Load` restores participants one at a time with no validate step, so a failure *inside* a participant cannot be rolled back. `SaveLoadModule` pre-validates before calling `Load` (slot exists, file parses, manifest readable and engine-compatible) and leaves the game untouched on any of those failures. Participants skip a stream or field they cannot read, and log an error. Spec Test 11 (participant fails mid-load) is **not covered**. Closing the gap needs a `Validate(LoadContext&)` step on `ISaveable` (a Dia change), to be made when a participant with real failure modes lands (A1).

## Dependencies

**Requires:**
- **DiaSaveGame** (ISaveable, SaveManager, SaveRegistry, SaveConfig, SaveContext/LoadContext)
- **GameSpeedController** (module ordering only: at most one `SimulationTick` per sim frame makes the frame boundary a safe save point; E1 has no code dependency on it)
- **DiaAPI** (command registration for `cow.save`, `cow.load`, `cow.save.list`)
- **DiaCore** (StringCRC, DynamicArrayC, file I/O, DIA_LOG_*)

**Required by:**
- **F-2 Core Runtime testbed** (Test 5: Save/Load Round-Trip)
- **All future testbeds** (S-1 through G-1 all assume saves work)
- **V-3 UI** (slot picker UI depends on EnumerateSlots)
- **S-7 Endurance** (history paging and long-run saves depend on robust save/load)

**Circular dependency risk:** None. Participating modules depend on E1 (to register); E1 depends on none of them.

## Test Strategy

E1 correctness is proven primarily by **F-2 Core Runtime testbed, Test 5: Save/Load Round-Trip**.

### Unit Tests (GoogleTests/CoW/SaveLoad), using a fake `ISaveable`

1. **SaveConfig initialization**: baseDirectory, slotPattern, format defaults
2. **Participant registration order**: register fakes A, B; serialization order matches registration order
3. **Unregister**: an unregistered participant is no longer serialized
4. **Round trip**: save, mutate fake state, load, state restored
5. **Request deferral**: `RequestSave` does nothing until `DoUpdate`, then saves exactly once
6. **Frame-boundary**: a request made from inside a handler runs at the next `DoUpdate`
7. **Slot enumeration**: 3 saves listed with correct ids

### Error Cases

8. **Load nonexistent slot**: FileNotFound, game state unchanged
9. **Save to unwritable directory**: error result, previous save intact
10. **Load corrupted save**: ParseError, game state unchanged
11. **Participant fails mid-load**: whole load aborted, no hybrid state

### Integration (owned by other plans)

GameSpeedController and RandomStream plans prove their own participants (always-paused resume, RNG continuation). F-2 Test 5 proves the end-to-end round trip.

## Status

**Status:** `Done` (features 1-2; 4-5 delivered through the participant owners)  
**Plan:** @docs/specs/applications/cow/systems/e1-saveload/e1-saveload.plan.md

### As built (2026-10-14)

- `SaveLoadModule` ships as specced, plus config override `{"base_directory": ...}` and `OnConnectStreams` injecting its registry into `DiaSimTimeModule` (instance id must be `DiaSimTime`) so the world clock and scheduler save as participant `DiaSimTime`.
- Participants: `game_speed` (day counter, restores paused), `random_streams` (per stream seed + draw count, replayed on restore), `DiaSimTime` (clock, scheduler, sleep state). Each registers lazily from its own `DoUpdate` once a `SaveLoadModule` exists; with none, nothing changes.
- Dia changes made for this (user-approved): `RandomStream::GetSeed/GetDrawCount/Restore`; `SlotManager::EnumerateSlots` now returns the id passed to `Save`; `TimeServer::SetTime` / `SimTimeDomain::SetTime` (rewind-capable) used by `SimTimeSaveState::Deserialize`; `TimeServer::SetTimeScaleImmediate` / `SimTimeDomain::SetScaleImmediate` so a restore pauses at once. All restore-only APIs.
- Hardening from the test gap analysis (2026-10-14, user-approved Dia changes): saves are atomic (temp file + replace) and the buffer is 10 MB; slot ids are validated (`SlotManager::IsValidSlotId`, `InvalidSlotId` result codes) at the SaveManager and module boundaries; loading replaces the scheduler's pending entries (`SimTimeScheduler::Clear`); `PreValidateLoad` also rejects missing migration steps and newer-than-live participant versions before any participant restores; duplicate / over-cap / unknown participant registration is rejected without a fatal assert; the manifest timestamp is set at save time (64-bit Unix seconds) and `SlotInfo::timestamp` / `cow.save.list` show the slot's last-write time; `SaveLoadHealthReporter` (Degraded after a failed save/load, Failing if the save dir is unavailable or 3 saves in a row fail).
- Limits: no live in-game smoke run of `cow_main.diaapp` yet (CoW has no headless launch); a load while a modal is open resets `modalDepth` to 0, so a speed change while that modal is still showing would unpause; a downgrade reports `MigrationError` (no dedicated code).
- Module order on SimPU in `cow_main.diaapp`: DiaSimTime, SaveLoadModule, GameSpeedControllerModule, RandomStreamModule.
