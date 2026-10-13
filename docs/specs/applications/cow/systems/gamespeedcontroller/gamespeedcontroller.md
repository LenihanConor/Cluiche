# System Spec: GameSpeedController

**Parent:** @docs/specs/applications/cow/cow.md  
**Design source:** `Output Docs/GDD/08_SystemsDeepDive/A2_WorldClock/` (DesignDocs repo; GDD name "A2 World Clock & Time Bands")

**Gameplay Domains:** simulation

## Summary

GameSpeedController lets the player run CoW's game clock faster or slower, EU4/Civ style: Pause plus four named speed presets whose values come from config. It does not own the clock — DiaSimTime does. It drives DiaSimTime's world time scale and pause, turns the passage of game time into discrete **day** ticks (`SimulationTick{day, band}`), and enforces the pause rules (player pause, nesting-safe modal auto-pause, single-step).

Every CoW gameplay system advances on `SimulationTick`. Systems that work at a coarser cadence (A4 weekly, A1 seasonal) gate on the tick's band. GameSpeedController decides *when* a day passes, never *what happens* in it.

## Goals

1. **Speed control** — Pause + four presets (Slow / Normal / Fast / Fastest). Changing speed changes how fast game time flows; the SimPU keeps its fixed tick rate.
2. **Presets from config** — Wall-clock seconds per day for each preset are loaded from the module's `.diaapp` config block, not compiled in.
3. **Day ticks** — Each time game time crosses a day boundary, publish exactly one `SimulationTick` with its band (DAY / WEEK / SEASON / YEAR / DECADE / CENTURY).
4. **Pause rules** — Player pause, nesting-safe modal auto-pause, player pause always wins, single-step while paused.
5. **New Game / Load state** — New Game starts at day 0, paused. Load restores the day and always comes back paused. State is exposed as a plain struct; E1 owns the save file.
6. **Speed-independent determinism** — The same inputs produce the same simulation regardless of which speed it ran at.

## Non-Goals

- **Owning the clock** — DiaSimTime owns game time, pause, step, and saving the world clock. GameSpeedController only steers it.
- **Speed/pause/step UI** — Buttons live in F-2 (Test 4 driver) now, UI-I later. They publish the messages below.
- **Save-file plumbing** — E1 serializes the state struct (E1 Feature 2).
- **A "tick finished" message** — Not needed: E1 saves at the sim-frame boundary (see E1).
- **Per-entity or per-subsystem time scaling** — Speed is global.
- **Calendar text** ("Year 3, Spring, Day 12") — UI-layer.
- **Batch step, audio pitch at speed, settings-driven default speed** — Deferred until something needs them.
- **Runtime SimPU frequency change** — Not needed; speed is a time scale, not a tick rate.

## Architecture

### How speed works

```
SimPU (fixed rate, 30 Hz in cow_main.diaapp)
  └─ world SimTimeDomain           ← DiaSimTime (owns game time)
       scale = kGameTimePerDay / preset.wallSecondsPerDay
       Pause() / Resume() / Step()
  └─ GameSpeedControllerModule     ← this system
       each sim frame: if game time crossed the next day boundary
           → dayCounter += 1, publish SimulationTick{day, ResolveBand(day)}
```

- `kGameTimePerDay` is a fixed constant: **1 game-second = 1 day**. A `SimTimeScheduler` deadline of `+7.0s` therefore means "in one week".
- DiaSimTime stores time as integer microseconds, so accumulation is exact and deterministic.
- Paused = `SimTimeDomain::Pause()`. Game time stops, SimPU keeps ticking (UI, debug, and rendering still run).
- Manual step = `SimTimeDomain::Step(time to next day boundary)`, which produces exactly one `SimulationTick` on the next sim frame.

### Determinism rule

**Gameplay state may change only in response to `SimulationTick` (or other bus messages), never by integrating per-frame `gameDt`.** Per-frame `gameDt` is for presentation only (interpolation, animation). This is what makes the simulation identical at every speed: speed changes how many sim frames fall between days, not what happens in a day.

### At most one tick per sim frame

GameSpeedController publishes **at most one** `SimulationTick` per sim frame, so each day's Primary and Reaction bus passes finish before the next day starts. That is also what makes the frame boundary a safe save point for E1.

Config validation asserts that the Fastest preset advances less than one day per sim frame (`wallSecondsPerDay > 1 / SimPU Hz`, i.e. > 0.033 s at 30 Hz), so a boundary can never be skipped. **This caps top speed at SimPU Hz days per second** (30 days/s, roughly 3 s per season, 20 min per century). Accepted; see Resolved Design Decisions.

### Calendar

Fixed constants, not tunable. The week and season sizes match A4 Weather (52 weeks/year) and A1 (13 weeks/season):

| Unit | Size | Days |
|---|---|---|
| Day | base tick | 1 |
| Week | 7 days | 7 |
| Season | 13 weeks | 91 |
| Year | 4 seasons | 364 |
| Decade | 10 years | 3,640 |
| Century | 10 decades | 36,400 |

