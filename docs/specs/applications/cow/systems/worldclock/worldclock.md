# System Spec: WorldClock

**Parent:** @docs/specs/applications/cow/cow.md  
**Design source:** `Output Docs/GDD/10_PrototypeStrategy/F2_CoreRuntime.md`, `Output Docs/GDD/08_SystemsDeepDive/A2_WorldClock/` (DesignDocs repo)

**Gameplay Domains:** simulation

## Summary

WorldClock is CoW's simulation time system. It owns the season counter (CoW's primary time unit), speed control (PAUSED/NORMAL/FAST/FASTEST), pause state with modal nesting, and time band resolution (SEASON/YEAR/DECADE/CENTURY). It is the sole broadcaster of `SimulationTick` to the entire game — every CoW gameplay system (ecology, dragons, clans, dynasty) reacts to the ticks WorldClock produces, but WorldClock decides nothing about *what happens* on those ticks.

WorldClock is a thin CoW-specific layer over DiaSimTime. DiaSimTime provides the deterministic game clock foundation (tick counter, pause/resume, save/load, event scheduling); WorldClock adds CoW's calendar system (seasons as the primary unit, year/decade/century bands), speed tier UI vocabulary (PAUSED/NORMAL/FAST/FASTEST mapped to Hz values), and modal-pause nesting rules.

## Problem

CoW is a centuries-long simulation where:
- The player must control pacing (pause to think, fast-forward through quiet stretches)
- The UI can interrupt at any moment (decree menu, lineage inspector) and the sim must not advance silently while the player is mid-decision (modal auto-pause)
- Time is CoW's core resource — seasons accumulate into years, decades, centuries; different systems care about different bands (ecology runs every season, genetics drift triggers on decades)
- Multiple independent time domains coexist: simulation time (deterministic, pausable, saved), wall-clock time (UI animations, never saved), render time (interpolates smooth visuals), future audio time (ambient soundscape)
- All of this must round-trip through save/load with deterministic replay

Without a unified time architecture, each system would implement its own pause logic, speed control, and calendar math — leading to desync, modal bugs, and save/load drift.

## Goals

1. **Single source of simulation time** — `season_counter` is the one true clock; all gameplay systems react to `SimulationTick`, never poll `GetCurrentSeason()`
2. **Speed control** — Player selects PAUSED/NORMAL/FAST/FASTEST; WorldClock maps these to Hz values (0 / 2 / 5 / 10 Hz) and drives the SimPU tick rate
3. **Modal-safe pause** — Any `ModalOpened` event auto-pauses the sim with nesting-safe bookkeeping (modal_depth counter, pre-modal speed restoration)
4. **CoW calendar** — Time band resolution (SEASON/YEAR/DECADE/CENTURY) from modulo arithmetic (season % 4/40/400), published with every `SimulationTick`
5. **Deterministic save/load** — season_counter, speed_tier, pause_source, modal_depth round-trip exactly; always resume paused (GDD A2 rule)
6. **Clear time domain boundaries** — Document which clocks are independent (sim vs wall vs render vs audio), so systems use the right clock for their needs

## Non-Goals

1. **Deciding what happens on a tick** — WorldClock emits `SimulationTick{season, time_band}`; subscribers (A1 ecology, B-layer dragons, C-layer clans) own their reactions entirely
2. **Per-entity time scaling** — No slow-motion for individual dragons or hearths; speed control is global only
3. **Network synchronization** — CoW is single-player; no lockstep, no rollback
4. **Time travel / replay** — F-2's determinism tests prove replay is *possible*, but a replay UI is out of scope
5. **Audio time implementation** — A-1 (future) will implement time-speed-aware audio; WorldClock just documents the audio time domain
6. **Editor time** — CluicheEditor runs on wall-clock time, independent of CoW's sim clock

## Architecture

### Time Domain Hierarchy

CoW has **four independent time domains**, each with its own clock and use case:

