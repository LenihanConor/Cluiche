#pragma once
#include <DiaApplicationFlow/SimModule.h>
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaApplicationFlow/ModuleRefV2.h>
#include <DiaCore/CRC/StringCRC.h>
#include "Modules/MessageBusHostModule.h"
#include <cstdint>

namespace CoW {

// Phases of the single F-2 scenario "Save, Reload, Replay", in execution order.
enum class F2PhaseId : uint8_t
{
    Isolation = 0,
    Session = 1,
    Save = 2,
    ReferenceFuture = 3,
    Reload = 4,
    Replay = 5,
    Rerun = 6,
    FailedLoad = 7,
    Idle = 0xFF,   // not running (before RunScenario, and again once ScenarioCompleted has fired)
};
constexpr int kF2PhaseCount = 8;

// The six named checks raised inside the scenario.
enum class F2CheckId : uint8_t
{
    StreamIndependence = 0,   // C1
    Determinism = 1,          // C2
    SaveLoadRng = 2,          // C3
    ClockProof = 3,           // C4
    SaveLoadProof = 4,        // C5
    BandResolver = 5,         // C6
};
constexpr int kF2CheckCount = 6;

enum class F2CheckStatus : uint8_t { NotRun, Running, Pass, Fail };

// Bus events (all trivially copyable).
struct F2PhaseEntered
{
    static inline const Dia::Core::StringCRC kTypeId{ "CoW.F2PhaseEntered" };
    F2PhaseId phase = F2PhaseId::Idle;
};

struct F2CheckCompleted
{
    static inline const Dia::Core::StringCRC kTypeId{ "CoW.F2CheckCompleted" };
    F2CheckId check = F2CheckId::StreamIndependence;
    bool passed = false;
    char summary[96] = "";
};

struct F2ScenarioCompleted
{
    static inline const Dia::Core::StringCRC kTypeId{ "CoW.F2ScenarioCompleted" };
    uint32_t passCount = 0;
    uint32_t failCount = 0;
};

// Runs the F-2 "Save, Reload, Replay" scenario as an internal phase state machine.
// DiaAutomation (or a debug key) only triggers RunScenario(); it is not the runner.
// Spec: docs/specs/applications/cow/systems/f2-core-runtime/f2-core-runtime.md
class F2TestRunnerModule : public Dia::ApplicationFlow::SimModule
{
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kSim;
    static constexpr const char* kDescription = "F-2 Core Runtime test runner: Save, Reload, Replay scenario";

    explicit F2TestRunnerModule(const Dia::Core::StringCRC& instanceId);

    // Requests a scenario run; the phases advance one per update starting on the next update.
    // Returns false (ignored) while a run is already in progress.
    bool RunScenario();

    F2CheckStatus GetCheckStatus(F2CheckId check) const;
    // Human-readable result of the last time the check completed ("" if it has not completed).
    const char* GetLastResult(F2CheckId check) const;
    F2PhaseId GetCurrentPhase() const { return mPhase; }
    bool IsRunning() const { return mRunning || mRunPending; }

protected:
    Dia::ApplicationFlow::StartResult DoStart() override;
    void DoUpdate(const Dia::SimTime::SimTimeContext& ctx) override;
    Dia::ApplicationFlow::StopResult DoStop() override;

private:
    void EnterPhase(F2PhaseId phase);
    void RunPhaseBody(F2PhaseId phase);
    void CompleteCheck(F2CheckId check, bool passed, const char* summary);
    template <class T> void Publish(const T& msg);

    static constexpr int kSummaryLen = 96;

    Dia::ApplicationFlow::ModuleRef<Cluiche::AppFlow::MessageBusHostModule> mBus{this};

    F2PhaseId     mPhase = F2PhaseId::Idle;
    bool          mRunPending = false;
    bool          mRunning = false;
    bool          mBusReady = false;
    F2CheckStatus mStatus[kF2CheckCount] = {};
    char          mLastResult[kF2CheckCount][kSummaryLen] = {};
};

} // namespace CoW