```cpp
enum class TimeBand : uint8_t { Day, Week, Season, Year, Decade, Century };   // ordered smallest → largest

// Largest unit whose boundary this day completes.
TimeBand ResolveBand(uint64_t day);   // %36400 → Century, %3640 → Decade, %364 → Year, %91 → Season, %7 → Week, else Day

uint64_t WeekOf(uint64_t day);     // day / 7
uint64_t SeasonOf(uint64_t day);   // day / 91
uint64_t YearOf(uint64_t day);     // day / 364
```

Subscribers gate with ordered comparison. For example, A1 runs its seasonal pipeline when `band >= TimeBand::Season`, and A4 updates weather when `band >= TimeBand::Week`. Day 0 never produces a tick. The first tick is day 1 (GDD EC-05).

### Speed presets (config)

```cpp
enum class SpeedTier : uint8_t { Paused, Slow, Normal, Fast, Fastest };

struct SpeedPresetConfig {
    float wallSecondsPerDay[4];   // Slow, Normal, Fast, Fastest
};
```

Loaded from the module's `"config"` block in `cow_main.diaapp`. This is the CoW convention; `BootMenuModule` uses the same mechanism:

```json
{
    "instance_id": "GameSpeedControllerModule",
    "type_id": "GameSpeedControllerModule",
    "stages": ["all"],
    "config": {
        "wall_seconds_per_day": { "slow": 60.0, "normal": 30.0, "fast": 10.0, "fastest": 1.0 }
    }
}
```

The values shown are **placeholders** to be tuned in playtesting. Validation on load: every value > 0, strictly decreasing Slow→Fastest, Fastest satisfies the one-tick-per-frame rule. A missing or invalid config logs an error and falls back to the compiled defaults.

### Pause state

```cpp
enum class PauseSource : uint8_t { None, Player, Modal };

struct GameSpeedState {
    uint64_t    dayCounter;          // 0 at New Game, +1 per tick, never decreases
    SpeedTier   speedTier;
    PauseSource pauseSource;
    uint8_t     modalDepth;          // nesting-safe count of open modals
    SpeedTier   preModalSpeedTier;   // restored when modalDepth returns to 0
};
```

Rules (GDD A2 §4–§6, EC-01 to EC-06):

```
SpeedChangeRequested(tier):
    if modalDepth > 0:                       // added rule, see GDD deltas
        if tier == Paused: pauseSource = Player
        else:              preModalSpeedTier = tier
        return                               // stay paused while a modal is open
    if tier == Paused: speedTier = Paused; pauseSource = Player
    else:              speedTier = tier;   pauseSource = None   // selecting a speed is a resume
    // same tier again = no-op (EC-04)

ManualAdvanceRequested():
    if speedTier != Paused: reject + log      // EC-06
    else: step to next day boundary

ModalOpened():
    if modalDepth == 0 and speedTier != Paused:
        preModalSpeedTier = speedTier; speedTier = Paused; pauseSource = Modal
    modalDepth += 1

ModalClosed():
    modalDepth = max(0, modalDepth - 1)
    if modalDepth == 0 and pauseSource == Modal:
        speedTier = preModalSpeedTier; pauseSource = None
    // a Player pause is never resumed by a modal closing (EC-01)

WorldGenerationComplete():   dayCounter = 0; speedTier = Paused; pauseSource = Player; modalDepth = 0
RestoreState(saved):         dayCounter = saved.dayCounter; speedTier = Paused; pauseSource = Player; modalDepth = 0   // EC-03
```

## Public Interfaces

### Messages (DiaMessageBus)