```
┌─────────────────────────────────────────────────────────────┐
│ 1. Simulation Time (WorldClock)                             │
│    - Deterministic, pausable, saved                         │
│    - Unit: season_counter (uint64, 0 at New Game)          │
│    - Drives: A1 ecology, B-layer dragons, C-layer clans    │
│    - Source: DiaSimTime (SimPU Hz * timeScale)             │
│    - Speed control: Player selects tier → WorldClock sets Hz│
└─────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│ 2. Wall-Clock Time                                          │
│    - Always runs, never paused, not saved                   │
│    - Unit: real seconds (float deltaTime)                   │
│    - Drives: UI animations, menu transitions, input polling │
│    - Source: DiaSimTime MainPU wall-clock tick              │
│    - Independent of sim pause/speed                         │
└─────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│ 3. Render Time                                              │
│    - Interpolates between sim ticks for smooth visuals     │
│    - Unit: fractional position between tick N and N+1       │
│    - Drives: Dragon movement lerp, animation blend          │
│    - Source: DiaSimTime RenderPU reads sim FrameStream     │
│    - Runs even when sim is paused (shows last sim state)   │
└─────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│ 4. Audio Time (A-1, future)                                 │
│    - Ambient: wall-clock (never paused)                     │
│    - Events: triggered by sim, played at wall-clock rate    │
│    - Time-speed-aware: pitch/tempo adjust at FAST/FASTEST? │
│    - Source: Wall-clock + sim event triggers                │
└─────────────────────────────────────────────────────────────┘
```

**Key insight:** These domains are **enforced independent** by Dia's PU architecture (SimPU / MainPU / RenderPU run on separate threads with separate clocks). WorldClock only controls domain 1 (Simulation Time); the others reference it but don't block on it.

### CoW Calendar System

CoW's time unit hierarchy:

```
1 Season  = 1 sim tick (base unit)
1 Year    = 4 seasons
1 Decade  = 40 seasons (10 years)
1 Century = 400 seasons (100 years, 10 decades)
```

Time band resolution (GDD A2 §2):

```cpp
enum class TimeBand : uint8_t {
    SEASON,   // default, every tick
    YEAR,     // season_counter % 4 == 0
    DECADE,   // season_counter % 40 == 0
    CENTURY   // season_counter % 400 == 0
};

TimeBand ResolveBand(uint64_t season_counter) {
    if (season_counter % 400 == 0) return TimeBand::CENTURY;
    if (season_counter % 40  == 0) return TimeBand::DECADE;
    if (season_counter % 4   == 0) return TimeBand::YEAR;
    return TimeBand::SEASON;
}
```

Published with every `SimulationTick` so subscribers can gate work by band (e.g., genetics drift only triggers on DECADE ticks).

### Speed Tier Mapping (Civ-style Hz approach)

```cpp
enum class SpeedTier : uint8_t {
    PAUSED,   // 0 Hz   - no ticks
    NORMAL,   // 2 Hz   - 500ms per season (default feel)
    FAST,     // 5 Hz   - 200ms per season (2.5x speed)
    FASTEST   // 10 Hz  - 100ms per season (5x speed)
};

constexpr float SPEED_TIER_HZ[] = {
    0.0f,   // PAUSED
    2.0f,   // NORMAL
    5.0f,   // FAST
    10.0f   // FASTEST
};
```

**Implementation:** `WorldClockModule` calls `ProcessingUnit::SetFrequencyHz(SPEED_TIER_HZ[tier])` when `SpeedChangeRequested` arrives. No timeScale needed — each tick is always exactly 1 season.

**Design rationale (Civ pattern):**
- Simple Hz change, no fractional-tick accumulation
- Easy to tune feel in playtesting (just tweak the Hz array)
- Matches GDD A2's `SEASON_MS[speed_tier]` array (500ms = 2 Hz)
- DiaSimTime's fixed-timestep accumulator (ST-011) handles catch-up if frame rate drops below Hz

### Pause State & Modal Nesting

WorldClock tracks three pause-related fields (GDD A2 §1):

