#pragma once
// World Step Feature 2: the SimModule that turns the pure StepChain into a running world step.
// Owns the "world" DiaDeferredStep channel, kicks once per game week, applies at kick day + k, retries a
// missed week, and runs the same chain inline when configured synchronous.
// Spec: docs/specs/applications/cow/systems/worldstep/module-and-cadence.md
#include <DiaApplicationFlow/SimModule.h>
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaApplicationFlow/ModuleRefV2.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Architecture/Observer.h>
#include <DiaCore/Json/external/json/json.h>
#include <Modules/DeferredStepModule.h>
#include <DiaDeferredStep/DeferredStepScheduler.h>
#include <DiaDeferredStep/IDeferredStep.h>
#include <DiaDeferredStep/IStepKeySource.h>
#include <DiaParallelExecutor/SerialExecutor.h>
#include <DiaMessageBus/BusSubscriptionHandle.h>
#include <DiaObservation/Health/HealthReporterBase.h>
#include <DiaObservation/Metric/Counter.h>
#include <DiaObservation/Metric/Gauge.h>
#include <DiaObservation/Profile/ScopedZone.h>
#include <DiaDeferredStep/IStepChainListener.h>
#include <DiaDeferredStep/StepChain.h>
#include <DiaDeferredStep/StepChainConfigLoader.h>
#include "Modules/GameSpeed/GameSpeedMessages.h"
#include "Modules/MessageBusHostModule.h"
#include "Modules/Plumbing/SaveLoadModule.h"
#include "Modules/SimQueryHostModule.h"

#include <atomic>
#include <cstdint>
#include <optional>