| Direction | Message | Payload |
|---|---|---|
| Publishes | `SimulationTick` | `uint64_t day; TimeBand band;` |
| Subscribes | `SpeedChangeRequested` | `SpeedTier tier;` (GDD's `PauseRequested` = `SpeedChangeRequested{Paused}`) |
| Subscribes | `ManualAdvanceRequested` | — |
| Subscribes | `ModalOpened` / `ModalClosed` | — |
| Subscribes | `WorldGenerationComplete` | (published by S-1 WLD) |

### C++

```cpp
namespace CoW {

    // Pure logic, no Dia module dependencies. Unit-tested directly.
    class GameSpeedController {
    public:
        explicit GameSpeedController(const SpeedPresetConfig& config);

        void OnSpeedChangeRequested(SpeedTier tier);
        bool OnManualAdvanceRequested();         // false if rejected (not paused)
        void OnModalOpened();
        void OnModalClosed();
        void OnNewGame();

        float  GetTimeScale() const;              // 0 when paused
        bool   IsPaused() const;
        const GameSpeedState& GetState() const;
        void   RestoreState(const GameSpeedState& saved);   // always resumes paused
    };

    TimeBand ResolveBand(uint64_t day);
    uint64_t WeekOf(uint64_t day);
    uint64_t SeasonOf(uint64_t day);
    uint64_t YearOf(uint64_t day);

    // SimModule wiring: config load, bus subscriptions, drives the world SimTimeDomain,
    // detects day boundaries, publishes SimulationTick.
    class GameSpeedControllerModule : public Dia::ApplicationFlow::SimModule {
    public:
        const GameSpeedController& GetController() const;   // UI/debug/E1 read access
        void RestoreState(const GameSpeedState& saved);    // called by E1 on load
    };

} // namespace CoW
```

Read access is for UI, debug and E1 only. **Gameplay must not poll the day counter.** It subscribes to `SimulationTick`.

## Features

| # | Feature | Description | Spec | Status |
|---|---------|-------------|------|--------|
| 1 | GameSpeedController Core | `GameSpeedController`, calendar (`ResolveBand`, `WeekOf`/`SeasonOf`/`YearOf`), `SpeedPresetConfig` validation: pause/modal state machine, speed→scale mapping, New Game/restore rules. Pure logic, TDD. | TBD | Approved |
| 2 | GameSpeedControllerModule | SimModule wiring: `.diaapp` config load, bus subscriptions, world `SimTimeDomain` scale/pause/step, day-boundary detection (≤1 tick per frame), `SimulationTick` publishing, `cow_main.diaapp` entry | TBD | Approved |

**Routed elsewhere:** save/load serialization → E1 Feature 2. Speed/pause/step buttons → F-2 stage (Test 4), UI-I later.

## Inherited Binding Decisions

| ID | Decision | How it applies |
|----|----------|----------------|
| PD-002 | ProcessingUnit/Phase/Module architecture | `GameSpeedControllerModule` is a SimModule on SimPU; no singletons or statics |
| AD-001 | Ask permission for Dia edits | No Dia changes needed; uses existing `SimTimeDomain::SetScale/Pause/Resume/Step` |
| AD-002 | Four-tier dependency model | CoW-Shared. Core logic uses no CoW types so it can be promoted to Dia if a second game needs it |
| AD-003 | GDD owns design intent | GDD A2 owns the pause rules; deviations listed below for GDD update |

## Deviations from GDD A2

These are for the DesignDocs owner to fold back into A2:

1. **Speed is a time scale, not a tick interval.** GDD §3 fires a tick every `SEASON_MS[tier]`. Here the sim runs at a fixed rate and the scale controls how fast game time flows.
2. **The day is the base tick, not the season.** `SimulationTick` fires daily with payload `{day, band}`, not `{season, time_band}`. Day and Week are new bands. The calendar is 7 days/week, 13 weeks/season, 4 seasons/year, which matches A4 and A1. `SEASON_MS_*` tuning becomes `wall_seconds_per_day`.
3. **Four speeds, not three.** Adds **Slow**.
4. **Speed request while a modal is open** (not covered by GDD). It updates the speed restored when the modal closes, but does not resume while the modal is open.
5. **Determinism rule.** Gameplay changes only on `SimulationTick`, never from per-frame `gameDt`.
6. **At most one `SimulationTick` per sim frame.** This caps top speed at SimPU Hz days/s (30 days/s). Accepted.
7. **`TickCompleted` removed.** E1 saves at the sim-frame boundary instead. The bus's Reaction pass can't post, so "after all subscribers" can't be a same-update message.
8. **`PauseRequested` is folded into `SpeedChangeRequested{Paused}`.**
9. **`SettingsChanged` preferred default speed is deferred** (the SYS settings system doesn't exist yet).

## Resolved Design Decisions

1. **Top-speed cap: accepted.** With one day tick per sim frame at 30 Hz, Fastest is at most 30 days/s: about 3 s per season and 20 min per century. The GDD's old Fastest (0.15 s/season) would need about 600 days/s.
   - Rejected for now: raising SimPU `frequency_hz` (every sim module runs more often), and several day ticks per frame (per-day Reaction-pass work would interleave with the next day).
   - Revisit if S-2 playtesting shows centuries need to go faster.

## Dependencies

**Requires:** DiaSimTime (world `SimTimeDomain`, Done), DiaMessageBus (built), DiaApplicationFlow (module `config` block), DiaCore (Json, StringCRC, logging).  
**Required by:** E1 (state struct), F-2 Test 4, A1, A4 and all B/C/D-layer systems (`SimulationTick`), UI-I (speed controls).

## Test Strategy

**Unit (Feature 1, GoogleTests):**
- Calendar:
  - 1→Day, 7→Week, 91→Season, 364→Year, 3640→Decade, 36400→Century.
  - `SeasonOf`/`YearOf` at boundaries.
- Pause rules:
  - EC-01: player pause survives a modal.
  - EC-02: nested modals.
  - EC-03: restore always resumes paused.
  - EC-04: same tier again is a no-op.
  - EC-06: step is rejected while running.
- Speed request while a modal is open.
- Config validation: non-decreasing values or a Fastest that is too fast are rejected, and defaults are used.

**Integration (Feature 2 + F-2 Test 4):**
- Each preset produces days at its configured wall-clock rate (within tolerance).
- Pause produces no ticks. A single step produces exactly one tick.
- Never more than one tick per sim frame, and no day is skipped across a speed change.

**Determinism (F-2 Tests 2/3):**
- The same seed run for N days at Slow and at Fastest gives identical state.
- Save at day N, load: day == N and the game is paused.

## Status

**Status:** `Approved` — 2026-10-13 (re-scoped from WorldClock)  
**Plan:** TBD (create when implementation starts)
