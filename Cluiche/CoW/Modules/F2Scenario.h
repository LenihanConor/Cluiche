// F2Scenario: the pure, data-driven part of the F-2 "Save, Reload, Replay" scenario.
// Holds the scripted session/remainder action tables, the per-tick RNG draw pattern and an
// independent reference model of the GameSpeedController (written from gamespeedcontroller.md,
// not from the controller code) used by check C4. No module or bus dependencies.
//
// "Scenario tick" = one F2TestRunnerModule::DoUpdate while the runner is in phase 1 (Session)
// or phase 3 (ReferenceFuture). Phase 1 covers ticks [0, 500), phase 3 ticks [500, 1000).
// Phase 2 (Save) does not advance the scenario tick.
//
// Master seed: the 8 streams are seeded by RandomStreamModule with fixed constants
// ((i + 1) * 10000), so every run starts from the same seed; the scenario adds no entropy.
// Spec: docs/specs/applications/cow/systems/f2-core-runtime/f2-core-runtime.md
#pragma once

#include <Shared/GameSpeedController.h>
#include <cstdint>
#include <string>
#include <vector>

namespace CoW {

constexpr uint32_t kF2SessionTicks   = 500;    // phase 1 runs scenario ticks [0, 500)
constexpr uint32_t kF2ScenarioTicks  = 1000;   // phase 3 runs scenario ticks [500, 1000)
constexpr uint32_t kF2SettleTicks    = 4;      // ticks an action gets to take effect before it is asserted
constexpr uint32_t kF2MinActionGap   = 6;      // minimum ticks between two script actions (> settle)
constexpr int      kF2ReferenceDrawsPerStream = 100;
constexpr int      kF2ReferenceDrawCount      = 8 * kF2ReferenceDrawsPerStream;   // 800
constexpr int      kF2MaxDrawsPerTick         = 4;

enum class F2ActionKind : uint8_t
{
    SetSpeed,       // SpeedChangeRequested{tier}
    PlayerPause,    // SpeedChangeRequested{Paused}
    PlayerResume,   // SpeedChangeRequested{tier}: resume is selecting a running speed
    OpenModal,      // ModalOpened
    CloseModal,     // ModalClosed
    ManualStep,     // ManualAdvanceRequested
};

const char* F2ActionKindName(F2ActionKind kind);

struct F2ScenarioAction
{
    uint32_t     tick = 0;
    F2ActionKind kind = F2ActionKind::SetSpeed;
    SpeedTier    tier = SpeedTier::Normal;   // SetSpeed / PlayerResume only
};

// Phase 1 script, sorted by tick, all ticks < kF2SessionTicks. Ends with no open modal and the
// player pause set, so the clock is stopped for the save at tick 500.
const std::vector<F2ScenarioAction>& F2SessionScript();
// Phase 3 script, sorted by tick, all ticks in [kF2SessionTicks, kF2ScenarioTicks). Its first
// action (tick 500) is an explicit SetSpeed(Normal): GameSpeedController restores only the day
// counter and always comes back paused, so the reference future and the replay must both start
// by choosing a speed.
const std::vector<F2ScenarioAction>& F2RemainderScript();

// Fixed, uneven per-tick draw pattern: fills `streamsOut` with the stream indices (order of
// RandomStreamModule::kStreamNames) to draw once each, returns the count (1..kF2MaxDrawsPerTick).
int F2DrawsForTick(uint32_t tick, uint8_t (&streamsOut)[kF2MaxDrawsPerTick]);

// ---- Reference model of the clock (what the spec says should happen) ----
struct F2ExpectedClock
{
    SpeedTier   tier        = SpeedTier::Paused;
    PauseSource source      = PauseSource::Player;
    uint8_t     modalDepth  = 0;
    SpeedTier   preModal    = SpeedTier::Normal;
};

void F2ApplyAction(F2ExpectedClock& model, const F2ScenarioAction& action);
// True when the model says a manual step issued now is accepted (clock is paused).
bool F2ManualStepAccepted(const F2ExpectedClock& model);
float F2ExpectedTimeScale(SpeedTier tier, const SpeedPresetConfig& config);

struct F2ObservedClock
{
    SpeedTier   tier = SpeedTier::Paused;
    PauseSource source = PauseSource::Player;
    uint8_t     modalDepth = 0;
    float       controllerScale = 0.0f;   // GameSpeedController::GetTimeScale()
    float       domainScale = 0.0f;       // the world SimTimeDomain's applied scale
};

// Empty string when observed matches expected (state exactly, scales within tolerance);
// otherwise a one-line description of the first mismatch.
std::string F2CompareClock(const F2ExpectedClock& expected, const F2ObservedClock& observed,
                           const SpeedPresetConfig& config);

} // namespace CoW
