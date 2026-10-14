#pragma once
#include "Modules/RandomStreamModule.h"   // first: RandomStream.h needs std::min/max unmacroed by windows.h
#include <DiaApplicationFlow/SimModule.h>
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaApplicationFlow/ModuleRefV2.h>
#include <DiaCore/CRC/StringCRC.h>
#include "Modules/MessageBusHostModule.h"
#include "Modules/GameSpeedControllerModule.h"
#include "Modules/SaveLoadModule.h"
#include "Modules/F2Fingerprint.h"
#include "Modules/F2Scenario.h"
#include <DiaMessageBus/BusSubscriptionHandle.h>
#ifdef DIA_DEBUG
#include "Modules/VisualDebuggerModule.h"
#include "Modules/F2TestRunnerDebugDomain.h"
#include <memory>
#endif
#include <array>
#include <cstdint>
#include <string>

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

    // Requests a scenario run; it starts on the next update. Phases 0, 2 (usually), 4-7 take one or
    // two updates each; phases 1 and 3 span many updates (one per "scenario tick", see F2Scenario.h).
    // Returns false (ignored) while a run is already in progress.
    bool RunScenario();

    F2CheckStatus GetCheckStatus(F2CheckId check) const;
    // Human-readable result of the last time the check completed ("" if it has not completed).
    const char* GetLastResult(F2CheckId check) const;
    F2PhaseId GetCurrentPhase() const { return mPhase; }
    bool IsRunning() const { return mRunning || mRunPending; }

    // ---- Manual controls (TestRunnerPanel) ----
    // Post the SAME bus messages the scenario script uses. Refused (return false) while the
    // scenario runs or is pending, or when the bus is unavailable.
    bool RequestSetSpeed(SpeedTier tier);
    bool RequestManualStep();
    bool RequestModal(bool open);
    // Current game clock for the panel readout. False if GameSpeedController is not active.
    bool GetClockState(GameSpeedState& out) const;

    // Test hook: makes the C6 stub dragon re-resolve on every read, so C6 must fail. Survives RunScenario.
    void SetBandResolverFaultForTest(bool enable) { mBandResolverFault = enable; }

    // ---- Results of phases 1-3 (valid once the scenario has passed the phase noted) ----
    uint32_t GetScenarioTick() const { return mScenarioTick; }
    // Phase 2: did the quick save complete successfully?
    bool SaveSucceeded() const { return mSaveSucceeded; }
    // Full fingerprint captured right after the save completed (phase 2). NOTE: GameSpeedController
    // restores only the day counter and always resumes paused, so the reload comparison (C5, task 5)
    // must compare restored fields only: 8 streams + day counter, speed state == paused defaults.
    bool HasFingerprint500() const { return mHasF500; }
    const F2Fingerprint& GetFingerprint500() const { return mF500; }
    // Fingerprint at scenario tick 1000 (end of phase 3), captured BEFORE the reference draws.
    bool HasFingerprint1000() const { return mHasF1000; }
    const F2Fingerprint& GetFingerprint1000() const { return mF1000; }
    // The 800 reference draws (phase 3): stream-major, 100 RandomInt() values per stream in
    // RandomStreamModule::kStreamNames order, drawn right after F1000 was captured. The replay
    // (task 5) must draw in this same order at the same point (after F1000 is reproduced).
    // Live streams are NOT rewound afterwards; the Reload phase restores them from the save.
    bool HasReferenceDraws() const { return mHasReferenceDraws; }
    const std::array<int, kF2ReferenceDrawCount>& GetReferenceDraws() const { return mReferenceDraws; }

    // ---- Results of phases 4-5 ----
    // C5a (reload restored the saved fields; see F2Fingerprint::CompareRestored). C5 as a whole stays
    // Running after C5a passes (part b, the failed load, is plan task 6) and goes to Fail if C5a fails.
    bool HasC5aResult() const { return mHasC5a; }
    bool C5aPassed() const { return mHasC5a && mC5aPassed; }
    const char* GetC5aSummary() const { return mC5aSummary; }
    // Fingerprint captured after the reload, before any replay tick.
    const F2Fingerprint& GetReloadFingerprint() const { return mFReload; }
    // Fingerprint at scenario tick 1000 of the replay (F1000'), captured before the replay draws.
    const F2Fingerprint& GetReplayFingerprint1000() const { return mFReplay1000; }

    // ---- Results of phases 6-7 ----
    // Fingerprint at scenario tick 0 (start of phase 1), taken together with the `start` save.
    bool HasStartFingerprint() const { return mHasFStart; }
    const F2Fingerprint& GetStartFingerprint() const { return mFStart; }
    // Rerun (phase 6) fingerprints F500'' (end of the session, before any save) and F1000''.
    const F2Fingerprint& GetRerunFingerprint500() const { return mFRerun500; }
    const F2Fingerprint& GetRerunFingerprint1000() const { return mFRerun1000; }
    // C5b (failed loads leave state untouched), valid once phase 7 has run.
    bool HasC5bResult() const { return mHasC5b; }
    bool C5bPassed() const { return mHasC5b && mC5bPassed; }
    const char* GetC5bSummary() const { return mC5bSummary; }