```cpp
struct WorldClockState {
    uint64_t    season_counter;          // 0 at New Game, +1 per tick
    SpeedTier   speed_tier;              // current speed
    PauseSource pause_source;            // NONE | PLAYER | MODAL
    uint8_t     modal_depth;             // count of open modals (nesting-safe)
    SpeedTier   pre_modal_speed_tier;   // remembered for restore
};

enum class PauseSource : uint8_t {
    NONE,      // sim is running
    PLAYER,    // player pressed pause
    MODAL      // UI modal forced pause
};
```

**Modal nesting rules (GDD A2 §5):**

```
on ModalOpened():
    if modal_depth == 0 and speed_tier != PAUSED:
        pre_modal_speed_tier = speed_tier
        speed_tier = PAUSED
        pause_source = MODAL
    modal_depth += 1

on ModalClosed():
    modal_depth = max(0, modal_depth - 1)
    if modal_depth == 0 and pause_source == MODAL:
        speed_tier = pre_modal_speed_tier
        pause_source = NONE
    // if pause_source == PLAYER, closing the modal does NOT auto-resume
```

**Key invariant:** Player pause always wins. A modal closing never overrides a player-requested pause.

### Save/Load Behavior

**Persisted by E1 (via `WorldClockState` serialization):**
- `season_counter` (primary clock value)
- `speed_tier` (but always loads as PAUSED, see below)
- `pause_source` (reconstructed as PLAYER on load)
- `modal_depth` (always 0 on load — no modals persist across sessions)
- Pending `SimTimeScheduler` events (via DiaSimTime's `SimTimeSaveState`)

**Load behavior (GDD A2 §6, E1 §4):**
```cpp
on GameLoaded(saved_state):
    season_counter = saved_state.season_counter
    speed_tier = PAUSED        // always resume paused (A2 edge case EC-03)
    pause_source = PLAYER
    modal_depth = 0            // no modals across sessions
```

**Why always PAUSED on load:** Prevents the sim from racing ahead before the player is ready. Player must explicitly resume after inspecting the loaded state.

## Public Interfaces

### Messages (DiaMessageBus)

**Published by WorldClock:**

```cpp
// Broadcast to all gameplay systems every sim tick
struct SimulationTick {
    uint64_t  season;      // current season_counter value
    TimeBand  time_band;   // SEASON | YEAR | DECADE | CENTURY
};

// Published after all SimulationTick subscribers finish
// E1 waits on this before serializing (guarantees no system is mid-tick)
struct TickCompleted {
    uint64_t  season;
};
```

**Subscribed by WorldClock:**

```cpp
// From UI-I (speed control buttons)
struct SpeedChangeRequested {
    SpeedTier  tier;   // PAUSED | NORMAL | FAST | FASTEST
};

// From UI-I (single-step button, only valid while PAUSED)
struct ManualAdvanceRequested {
    // no payload — advances season_counter by 1, publishes one SimulationTick
};

// From UI system (any modal window opening)
struct ModalOpened {
    // no payload — increments modal_depth, may auto-pause
};

// From UI system (any modal window closing)
struct ModalClosed {
    // no payload — decrements modal_depth, may auto-resume
};

// From SYS settings system (preferred default speed for New Game)
struct SettingsChanged {
    StringCRC  key;
    // ... payload varies by key
};
```

### C++ API (Read-Only Query)

**WorldClockModule public accessors:**

```cpp
namespace CoW {

    class WorldClockModule : public Dia::ApplicationFlow::SimModule {
    public:
        // Current state (read-only)
        uint64_t    GetSeasonCounter() const;
        TimeBand    GetCurrentBand() const;
        SpeedTier   GetSpeedTier() const;
        bool        IsPaused() const;  // true if speed_tier == PAUSED
        PauseSource GetPauseSource() const;

        // CoW calendar helpers
        uint64_t    GetCurrentYear() const;     // season_counter / 4
        uint64_t    GetCurrentDecade() const;   // season_counter / 40
        uint64_t    GetCurrentCentury() const;  // season_counter / 400
    };

} // namespace CoW
```

**Usage note:** These are debugging/UI queries only. **Gameplay systems must never poll the clock** — they subscribe to `SimulationTick` and react to events. Polling breaks determinism (query timing isn't replayed).

### Dia Integration Points

**WorldClockModule uses DiaSimTime:**
- Receives `SimTimeContext` each tick (from SimPU's fixed-timestep accumulator)
- Increments `season_counter` once per `DoUpdate(SimTimeContext)` call
- Calls `ProcessingUnit::SetFrequencyHz()` to change tick rate when speed changes
- Publishes `SimTimeContext { gameTime, gameDt, tick }` via DiaStreams for RenderPU interpolation (DiaSimTime already does this — WorldClock just forwards it)

**WorldClockModule does NOT use:**
- `SimTimeDomain::SetTimeScale()` — speed control is Hz-based, not timeScale
- `SimTimeScheduler` directly — gameplay systems schedule their own events; WorldClock just ensures the scheduler advances

## Features

| # | Feature | Description | Spec | Status |
|---|---------|-------------|------|--------|
| 1 | WorldClockModule Core | `WorldClockState`, season counter, time band resolution, Hz-based speed tiers, message handlers | TBD | Approved |
| 2 | Modal Pause Nesting | `modal_depth` tracking, auto-pause/resume with pre-modal speed restoration | TBD | Approved |
| 3 | Save/Load Integration | `WorldClockState` serialization via E1, always-paused load behavior | TBD | Approved |
| 4 | Speed Control UI | UI-I integration — speed buttons, pause button, single-step button (F-2 testbed proves this) | TBD | Approved |

> **Note:** Features 1-3 are tightly coupled (all live in `WorldClockModule`); they ship together as one unit. Feature 4 is UI work that uses the WorldClock messages; it's CoW-side but separate from the core module.

## Inherited Binding Decisions

| ID | Decision | How it applies to WorldClock |
|----|----------|------------------------------|
| PD-001 | Use StringCRC for entity/component IDs | Message types (SpeedChangeRequested, SimulationTick, etc.) are StringCRC-identified in DiaMessageBus registry |
| PD-002 | ProcessingUnit/Phase/Module architecture | WorldClockModule is a proper SimModule on SimPU; no singletons or static state |
| PD-004, AD-002 | No STL containers in public APIs | All public APIs use primitive types (uint64_t, SpeedTier enum); no vectors/maps exposed |
| PD-007 | C++20 required language standard | `enum class` for SpeedTier/TimeBand/PauseSource; `constexpr` for SPEED_TIER_HZ array |
| AD-001 | CoW develops on cow branch with ask-permission for Dia edits | WorldClock is pure CoW code (Cluiche/CoW/); DiaSimTime edits require sign-off |
| AD-002 | Four-tier dependency model | WorldClock is CoW-Shared tier (used by all testbeds S-1 through G-1); not generic enough for Dia |
| AD-003 | GDD owns design intent, spec tree owns implementation | GDD A2 folder owns *what* must be proven (speed tiers, modal pause, time bands); this spec owns *how* it's built |
| AD-004 | Promote BootMenuModule to baseline | Not applicable — WorldClock is gameplay infrastructure, not boot/menu |
| AD-005 | One stage per testbed | WorldClock is a module used by testbeds, not a testbed itself |

## Open Design Questions

1. **SEASON_MS tuning** — GDD A2 specifies 500ms (NORMAL), but exact values for FAST/FASTEST are "tuned in playtesting." Should we start with 2/5/10 Hz (as proposed), or different ratios?
   - **Recommendation:** Start with 2/5/10 Hz. F-2 testbed proves determinism at all speeds; playtesting tunes feel from there.

2. **Manual advance batch size** — `ManualAdvanceRequested` currently advances 1 season. Should there be a `ManualAdvanceBatch(count)` variant for debugging (advance 10 seasons at once)?
   - **Recommendation:** Defer. Single-step is sufficient for F-2 tests; a batch variant can be added if debuggers actually need it.

3. **Calendar display format** — CoW UI will show "Year 3, Season 2" or "Decade 4." Does WorldClock provide a formatting helper (`FormatCalendar() -> string`), or is that UI-layer code?
   - **Recommendation:** UI-layer. WorldClock provides `GetCurrentYear()` etc.; UI constructs the string. Keeps localization out of WorldClock.

4. **Audio time-speed awareness** — A-1 (future) mentions "time-speed-aware audio." Should ambient soundscape pitch-shift at FAST/FASTEST (chipmunk effect), or stay normal?
   - **Recommendation:** Defer to A-1 spec. WorldClock documents the audio time domain; A-1 decides the feel.

5. **SimTimeDomain usage** — DiaSimTime provides `SimTimeDomain` for sub-clock hierarchies (e.g., a "dragon metabolism" clock that runs slower). Should WorldClock expose this to CoW systems, or keep it internal?
   - **Recommendation:** Keep internal for now. CoW's speed control is global-only (Non-Goal 2). If a future system needs a sub-clock, revisit.

## Dependencies

**Requires:**
- **DiaSimTime** (SimTimeContext, SimModule base class, ProcessingUnit Hz control, save/load via SimTimeSaveState)
- **DiaMessageBus** (Primary/Reaction passes, message publishing/subscription)
- **DiaStreams** (optional: publish WorldClockContext stream for RenderPU interpolation — may already be covered by DiaSimTime's FrameStream)
- **DiaCore** (StringCRC, DIA_LOG_*, DIA_ASSERT)

**Required by:**
- **A1** (ecology subscribes to SimulationTick, uses time_band to gate work)
- **E1** (save/load subscribes to TickCompleted)
- **All B-layer** (dragons), **C-layer** (clans), **D-layer** (dynasty) systems
- **V-1** (visuals may use time_band for day/night cycle, weather)
- **UI-I** (speed control, pause button)

**Circular dependency risk:** WorldClock depends on DiaMessageBus; E1 depends on WorldClock (TickCompleted). But E1 also depends on DiaMessageBus. No actual cycle — all three are independent modules on SimPU, wired by messages.

## Test Strategy

WorldClock correctness is proven primarily by **F-2 Core Runtime testbed** (GDD F2_CoreRuntime.md):

### Unit Tests (GoogleTests/CoW/WorldClock)

1. **Time band resolution** — `ResolveBand(0) == SEASON`, `ResolveBand(4) == YEAR`, `ResolveBand(40) == DECADE`, `ResolveBand(400) == CENTURY`
2. **Speed tier Hz mapping** — Verify `SPEED_TIER_HZ[PAUSED] == 0.0`, `SPEED_TIER_HZ[NORMAL] == 2.0`, etc.
3. **Modal nesting** — Open 3 modals, close 2, verify sim stays paused and `modal_depth == 1`
4. **Player pause wins** — Player pauses, modal opens/closes, verify sim stays paused (pause_source == PLAYER)
5. **Pre-modal speed restoration** — Start at FAST, modal pauses, close modal, verify speed returns to FAST

### Integration Tests (F-2 testbed, Test 4: Clock Proof)

6. **All four speed tiers** — Exercise PAUSED/NORMAL/FAST/FASTEST on a live stage, confirm Hz matches
7. **Nested modal auto-pause** — Open two modals, close one, confirm sim stays paused; close second, confirm auto-resume
8. **Manual single-step** — Pause, call `ManualAdvanceRequested`, confirm exactly 1 `SimulationTick` published

### Determinism Tests (F-2 testbed, Tests 2 & 3)

9. **Speed-independent determinism** — Run same seed at NORMAL, then FAST, confirm identical `season_counter` progression after N ticks
10. **Save/load clock round-trip** — Save at season 1000, reload, verify `season_counter == 1000` and `speed_tier == PAUSED`

## Status

**Status:** `Approved`  
**Plan:** TBD (create when implementation starts)