namespace CoW {

// Failing = a configured stage never registered. Degraded = a stall, late or lost week in the last 16
// applied steps, or a retired stage. Healthy otherwise.
class WorldStepHealthReporter : public Dia::Observation::Health::HealthReporterBase
{
public:
    WorldStepHealthReporter() { SetOK(); }
    Dia::Core::StringCRC GetReporterName() const override { return Dia::Core::StringCRC("WorldStepModule"); }
};

class WorldStepModule
    : public Dia::ApplicationFlow::SimModule
    , public Dia::DeferredStep::IDeferredStep
    , public Dia::DeferredStep::IStepKeySource
    , private Dia::DeferredStep::IChainObserver
{
public:
    static const Dia::Core::StringCRC kTypeId;
    static const Dia::Core::StringCRC kChannelId;   // "world"
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kSim;
    static constexpr const char* kDescription = "World step coordinator: kicks the chained world-sim stages once per game week on the world deferred-step channel";
    static constexpr const char* kStatusQueryName  = "cow.worldstep.status";
    static constexpr const char* kStepCommandName  = "cow.worldstep.step";

    // Weeks sit this far apart; the apply key is the kick day plus StepChainConfig::applyAfterSteps (days: the step key is the game day).
    static constexpr uint32_t kHealthWindow = 16;   // applied steps a stall / late / lost week keeps health Degraded
    static constexpr uint32_t kTickQueueCapacity = 8;
    static constexpr uint32_t kMaxListeners = 4;
    static constexpr int32_t  kWorldVersion = 1;    // pending-record "world_version"

    explicit WorldStepModule(const Dia::Core::StringCRC& instanceId);
    ~WorldStepModule() override;

    // ---- Stage registration (stages call these in DoStart / DoStop through ModuleRef<WorldStepModule>) ----
    // False on a duplicate id (logged once per id), a full table, or a removal still pending for that id.
    bool RegisterStage(Dia::Core::StringCRC id, Dia::DeferredStep::IStepStage& stage);
    void UnregisterStage(Dia::Core::StringCRC id);

    // ---- History signal (Feature 3). Up to kMaxListeners, raised in registration order, SimPU, synchronous.
    // False on a duplicate, a full table, or a call from inside a listener (debug assert). Raw non-owning pointers. ----
    bool RegisterListener(Dia::DeferredStep::IStepChainListener& listener);
    void UnregisterListener(Dia::DeferredStep::IStepChainListener& listener);
    uint32_t ListenerCount() const { return mListenerCount; }

    // ---- Tick intake. Called from the bus handler (SimPU). Records the key now; the cadence runs in DoUpdate,
    // after DeferredStepModule has ticked, so a step due on this day applies before the next kick is attempted. ----
    void OnSimulationTick(const SimulationTick& tick);

    // ---- Load reset. SaveManager::OnLoadStarted fires before any participant deserializes, so on it the module drops
    // the old timeline (step in flight, pending week, key, queued ticks); a pending record then restores over the clean state.
    // Production resolves the manager lazily through ModuleRef<SaveLoadModule>; tests call these directly. ----
    void ObserveLoads(Dia::SaveGame::SaveManager& manager);
    void StopObservingLoads();
    bool IsObservingLoads() const { return mObservedManager != nullptr; }
    void OnLoadStarted();   // public so it can be driven without a SaveManager

    // ---- IStepKeySource: the day of the latest SimulationTick received ----
    Dia::DeferredStep::StepKey GetCurrentKey() const override;

    // ---- Live mode switch. False (with *outReason) while a step is in flight or a week is pending. ----
    bool SetDeferred(bool deferred, const char** outReason = nullptr);

    // ---- IDeferredStep ----
    void Capture() override;
    void Run(Dia::ParallelExecutor::IParallelExecutor& executor) override;
    void Apply() override;
    uint32_t GetPendingVersion() const override { return static_cast<uint32_t>(kWorldVersion); }
    bool ValidatePending(Dia::SaveGame::LoadContext& ctx) const override;      // read-only, refuses the whole load
    void SerializePending(Dia::SaveGame::SaveContext& ctx) const override;
    void DeserializePending(Dia::SaveGame::LoadContext& ctx) override;

    // ---- sim-hosted names: the handler bodies, public so they can be driven without the debug server ----
    Json::Value BuildStatusPayload() const;
    Json::Value RunStepCommand(const Json::Value& args);
    bool NamesRegistered() const { return mNamesRegistered; }

    // ---- read access (UI, tests) ----
    const WorldStepHealthReporter& GetHealthReporter() const { return mHealth; }
    const Dia::DeferredStep::StepChainConfig<>& GetConfig() const { return mConfig; }
    const Dia::DeferredStep::ConfigError& GetConfigError() const { return mConfigError; }
    const Dia::DeferredStep::StepChain<>& GetChain() const { return mChain; }
    bool IsDeferred() const { return mConfig.deferred; }
    bool IsRunning() const { return mRunning; }
    bool IsChannelRegistered() const { return mChannelRegistered; }
    bool HasPendingWeek() const { return mPendingDay >= 0; }
    int64_t PendingWeekDay() const { return mPendingDay; }
    uint64_t KickCount() const { return mKickCount; }
    uint64_t SyncStepCount() const { return mSyncSteps; }
    uint64_t ApplyCount() const { return mApplyCount; }
    uint64_t LateKicks() const { return mLateKicks; }
    uint64_t LostWeeks() const { return mLostWeeks; }
    uint64_t TicksProcessed() const { return mTicksProcessed; }
    uint64_t LastKickDay() const { return mLastKickDay; }
    uint64_t LastApplyKey() const { return mLastApplyKey; }
    uint64_t LastStepDay() const { return mStepDay; }
    double   LastStepMs() const { return mLastStepMs; }
    double   LastStallMs() const { return mLastStallMs; }
    uint32_t HistorySeamCalls() const { return mHistorySeamCalls; }
    uint32_t RetiredStageCount() const { return mRetiredCount; }
    uint32_t MissingStageCount() const { return mMissingCount; }

protected:
    void OnConfigure(const char* configJson) override;
    Dia::ApplicationFlow::StartResult DoStart() override;
    void DoUpdate(const Dia::SimTime::SimTimeContext& ctx) override;
    Dia::ApplicationFlow::StopResult DoStop() override;

public:
    // Test seam: injects the scheduler and sim-query host so the module runs without a ProcessingUnit
    // (no bus subscription, no ModuleRef resolution). Production never calls this.
    struct TestHosts
    {
        Dia::DeferredStep::DeferredStepScheduler* scheduler = nullptr;
        Cluiche::AppFlow::SimQueryHostModule*     queryHost = nullptr;
    };
    void UseTestHosts(const TestHosts& hosts) { mTestHosts = hosts; mUseTestHosts = true; }

private:
    // IChainObserver: per-stage profile scope and CPU accumulation (the chain keeps no DiaObservation dependency).
    void OnStageBegin(uint32_t index, Dia::DeferredStep::StagePhase phase) override;
    void OnStageEnd(uint32_t index, Dia::DeferredStep::StagePhase phase) override;

    // ---- cadence: one named method per spec step ----
    void RunCadence(const SimulationTick& tick);
    void ResyncWithScheduler();                     // step 2
    void EvaluateStages();                          // step 4 (missing / retired / unknown), health
    bool CheckReadiness();                          // step 5, false when not ready
    void RetryPendingWeek(const SimulationTick& t); // step 6
    void HandleDueWeek(const SimulationTick& t);    // steps 7 and 8
    void RunSynchronousPath(const SimulationTick& t);
    bool RunSynchronousStep(uint64_t day);

    enum class KickOutcome : uint8_t { Launched, InFlight, Lost };
    KickOutcome TryKick(uint64_t stepDay, uint64_t kickDay);

    bool RegisterChannel();
    void UnregisterChannel();
    bool DrainWorldChannel();                       // scheduler Unregister + Register, chain Abandon
    bool WorldStepInFlight() const;

    bool RegisterNames();
    void UnregisterNames();

    void NoteApplied(uint64_t applyKey);
    void RaiseApplied(uint64_t applyKey, uint64_t stepDay);   // the single history-signal call
    bool GuardReentry(const char* what);            // false (and debug assert) when called from inside a listener
    void PublishTelemetry();
    void UpdateHealth();
    void WarnUnknownStages();
    bool EverRegistered(Dia::Core::StringCRC id) const;
    void ResetRunState();

    // ---- configuration ----
    Dia::DeferredStep::StepChainConfig<> mConfig;
    Dia::DeferredStep::ConfigError     mConfigError;
    bool mConfigOk = false;

    // ---- collaborators (SimPU only, no locks) ----
    Dia::DeferredStep::StepChain<> mChain;
    Dia::ParallelExecutor::SerialExecutor mSerial;
    Dia::DeferredStep::DeferredStepScheduler* mScheduler = nullptr;   // borrowed
    Cluiche::AppFlow::SimQueryHostModule* mQueryHost = nullptr;       // borrowed
    TestHosts mTestHosts;
    bool mUseTestHosts = false;

    Dia::ApplicationFlow::ModuleRef<Dia::DeferredStep::DeferredStepModule> mDeferredRef{this};
    Dia::ApplicationFlow::ModuleRef<Cluiche::AppFlow::SimQueryHostModule> mQueryRef{this};
    Dia::ApplicationFlow::ModuleRef<Cluiche::AppFlow::MessageBusHostModule> mBusRef{this};
    Dia::MessageBus::BusSubscriptionHandle mTickSub;

    struct LoadObserver final : Dia::Core::Observer
    {
        explicit LoadObserver(WorldStepModule& owner) : mOwner(owner) {}
        void ObserverNotification(const Dia::Core::ObserverSubject*, int message) override;
        WorldStepModule& mOwner;
    };
    LoadObserver mLoadObserver{ *this };
    Dia::SaveGame::SaveManager* mObservedManager = nullptr;   // borrowed
    Dia::ApplicationFlow::ModuleRef<SaveLoadModule> mSaveLoadRef{this};
    void TryObserveLoads();

    // ---- keys and cadence state ----
    bool     mRunning = false;
    bool     mHasTick = false;
    uint64_t mLatestDay = 0;
    uint64_t mFrozenKey = 0;       // key reported after DoStop, so a residual DeferredStep update never sees it fall
    uint64_t mStepDay = 0;         // the day the step in flight represents
    int64_t  mPendingDay = -1;     // missed week waiting for a free channel (-1 = none)
    uint64_t mKickApplyKey = 0;
    uint64_t mLastKickDay = 0;
    uint64_t mLastApplyKey = 0;
    bool     mHasKicked = false;
    bool     mChannelRegistered = false;

    SimulationTick mQueue[kTickQueueCapacity];
    uint32_t mQueueCount = 0;

    // ---- stage bookkeeping ----
    Dia::Core::Containers::DynamicArrayC<Dia::DeferredStep::StageId, 16> mEverRegistered;
    uint32_t mMissingCount = 0;
    uint32_t mRetiredCount = 0;
    uint32_t mMissingSignature = 0;
    uint32_t mWarnedUnknown[16] = {};
    uint32_t mWarnedUnknownCount = 0;
    uint32_t mWarnedDuplicate[16] = {};
    uint32_t mWarnedDuplicateCount = 0;
    uint32_t mLoggedRetired[16] = {};
    uint32_t mLoggedRetiredCount = 0;
    enum class ReadyState : uint8_t { Unknown, NotReady, Ready };
    ReadyState mReadyState = ReadyState::Unknown;

    // ---- counters and telemetry ----
    uint64_t mTicksProcessed = 0;
    uint64_t mKickCount = 0;
    uint64_t mSyncSteps = 0;
    uint64_t mApplyCount = 0;
    uint64_t mLateKicks = 0;
    uint64_t mLostWeeks = 0;
    uint64_t mPublishedApplies = 0;
    uint64_t mPublishedLate = 0;
    uint64_t mPublishedLost = 0;
    uint64_t mSeenStalls = 0;
    uint64_t mSeenStallMicros = 0;
    double   mLastStepMs = 0.0;
    double   mLastStallMs = 0.0;
    double   mLastSlackDays = 0.0;
    uint32_t mHistorySeamCalls = 0;

    Dia::DeferredStep::IStepChainListener* mListeners[kMaxListeners] = {};
    uint32_t mListenerCount = 0;
    bool     mRaising = false;             // re-entrancy guard while a listener runs
    bool     mRestoreFailed = false;       // a stage could not restore its pending result; Degraded until a step applies

    uint32_t mBadBits = 0;                 // 1 bit per applied step, newest in bit 0: stall, late or lost week
    bool     mPendingBad = false;          // a bad event not yet attributed to an applied step
    const char* mBadReason = "late_kick";

    std::atomic<uint64_t> mStepAccumNs{ 0 };   // per-stage CPU summed for the step in flight (worker writes during Run)
    uint64_t mStageBeginNs = 0;
    std::optional<Dia::Observation::Profile::ScopedZone> mZone;

    WorldStepHealthReporter mHealth;
    bool mHealthRegistered = false;
    bool mNamesRegistered = false;

    Dia::Observation::Metric::Gauge*   mStepMsGauge = nullptr;
    Dia::Observation::Metric::Gauge*   mSlackGauge = nullptr;
    Dia::Observation::Metric::Counter* mStallsCounter = nullptr;
    Dia::Observation::Metric::Counter* mLateKicksCounter = nullptr;
    Dia::Observation::Metric::Counter* mLostWeeksCounter = nullptr;
};

} // namespace CoW