protected:
    Dia::ApplicationFlow::StartResult DoStart() override;
    void DoUpdate(const Dia::SimTime::SimTimeContext& ctx) override;
    Dia::ApplicationFlow::StopResult DoStop() override;

private:
#ifdef DIA_DEBUG
    void TryRegisterDebugDomain();
    void UnregisterDebugDomain();
    Dia::ApplicationFlow::ModuleRef<Cluiche::AppFlow::VisualDebuggerModule> mVisualDebuggerRef{this};
    std::unique_ptr<F2TestRunnerDebugDomain> mDebugDomain;
    bool mDomainRegistered = false;
#endif

    void EnterPhase(F2PhaseId phase);
    void RunPhaseBody(F2PhaseId phase);
    void ResetScenarioState();
    void RunIsolation();
    void RunSession();            // phase 1, one scenario tick per call
    void RunSave();               // phase 2
    void RunReferenceFuture();    // phase 3, one scenario tick per call
    void RunReload();             // phase 4: load `quick`, C5a
    void RunReplay();             // phase 5, one scenario tick per call; C3
    void RunRerun();              // phase 6: reload the tick-0 save, run the whole script again; C2
    void RunFailedLoad();         // phase 7: missing and corrupt slot loads; C5b, then resolves C5
    // Shared body of phases 3 and 5: the remainder script and per-tick draws over ticks [500, 1000),
    // then (at tick 1000) the F1000 capture and the 100-draws-per-stream pass. Returns true when finished.
    // `replay` selects what is done with the captured values (store as reference vs compare).
    enum class RemainderMode : uint8_t { Reference, Replay, Rerun };
    bool RunRemainder(RemainderMode mode);
    void FinishC5a(bool passed, const char* detail);
    void FinishC2(bool passed, const char* summary);
    void FinishC5b(bool passed, const char* detail);
    void RunBandResolverCheck();  // C6 load path, at tick 0 of phase 1
    void FinishRerunComparison(bool haveLive, const std::array<int, kF2ReferenceDrawCount>& draws);

    // One step of "request a slot operation, then wait for it to execute at the frame boundary".
    // Pending = keep calling; Ok = executed and succeeded; Failed = executed and reported failure;
    // Rejected = the module refused the request; Timeout = never executed.
    enum class StepResult : uint8_t { Pending, Ok, Failed, Rejected, Timeout };
    StepResult StepSave(const char* slot);
    StepResult StepLoad(const char* slot);
    // Issues one script action as the real bus message. Returns false if the bus is unavailable.
    bool IssueAction(const F2ScenarioAction& action);
    void DrawForTick(uint32_t tick);
    void EvaluatePendingClockCheck();
    void RecordC4Failure(const std::string& what);
    F2Fingerprint CaptureFingerprint(bool& ok);
    void CompleteCheck(F2CheckId check, bool passed, const char* summary);
    template <class T> void Publish(const T& msg);

    static constexpr int kSummaryLen = 96;

    Dia::ApplicationFlow::ModuleRef<Cluiche::AppFlow::MessageBusHostModule> mBus{this};
    Dia::ApplicationFlow::ModuleRef<RandomStreamModule> mRandom{this};
    Dia::ApplicationFlow::ModuleRef<GameSpeedControllerModule> mGameSpeed{this};
    Dia::ApplicationFlow::ModuleRef<SaveLoadModule> mSaveLoad{this};

    F2PhaseId     mPhase = F2PhaseId::Idle;
    bool          mRunPending = false;
    bool          mRunning = false;
    bool          mBusReady = false;
    F2CheckStatus mStatus[kF2CheckCount] = {};
    char          mLastResult[kF2CheckCount][kSummaryLen] = {};

    bool          mPhaseDone = false;
    float         mCtxTimeScale = 0.0f;   // world time scale seen by the current update

    // Phases 1 and 3 (scripted ticks).
    uint32_t         mScenarioTick = 0;
    size_t           mScriptIndex = 0;
    F2ExpectedClock  mModel;

    // C4 bookkeeping.
    Dia::MessageBus::BusSubscriptionHandle mTickSub;
    uint32_t         mTicksThisUpdate = 0;   // SimulationTicks delivered since the last runner update
    uint64_t         mLastTickDay = 0;
    uint32_t         mC4Transitions = 0;
    uint32_t         mC4Matched = 0;
    uint32_t         mC4TickViolations = 0;
    std::string      mC4FirstFailure;
    bool             mCheckPending = false;
    uint32_t         mCheckDueTick = 0;
    F2ScenarioAction mCheckAction;
    bool             mCheckStepAccepted = false;
    uint64_t         mCheckDayAtAction = 0;

    // Phase 2.
    bool             mSaveRequested = false;
    uint32_t         mSaveCountBefore = 0;
    uint32_t         mSaveWaitUpdates = 0;
    bool             mSaveSucceeded = false;

    // Phases 2-3 results.
    bool             mHasF500 = false;
    bool             mHasF1000 = false;
    bool             mHasReferenceDraws = false;
    F2Fingerprint    mF500;
    F2Fingerprint    mF1000;
    std::array<int, kF2ReferenceDrawCount> mReferenceDraws = {};

    // Phase 4 (reload) and phase 5 (replay).
    bool             mLoadRequested = false;
    uint32_t         mLoadCountBefore = 0;
    uint32_t         mLoadWaitUpdates = 0;
    bool             mHasC5a = false;
    bool             mC5aPassed = false;
    char             mC5aSummary[kSummaryLen] = "";
    F2Fingerprint    mFReload;
    F2Fingerprint    mFReplay1000;

    // Tick-0 save (taken at the start of phase 1) and phase 6 (rerun).
    bool             mStartSaveDone = false;
    bool             mStartSaveOk = false;
    bool             mHasFStart = false;
    F2Fingerprint    mFStart;
    bool             mRerun = false;          // phase 1 / remainder code is running for the rerun (C2)
    enum class RerunStage : uint8_t { LoadStart, Session, Remainder };
    RerunStage       mRerunStage = RerunStage::LoadStart;
    F2Fingerprint    mFRerun500;
    F2Fingerprint    mFRerun1000;

    bool             mBandResolverFault = false;

    // Phase 7 (failed loads).
    enum class FailStage : uint8_t { Quiesce, Begin, MissingLoad, CorruptLoad };
    FailStage        mFailStage = FailStage::Quiesce;
    uint32_t         mFailQuiesceUpdates = 0;
    std::string      mQuickBytesBefore;
    F2Fingerprint    mFLiveBefore;
    bool             mMissingLoadFailed = false;
    bool             mHasC5b = false;
    bool             mC5bPassed = false;
    char             mC5bSummary[kSummaryLen] = "";
};

} // namespace CoW
