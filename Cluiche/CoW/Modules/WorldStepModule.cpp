#include "Modules/WorldStepModule.h"

#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaApplicationFlow/ProcessingUnit.h>
#include <DiaCore/Core/Assert.h>
#include <DiaDebugServer/SimHelpInfo.h>
#include <DiaSaveGame/LoadContext.h>
#include <DiaSaveGame/SaveContext.h>
#include <DiaObservation/Health/HealthRegistry.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Metric/MetricRegistry.h>
#include <DiaObservation/Profile/DiaProfile.h>
#include <DiaObservation/Trace/DiaTrace.h>

#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>

namespace CoW {

using Dia::Core::StringCRC;
using Dia::DeferredStep::KickResult;
using Dia::DeferredStep::StepKey;
using Dia::DeferredStep::StepState;

const StringCRC WorldStepModule::kTypeId("WorldStepModule");
const StringCRC WorldStepModule::kChannelId("world");

namespace {

constexpr const char* kLog = "WST";

uint64_t NowNs()
{
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}

// Fixed-capacity "have I logged this id" latch. Returns true if the value was already latched.
bool LatchTestAndSet(uint32_t (&values)[16], uint32_t& count, uint32_t value)
{
    for (uint32_t i = 0; i < count; ++i)
    {
        if (values[i] == value)
            return true;
    }
    if (count < 16)
        values[count++] = value;
    return false;
}

void LatchClear(uint32_t (&values)[16], uint32_t& count, uint32_t value)
{
    for (uint32_t i = 0; i < count; ++i)
    {
        if (values[i] == value)
        {
            values[i] = values[count - 1];
            --count;
            return;
        }
    }
}

using Dia::Observation::Metric::MetricInfo;
// Info strings are stored by pointer in the registry: literals only.
const MetricInfo kStepMsInfo    { "World step CPU",  "Total CPU across stages for the last applied step",          "ms" };
const MetricInfo kSlackInfo     { "World step slack","Apply key minus the day the step was found complete",         "days" };
const MetricInfo kStallsInfo    { "World step stalls","Applies that had to wait for the step to finish",           "count" };
const MetricInfo kLateInfo      { "World step late kicks","Weeks that found the channel in flight and were retried","count" };
const MetricInfo kLostInfo      { "World step lost weeks","Weeks dropped because an older week was still waiting",  "count" };

const char* const kMetricNames[] = {
    "cow.worldstep.step_ms", "cow.worldstep.slack_days", "cow.worldstep.stalls",
    "cow.worldstep.late_kicks", "cow.worldstep.lost_weeks" };

Json::Value Refuse(const char* reason)
{
    Json::Value v(Json::objectValue);
    v["success"] = false;
    v["error"] = reason;
    return v;
}

} // namespace

WorldStepModule::WorldStepModule(const StringCRC& instanceId)
    : SimModule(instanceId)
{}

WorldStepModule::~WorldStepModule()
{
    // Destroyed without a Stop: never leave a dangling reporter in the process-wide registry.
    if (mHealthRegistered)
    {
        Dia::Observation::Health::HealthRegistry::Instance().Unregister(&mHealth);
        mHealthRegistered = false;
    }
}

// ---------------------------------------------------------------------------------------------
// Configuration and lifecycle
// ---------------------------------------------------------------------------------------------

void WorldStepModule::OnConfigure(const char* configJson)
{
    mConfig = WorldStep::WorldStepConfig();
    mConfigError = WorldStep::ConfigError();
    mConfigOk = false;

    if (configJson == nullptr || configJson[0] == '\0')
    {
        mConfigError.key.SetText("config");
        mConfigError.reason.SetText("missing");
        return;
    }

    Json::Value root;
    Json::Reader reader;
    if (!reader.parse(configJson, root) || !root.isObject())
    {
        mConfigError.key.SetText("config");
        mConfigError.reason.SetText("not a JSON object");
        return;
    }

    WorldStep::WorldStepConfig parsed;
    WorldStep::ConfigError error;
    if (WorldStep::LoadWorldStepConfig(root, parsed, error))
    {
        mConfig = parsed;
        mConfigOk = true;
    }
    else
    {
        mConfigError = error;
    }
}

void WorldStepModule::ResetRunState()
{
    mHasTick = false;
    mLatestDay = 0;
    mStepDay = 0;
    mPendingDay = -1;
    mKickApplyKey = 0;
    mLastKickDay = 0;
    mLastApplyKey = 0;
    mHasKicked = false;
    mQueueCount = 0;
    mMissingCount = 0;
    mRetiredCount = 0;
    mMissingSignature = 0;
    mReadyState = ReadyState::Unknown;
    mTicksProcessed = 0;
    mKickCount = 0;
    mSyncSteps = 0;
    mApplyCount = 0;
    mLateKicks = 0;
    mLostWeeks = 0;
    mPublishedApplies = 0;
    mPublishedLate = 0;
    mPublishedLost = 0;
    mLastStepMs = 0.0;
    mLastStallMs = 0.0;
    mLastSlackDays = 0.0;
    mHistorySeamCalls = 0;
    mRaising = false;
    mRestoreFailed = false;
    mBadBits = 0;
    mPendingBad = false;
    mStepAccumNs.store(0);
}

Dia::ApplicationFlow::StartResult WorldStepModule::DoStart()
{
    using Dia::ApplicationFlow::StartResult;
    DIA_TRACE_ZONE("WorldStepModule::DoStart", Dia::Observation::Trace::Category::kNone);

    if (!mConfigOk)
    {
        DIA_LOG_ERROR(kLog, "WorldStepModule: invalid config, key '%s': %s",
                      mConfigError.key.AsCStr(), mConfigError.reason.AsCStr());
        return StartResult::kFailed;
    }

    // Resolve collaborators. Production reaches them through ModuleRef; tests inject them.
    Cluiche::AppFlow::MessageBusHostModule* bus = nullptr;
    Dia::DeferredStep::DeferredStepModule* deferredHost = nullptr;
    if (mUseTestHosts)
    {
        mScheduler = mTestHosts.scheduler;
        mQueryHost = mTestHosts.queryHost;
    }
    else
    {
        deferredHost = mDeferredRef.Get();
        mQueryHost = mQueryRef.Get();
        bus = mBusRef.Get();
        if (deferredHost == nullptr || mQueryHost == nullptr || bus == nullptr)
            return StartResult::kLoading;
        mScheduler = &deferredHost->GetScheduler();
    }
    if (mScheduler == nullptr)
    {
        DIA_LOG_ERROR(kLog, "WorldStepModule: no deferred-step scheduler available");
        mQueryHost = nullptr;
        return StartResult::kFailed;
    }

    ResetRunState();
    mFrozenKey = 0;

    // Names first: all-or-nothing, and nothing else has been acquired yet if they fail.
    if (mQueryHost != nullptr && !RegisterNames())
    {
        mScheduler = nullptr;
        mQueryHost = nullptr;
        return StartResult::kFailed;
    }

    if (!mChain.Configure(mConfig))
    {
        DIA_LOG_ERROR(kLog, "WorldStepModule: chain refused the configuration (a step is in flight)");
    }
    mChain.SetObserver(this);

    if (mConfig.deferred && !RegisterChannel())
    {
        DIA_LOG_ERROR(kLog, "WorldStepModule: could not register the 'world' channel with the scheduler");
        UnregisterNames();
        mScheduler = nullptr;
        mQueryHost = nullptr;
        return StartResult::kFailed;
    }

    if (bus != nullptr)
    {
        Dia::MessageBus::Bus& b = bus->GetBus();
        b.RegisterType<SimulationTick, 16>();   // idempotent: the game clock registers it first
        mTickSub = b.Subscribe<SimulationTick>(GetInstanceId(),
            [this](const SimulationTick& tick) { OnSimulationTick(tick); });
        if (!mTickSub.IsValid())
        {
            DIA_LOG_ERROR(kLog, "WorldStepModule: SimulationTick subscription failed (handler pool full?)");
            UnregisterChannel();
            UnregisterNames();
            mScheduler = nullptr;
            mQueryHost = nullptr;
            return StartResult::kFailed;
        }
    }
    if (deferredHost != nullptr)
        deferredHost->SetKeySource(*this);   // read lazily on each DeferredStepModule::DoUpdate

    mSeenStalls = mScheduler->GetStats().stalls;
    mSeenStallMicros = mScheduler->GetStats().totalStallMicros;

    auto& metrics = Dia::Observation::Metric::MetricRegistry::Instance();
    mStepMsGauge      = metrics.RegisterGauge  (StringCRC(kMetricNames[0]), kStepMsInfo);
    mSlackGauge       = metrics.RegisterGauge  (StringCRC(kMetricNames[1]), kSlackInfo);
    mStallsCounter    = metrics.RegisterCounter(StringCRC(kMetricNames[2]), kStallsInfo);
    mLateKicksCounter = metrics.RegisterCounter(StringCRC(kMetricNames[3]), kLateInfo);
    mLostWeeksCounter = metrics.RegisterCounter(StringCRC(kMetricNames[4]), kLostInfo);

    Dia::Observation::Health::HealthRegistry::Instance().Register(&mHealth);
    mHealthRegistered = true;
    mRunning = true;
    UpdateHealth();

    char order[160] = {};
    for (uint32_t i = 0; i < mConfig.order.Size(); ++i)
    {
        std::snprintf(order + std::strlen(order), sizeof(order) - std::strlen(order), "%s%s",
                      i == 0 ? "" : ",", mConfig.order[i].name.AsCStr());
    }
    DIA_LOG_INFO(kLog, "WorldStepModule: configured mode=%s order=[%s] k=%u days",
                 mConfig.deferred ? "deferred" : "synchronous", order, static_cast<unsigned>(mConfig.applyAfterDays));
    return StartResult::kReady;
}

Dia::ApplicationFlow::StopResult WorldStepModule::DoStop()
{
    DIA_TRACE_ZONE("WorldStepModule::DoStop", Dia::Observation::Trace::Category::kNone);

    mTickSub = Dia::MessageBus::BusSubscriptionHandle();

    // Drains an in-flight step and discards it: no Apply, so no Commit and no history signal.
    UnregisterChannel();
    mChain.SetObserver(nullptr);
    mChain.Abandon();
    mChain = WorldStep::WorldStepChain();   // drops every registration; stages re-register on re-entry
    UnregisterNames();

    if (mHealthRegistered)
    {
        Dia::Observation::Health::HealthRegistry::Instance().Unregister(&mHealth);
        mHealthRegistered = false;
    }
    mHealth.SetOK();
    auto& metrics = Dia::Observation::Metric::MetricRegistry::Instance();
    if (mStepMsGauge != nullptr)
    {
        for (const char* name : kMetricNames)
            metrics.Unregister(StringCRC(name));
    }
    mStepMsGauge = mSlackGauge = nullptr;
    mStallsCounter = mLateKicksCounter = mLostWeeksCounter = nullptr;

    // A residual DeferredStepModule update after this point must not see the key fall.
    mFrozenKey = mLatestDay;
    const uint64_t stoppedAt = mLatestDay;
    mRunning = false;
    ResetRunState();
    // Registration history belongs to one visit: a stage that registered before Start survives into it, one that
    // registered during it does not survive the stop.
    mEverRegistered.RemoveAll();
    mWarnedUnknownCount = 0;
    mWarnedDuplicateCount = 0;
    mLoggedRetiredCount = 0;
    mScheduler = nullptr;
    mQueryHost = nullptr;
    mZone.reset();

    DIA_LOG_INFO(kLog, "WorldStepModule: stopped at day %llu", static_cast<unsigned long long>(stoppedAt));
    return Dia::ApplicationFlow::StopResult::kDone;
}

// ---------------------------------------------------------------------------------------------
// Key source and tick intake
// ---------------------------------------------------------------------------------------------

StepKey WorldStepModule::GetCurrentKey() const
{
    return mRunning ? mLatestDay : mFrozenKey;
}

void WorldStepModule::OnSimulationTick(const SimulationTick& tick)
{
    // The key is monotonic by construction: a smaller day (reload) resets it only through Feature 3's load path.
    if (!mHasTick || tick.day > mLatestDay)
        mLatestDay = tick.day;
    mHasTick = true;

    if (mQueueCount == kTickQueueCapacity)
    {
        DIA_LOG_ERROR(kLog, "WorldStepModule: tick queue full, dropping the oldest tick (day %llu)",
                      static_cast<unsigned long long>(mQueue[0].day));
        for (uint32_t i = 1; i < mQueueCount; ++i)
            mQueue[i - 1] = mQueue[i];
        --mQueueCount;
    }
    mQueue[mQueueCount++] = tick;
}

// ---------------------------------------------------------------------------------------------
// Stage registration
// ---------------------------------------------------------------------------------------------

bool WorldStepModule::EverRegistered(StringCRC id) const
{
    for (uint32_t i = 0; i < mEverRegistered.Size(); ++i)
    {
        if (mEverRegistered[i].crc == id)
            return true;
    }
    return false;
}

bool WorldStepModule::RegisterStage(StringCRC id, WorldStep::IWorldStepStage& stage)
{
    if (!GuardReentry("RegisterStage"))
        return false;
    if (!mChain.RegisterStage(id, stage))
    {
        if (!LatchTestAndSet(mWarnedDuplicate, mWarnedDuplicateCount, id.Value()))
        {
            DIA_LOG_WARNING(kLog, "WorldStepModule: registration of stage '%s' rejected (duplicate id, table full or removal pending)",
                            id.AsChar());
        }
        return false;
    }

    if (!EverRegistered(id) && !mEverRegistered.IsFull())
    {
        WorldStep::StageId entry;
        entry.crc = id;
        entry.name.SetText(id.AsChar());
        mEverRegistered.Add(entry);
    }
    LatchClear(mLoggedRetired, mLoggedRetiredCount, id.Value());   // a returning stage can retire again

    DIA_LOG_INFO(kLog, "WorldStepModule: stage '%s' registered", id.AsChar());
    WarnUnknownStages();
    if (mRunning)
        EvaluateStages();
    return true;
}

void WorldStepModule::UnregisterStage(StringCRC id)
{
    if (!GuardReentry("UnregisterStage"))
        return;
    if (mRunning)
    {
        if (WorldStepInFlight())
        {
            // The chain would queue the removal, but the step is still reading the stage on a worker. Drain and
            // discard it (no Commit, no history signal), then remove at Idle.
            DIA_LOG_WARNING(kLog, "WorldStepModule: stage '%s' unregistered mid-step, draining and discarding the step",
                            id.AsChar());
            DrainWorldChannel();
        }
        else if (mChain.Phase() != WorldStep::ChainPhase::Idle)
        {
            mChain.Abandon();
        }
    }

    mChain.UnregisterStage(id);

    if (!mRunning)
        return;

    bool configured = false;
    for (uint32_t i = 0; i < mConfig.order.Size(); ++i)
        configured = configured || mConfig.order[i].crc == id;
    if (configured && EverRegistered(id) && !LatchTestAndSet(mLoggedRetired, mLoggedRetiredCount, id.Value()))
    {
        DIA_LOG_INFO(kLog, "WorldStepModule: stage '%s' retired, kicks stop until it registers again", id.AsChar());
    }
    EvaluateStages();
}

void WorldStepModule::WarnUnknownStages()
{
    if (!mRunning)
        return;   // the chain is not configured yet: every stage would look unknown
    WorldStep::StageIdList unknown;
    if (!mChain.UnknownStages(unknown))
        return;
    for (uint32_t i = 0; i < unknown.Size(); ++i)
    {
        if (!LatchTestAndSet(mWarnedUnknown, mWarnedUnknownCount, unknown[i].crc.Value()))
        {
            DIA_LOG_WARNING(kLog, "WorldStepModule: stage '%s' is registered but not in the configured order, it is skipped",
                            unknown[i].name.AsCStr());
        }
    }
}

// ---------------------------------------------------------------------------------------------
// Channel management
// ---------------------------------------------------------------------------------------------

bool WorldStepModule::RegisterChannel()
{
    if (mScheduler == nullptr)
        return false;
    mChannelRegistered = mScheduler->Register(kChannelId, *this);
    return mChannelRegistered;
}

void WorldStepModule::UnregisterChannel()
{
    if (mScheduler != nullptr && mChannelRegistered)
        mScheduler->Unregister(kChannelId);   // drains (waits for Run) and discards: no Apply
    mChannelRegistered = false;
}

bool WorldStepModule::WorldStepInFlight() const
{
    return mScheduler != nullptr && mChannelRegistered && mScheduler->GetState(kChannelId) != StepState::kIdle;
}

bool WorldStepModule::DrainWorldChannel()
{
    UnregisterChannel();
    mChain.Abandon();
    mStepAccumNs.store(0);
    if (mConfig.deferred)
        return RegisterChannel();   // keep the channel available
    return true;
}

bool WorldStepModule::SetDeferred(bool deferred, const char** outReason)
{
    auto refuse = [&](const char* reason)
    {
        if (outReason != nullptr)
            *outReason = reason;
        DIA_LOG_WARNING(kLog, "WorldStepModule: mode switch refused: %s", reason);
        return false;
    };

    if (!GuardReentry("SetDeferred"))
        return refuse("called from inside the history signal");
    if (deferred == mConfig.deferred)
        return true;
    if (mPendingDay >= 0)
        return refuse("a missed week is pending");
    if (WorldStepInFlight() || (mRunning && mChain.Phase() != WorldStep::ChainPhase::Idle))
        return refuse("a step is in flight");

    if (mRunning && mScheduler != nullptr)
    {
        if (deferred)
        {
            if (!RegisterChannel())
                return refuse("could not register the world channel");
        }
        else
        {
            UnregisterChannel();   // idle: nothing is drained or discarded
        }
    }
    mConfig.deferred = deferred;
    mChain.Configure(mConfig);   // mirror the mode; the order is unchanged
    DIA_LOG_INFO(kLog, "WorldStepModule: mode switched to %s", deferred ? "deferred" : "synchronous");
    return true;
}

// ---------------------------------------------------------------------------------------------
// Cadence
// ---------------------------------------------------------------------------------------------

void WorldStepModule::DoUpdate(const Dia::SimTime::SimTimeContext& /*ctx*/)
{
    DIA_TRACE_ZONE("WorldStepModule::DoUpdate", Dia::Observation::Trace::Category::kNone);
    DIA_PROFILE_SCOPE("WorldStepModule::DoUpdate", Dia::Observation::Profile::Category::kNone);

    if (!mRunning)
        return;

    // Copy first: the queue belongs to the bus handler, the cadence only reads its own snapshot.
    SimulationTick pending[kTickQueueCapacity];
    const uint32_t count = mQueueCount;
    for (uint32_t i = 0; i < count; ++i)
        pending[i] = mQueue[i];
    mQueueCount = 0;

    for (uint32_t i = 0; i < count; ++i)
        RunCadence(pending[i]);

    PublishTelemetry();
}

void WorldStepModule::RunCadence(const SimulationTick& tick)
{
    ++mTicksProcessed;                                  // step 1: the key was recorded on arrival
    ResyncWithScheduler();                              // step 2
    EvaluateStages();                                   // step 4 (also feeds health; runs in both modes)

    if (!mConfig.deferred)                              // step 3
    {
        RunSynchronousPath(tick);
        return;
    }
    if (mMissingCount > 0)
        return;                                         // a configuration error blocks kicks
    if (!CheckReadiness())                              // step 5
        return;
    if (mPendingDay >= 0)
        RetryPendingWeek(tick);                         // step 6
    if (tick.band >= TimeBand::Week)                    // step 7: >= so Season/Year/Decade/Century weeks still kick
        HandleDueWeek(tick);                            // step 8
}

void WorldStepModule::ResyncWithScheduler()
{
    // Only read the chain's phase when the scheduler holds nothing: an idle channel means no worker is in Run.
    if (!mChannelRegistered || mScheduler == nullptr)
        return;
    if (mScheduler->GetState(kChannelId) == StepState::kIdle && mChain.Phase() != WorldStep::ChainPhase::Idle)
    {
        mChain.Abandon();
        mStepAccumNs.store(0);
        DIA_LOG_INFO(kLog, "WorldStepModule: the scheduler abandoned the step in flight, chain reset to idle");
    }
}

void WorldStepModule::EvaluateStages()
{
    WorldStep::StageIdList absent;
    mChain.MissingStages(absent);

    uint32_t missing = 0;
    uint32_t retired = 0;
    uint32_t signature = 0;
    char names[160] = {};
    for (uint32_t i = 0; i < absent.Size(); ++i)
    {
        if (EverRegistered(absent[i].crc))
        {
            ++retired;
        }
        else
        {
            ++missing;
            signature += absent[i].crc.Value();
            std::snprintf(names + std::strlen(names), sizeof(names) - std::strlen(names), "%s%s",
                          missing == 1 ? "" : ",", absent[i].name.AsCStr());
        }
    }
    mMissingCount = missing;
    mRetiredCount = retired;

    if (missing > 0)
    {
        signature += 0x9E3779B9u;
        // The configuration check starts at the first tick: stages register one by one before it.
        if (mTicksProcessed > 0 && signature != mMissingSignature)
        {
            mMissingSignature = signature;
            DIA_LOG_ERROR(kLog, "WorldStepModule: configured stage(s) never registered, no kicks: %s", names);
        }
    }
    else
    {
        mMissingSignature = 0;
    }

    WarnUnknownStages();
    UpdateHealth();
}

bool WorldStepModule::CheckReadiness()
{
    if (!mChain.AllReady())
    {
        if (mReadyState != ReadyState::NotReady)
        {
            mReadyState = ReadyState::NotReady;
            DIA_LOG_INFO(kLog, "WorldStepModule: stages not ready, no kicks until every configured stage reports ready");
        }
        return false;
    }
    if (mReadyState != ReadyState::Ready)
    {
        mReadyState = ReadyState::Ready;
        DIA_LOG_INFO(kLog, "WorldStepModule: stages now ready (%u enabled), kicks begin", mChain.EnabledCount());
    }
    return true;
}

WorldStepModule::KickOutcome WorldStepModule::TryKick(uint64_t stepDay, uint64_t kickDay)
{
    const uint64_t previousStepDay = mStepDay;   // the step already in flight keeps its day if this kick is refused
    mStepDay = stepDay;   // IDeferredStep::Capture() reads this inside Kick
    const uint64_t applyKey = kickDay + mConfig.applyAfterDays;
    const KickResult result = mScheduler->Kick(kChannelId, kickDay, applyKey);
    if (result != KickResult::kLaunched)
        mStepDay = previousStepDay;
    switch (result)
    {
    case KickResult::kLaunched:
        mKickApplyKey = applyKey;
        mLastKickDay = kickDay;
        mHasKicked = true;
        ++mKickCount;
        return KickOutcome::Launched;

    case KickResult::kAlreadyInFlight:
        return KickOutcome::InFlight;

    case KickResult::kUnknownChannel:
        DIA_LOG_ERROR(kLog, "WorldStepModule: the 'world' channel was not registered; re-registering, week %llu is lost",
                      static_cast<unsigned long long>(stepDay));
        mChannelRegistered = false;
        RegisterChannel();
        return KickOutcome::Lost;

    case KickResult::kKeyNotMonotonic:
        DIA_LOG_ERROR(kLog, "WorldStepModule: kick key %llu is lower than the previous kick, week %llu is lost",
                      static_cast<unsigned long long>(kickDay), static_cast<unsigned long long>(stepDay));
        DIA_ASSERT(false, "WorldStepModule: kick rejected, key not monotonic");
        return KickOutcome::Lost;

    case KickResult::kApplyBeforeKick:
        DIA_LOG_ERROR(kLog, "WorldStepModule: apply key %llu is before kick key %llu, week %llu is lost",
                      static_cast<unsigned long long>(applyKey), static_cast<unsigned long long>(kickDay),
                      static_cast<unsigned long long>(stepDay));
        DIA_ASSERT(false, "WorldStepModule: kick rejected, apply key before kick key");
        return KickOutcome::Lost;
    }
    return KickOutcome::Lost;
}

void WorldStepModule::RetryPendingWeek(const SimulationTick& t)
{
    const uint64_t stepDay = static_cast<uint64_t>(mPendingDay);
    switch (TryKick(stepDay, t.day))
    {
    case KickOutcome::Launched:
        mPendingDay = -1;
        DIA_LOG_INFO(kLog, "WorldStepModule: missed week %llu kicked on day %llu (%llu day(s) late), applies at %llu",
                     static_cast<unsigned long long>(stepDay), static_cast<unsigned long long>(t.day),
                     static_cast<unsigned long long>(t.day - stepDay), static_cast<unsigned long long>(mKickApplyKey));
        break;
    case KickOutcome::InFlight:
        break;   // still busy: try again next tick
    case KickOutcome::Lost:
        mPendingDay = -1;
        ++mLostWeeks;
        mPendingBad = true;
        mBadReason = "lost_week";
        break;
    }
}

void WorldStepModule::HandleDueWeek(const SimulationTick& t)
{
    if (mPendingDay >= 0)
    {
        // Unrecoverable lag: keep the older week (order is preserved) and drop this one.
        ++mLostWeeks;
        mPendingBad = true;
        mBadReason = "lost_week";
        DIA_LOG_ERROR(kLog, "WorldStepModule: week %llu dropped, week %lld is still waiting for a free channel",
                      static_cast<unsigned long long>(t.day), static_cast<long long>(mPendingDay));
        return;
    }

    switch (TryKick(t.day, t.day))
    {
    case KickOutcome::Launched:
        break;
    case KickOutcome::InFlight:
        mPendingDay = static_cast<int64_t>(t.day);
        ++mLateKicks;
        mPendingBad = true;
        mBadReason = "late_kick";
        DIA_LOG_WARNING(kLog, "WorldStepModule: week %llu found the channel in flight, retrying on a following tick",
                        static_cast<unsigned long long>(t.day));
        break;
    case KickOutcome::Lost:
        ++mLostWeeks;
        mPendingBad = true;
        mBadReason = "lost_week";
        break;
    }
}

// ---------------------------------------------------------------------------------------------
// Synchronous mode
// ---------------------------------------------------------------------------------------------

void WorldStepModule::RunSynchronousPath(const SimulationTick& t)
{
    if (mMissingCount > 0)
        return;
    if (!CheckReadiness())
        return;
    if (t.band >= TimeBand::Week)
        RunSynchronousStep(t.day);
}

bool WorldStepModule::RunSynchronousStep(uint64_t day)
{
    DIA_TRACE_ZONE("WorldStepModule::SyncStep", Dia::Observation::Trace::Category::kNone);
    mStepDay = day;
    mStepAccumNs.store(0);
    if (!mChain.Capture(day))
    {
        DIA_LOG_ERROR(kLog, "WorldStepModule: synchronous step refused by the chain at day %llu", static_cast<unsigned long long>(day));
        return false;
    }
    mChain.Run(mSerial);
    if (!mChain.Commit())
    {
        DIA_LOG_ERROR(kLog, "WorldStepModule: synchronous commit refused by the chain at day %llu", static_cast<unsigned long long>(day));
        mChain.Abandon();
        return false;
    }
    ++mSyncSteps;
    mLastKickDay = day;
    mHasKicked = true;
    NoteApplied(day);
    RaiseApplied(day, day);
    return true;
}

// ---------------------------------------------------------------------------------------------
// IDeferredStep
// ---------------------------------------------------------------------------------------------

void WorldStepModule::Capture()
{
    DIA_TRACE_ZONE("WorldStepModule::Capture", Dia::Observation::Trace::Category::kNone);
    mStepAccumNs.store(0);
    if (!mChain.Capture(mStepDay))
    {
        DIA_LOG_ERROR(kLog, "WorldStepModule: chain refused Capture for day %llu", static_cast<unsigned long long>(mStepDay));
    }
}

void WorldStepModule::Run(Dia::DeferredStep::IParallelExecutor& executor)
{
    DIA_TRACE_ZONE("WorldStepModule::Run", Dia::Observation::Trace::Category::kNone);
    mChain.Run(executor);
}

void WorldStepModule::Apply()
{
    DIA_TRACE_ZONE("WorldStepModule::Apply", Dia::Observation::Trace::Category::kNone);
    // A restored channel does not know its apply key: ask the scheduler (the channel is still complete during Apply).
    uint64_t applyKey = mKickApplyKey;
    Dia::DeferredStep::StepKey scheduled = 0;
    if (mScheduler != nullptr && mScheduler->GetApplyKey(kChannelId, scheduled))
        applyKey = scheduled;
    mKickApplyKey = applyKey;

    if (!mChain.Commit())
    {
        // A restored step whose stages could not restore (chain stayed Idle): nothing to commit, so no signal and no metric.
        DIA_LOG_WARNING(kLog, "WorldStepModule: nothing to commit at apply key %llu (chain not in the Ran phase), step ignored",
                        static_cast<unsigned long long>(applyKey));
        mChain.Abandon();
        return;
    }
    NoteApplied(applyKey);
    RaiseApplied(applyKey, mStepDay);
}

// ---------------------------------------------------------------------------------------------
// Pending record (save/load of a step in flight)
// ---------------------------------------------------------------------------------------------

void WorldStepModule::SerializePending(Dia::SaveGame::SaveContext& ctx) const
{
    DIA_TRACE_ZONE("WorldStepModule::SerializePending", Dia::Observation::Trace::Category::kNone);
    if (mChain.Phase() != WorldStep::ChainPhase::Ran)
    {
        DIA_LOG_ERROR(kLog, "WorldStepModule: SerializePending with no completed step (chain phase %u), nothing written; this save will be refused on load",
                      static_cast<unsigned>(mChain.Phase()));
        return;
    }

    const uint32_t count = mChain.EnabledCount();
    ctx.Write(StringCRC("world_version"), kWorldVersion);
    ctx.Write(StringCRC("step_day"), static_cast<int64_t>(mStepDay));
    ctx.Write(StringCRC("pending_day"), mPendingDay);
    ctx.Write(StringCRC("latest_day"), static_cast<int64_t>(mLatestDay));
    ctx.Write(StringCRC("stage_count"), static_cast<int32_t>(count));
    for (uint32_t i = 0; i < count; ++i)
    {
        char name[16];
        std::snprintf(name, sizeof(name), "stage_%u", i);
        ctx.BeginObject(StringCRC(name));
        ctx.Write(StringCRC("id"), static_cast<int64_t>(mChain.StageIdAt(i).Value()));
        mChain.StageAt(i)->SerializePending(ctx);
        ctx.EndObject();
    }
    DIA_LOG_INFO(kLog, "WorldStepModule: pending step day %llu saved (%u stage(s), pending week %lld)",
                 static_cast<unsigned long long>(mStepDay), count, static_cast<long long>(mPendingDay));
}

namespace {

// The saved stage table: ids in saved (declared) order.
struct SavedStages
{
    int64_t ids[WorldStep::kMaxStages] = {};
    uint32_t count = 0;
};

int FindSaved(const SavedStages& saved, uint32_t value)
{
    for (uint32_t i = 0; i < saved.count; ++i)
    {
        if (saved.ids[i] == static_cast<int64_t>(value))
            return static_cast<int>(i);
    }
    return -1;
}

} // namespace

bool WorldStepModule::ValidatePending(Dia::SaveGame::LoadContext& ctx) const
{
    DIA_TRACE_ZONE("WorldStepModule::ValidatePending", Dia::Observation::Trace::Category::kNone);
    auto fail = [](const char* key, const char* reason, const char* stage = nullptr)
    {
        if (stage != nullptr)
            DIA_LOG_ERROR(kLog, "WorldStepModule: pending record refused, key '%s' (stage '%s'): %s", key, stage, reason);
        else
            DIA_LOG_ERROR(kLog, "WorldStepModule: pending record refused, key '%s': %s", key, reason);
        return false;
    };

    int32_t version = 0;
    if (!ctx.Read(StringCRC("world_version"), version))
        return fail("world_version", "missing or not an integer");
    if (version < 1 || version > kWorldVersion)
        return fail("world_version", "unsupported version");

    int64_t stepDay = 0, pendingDay = 0, latestDay = 0;
    if (!ctx.Read(StringCRC("step_day"), stepDay) || stepDay < 0)
        return fail("step_day", "missing, not an integer or negative");
    if (!ctx.Read(StringCRC("pending_day"), pendingDay) || pendingDay < -1)
        return fail("pending_day", "missing, not an integer or below -1");
    if (!ctx.Read(StringCRC("latest_day"), latestDay) || latestDay < 0)
        return fail("latest_day", "missing, not an integer or negative");

    int32_t stageCount = 0;
    if (!ctx.Read(StringCRC("stage_count"), stageCount) || stageCount < 0)
        return fail("stage_count", "missing, not an integer or negative");

    // Count the stage_N objects actually present.
    const Json::Value& node = ctx.CurrentNode();
    uint32_t present = 0;
    for (const std::string& member : node.getMemberNames())
    {
        if (member.compare(0, 6, "stage_") == 0 && node[member].isObject())
            ++present;
    }
    if (present != static_cast<uint32_t>(stageCount))
        return fail("stage_count", "does not match the number of stage_N objects");
    if (present > WorldStep::kMaxStages)
        return fail("stage_count", "more stages than the chain can hold");

    SavedStages saved;
    for (uint32_t i = 0; i < present; ++i)
    {
        char name[16];
        std::snprintf(name, sizeof(name), "stage_%u", i);
        if (!ctx.BeginObject(StringCRC(name)))
            return fail(name, "missing (stage objects must be numbered from 0)");
        int64_t id = 0;
        const bool idOk = ctx.Read(StringCRC("id"), id);
        ctx.EndObject();
        if (!idOk)
            return fail(name, "no integer 'id'");
        for (uint32_t j = 0; j < saved.count; ++j)
        {
            if (saved.ids[j] == id)
                return fail(name, "duplicate stage id");
        }
        saved.ids[saved.count++] = id;
    }

    // Every enabled stage (registered and in order) needs its blob, and the blob must pass the stage's own pre-check.
    for (uint32_t i = 0; i < mChain.EnabledCount(); ++i)
    {
        const StringCRC id = mChain.StageIdAt(i);
        const int index = FindSaved(saved, id.Value());
        if (index < 0)
            return fail("stage_N", "no blob for a stage that is in the configured order", id.AsChar());
        char name[16];
        std::snprintf(name, sizeof(name), "stage_%d", index);
        if (!ctx.BeginObject(StringCRC(name)))
            return fail(name, "missing", id.AsChar());
        const bool stageOk = mChain.StageAt(i)->ValidatePending(ctx);
        ctx.EndObject();
        if (!stageOk)
            return fail(name, "the stage rejected its pending data", id.AsChar());
    }
    return true;
}

void WorldStepModule::DeserializePending(Dia::SaveGame::LoadContext& ctx)
{
    DIA_TRACE_ZONE("WorldStepModule::DeserializePending", Dia::Observation::Trace::Category::kNone);

    int32_t stageCount = 0;
    int64_t stepDay = 0, pendingDay = -1, latestDay = 0;
    if (!ctx.Read(StringCRC("step_day"), stepDay) || !ctx.Read(StringCRC("pending_day"), pendingDay)
        || !ctx.Read(StringCRC("latest_day"), latestDay) || !ctx.Read(StringCRC("stage_count"), stageCount)
        || stageCount < 0 || static_cast<uint32_t>(stageCount) > WorldStep::kMaxStages)
    {
        DIA_LOG_ERROR(kLog, "WorldStepModule: pending record unreadable after validation, nothing restored");
        mRestoreFailed = true;
        UpdateHealth();
        return;
    }

    // The scheduler discarded the old step on LoadStarted; the chain still shows the old step's phase.
    if (mChain.Phase() != WorldStep::ChainPhase::Idle)
    {
        mChain.Abandon();
        mStepAccumNs.store(0);
    }

    SavedStages saved;
    for (int32_t i = 0; i < stageCount; ++i)
    {
        char name[16];
        std::snprintf(name, sizeof(name), "stage_%d", i);
        int64_t id = 0;
        if (ctx.BeginObject(StringCRC(name)))
        {
            ctx.Read(StringCRC("id"), id);
            ctx.EndObject();
        }
        saved.ids[saved.count++] = id;
    }

    // Per-stage delegation in declared order.
    bool stageFailed = false;
    for (uint32_t i = 0; i < mChain.EnabledCount(); ++i)
    {
        const int index = FindSaved(saved, mChain.StageIdAt(i).Value());
        if (index < 0)
            continue;
        char name[16];
        std::snprintf(name, sizeof(name), "stage_%d", index);
        if (!ctx.BeginObject(StringCRC(name)))
            continue;
        WorldStep::IWorldStepStage* stage = mChain.StageAt(i);
        stage->DeserializePending(ctx);
        ctx.EndObject();
        if (!stage->IsReady())
        {
            stageFailed = true;
            DIA_LOG_ERROR(kLog, "WorldStepModule: stage '%s' could not restore its pending result", mChain.StageIdAt(i).AsChar());
        }
    }

    // Blobs for stages that are not enabled here (not in order, or configured but not registered): skipped, one Warning each.
    for (uint32_t k = 0; k < saved.count; ++k)
    {
        bool enabled = false;
        for (uint32_t i = 0; i < mChain.EnabledCount(); ++i)
            enabled = enabled || static_cast<int64_t>(mChain.StageIdAt(i).Value()) == saved.ids[k];
        if (enabled)
            continue;
        const char* known = "unknown";
        for (uint32_t i = 0; i < mConfig.order.Size(); ++i)
        {
            if (static_cast<int64_t>(mConfig.order[i].crc.Value()) == saved.ids[k])
                known = mConfig.order[i].name.AsCStr();
        }
        DIA_LOG_WARNING(kLog, "WorldStepModule: pending blob for stage id %lld (%s) skipped, that stage is not enabled in this world",
                        static_cast<long long>(saved.ids[k]), known);
    }

    mStepDay = static_cast<uint64_t>(stepDay);
    mPendingDay = pendingDay;
    mLatestDay = static_cast<uint64_t>(latestDay);
    mHasTick = true;   // keep the restored key until a later tick overtakes it

    if (!mChain.RestoreRan(static_cast<StepKey>(stepDay)))
    {
        DIA_LOG_ERROR(kLog, "WorldStepModule: the chain could not restore step day %lld (a stage is missing or not ready); the pending step will be ignored at its apply key",
                      static_cast<long long>(stepDay));
        stageFailed = true;
    }
    if (stageFailed)
    {
        mRestoreFailed = true;
        UpdateHealth();
    }
    else
    {
        DIA_LOG_INFO(kLog, "WorldStepModule: pending step day %lld restored (%d stage blob(s), pending week %lld, key %lld)",
                     static_cast<long long>(stepDay), static_cast<int>(stageCount), static_cast<long long>(pendingDay),
                     static_cast<long long>(latestDay));
    }
}


// ---------------------------------------------------------------------------------------------
// History signal
// ---------------------------------------------------------------------------------------------

bool WorldStepModule::GuardReentry(const char* what)
{
    if (!mRaising)
        return true;
    DIA_LOG_ERROR(kLog, "WorldStepModule: %s called from inside the history signal, ignored", what);
    DIA_ASSERT(false, "WorldStepModule: re-entrant call from a world-step listener");
    return false;
}

bool WorldStepModule::RegisterListener(WorldStep::IWorldStepListener& listener)
{
    if (!GuardReentry("RegisterListener"))
        return false;
    for (uint32_t i = 0; i < mListenerCount; ++i)
    {
        if (mListeners[i] == &listener)
            return false;
    }
    if (mListenerCount == kMaxListeners)
    {
        DIA_LOG_WARNING(kLog, "WorldStepModule: listener rejected, %u already registered", kMaxListeners);
        return false;
    }
    mListeners[mListenerCount++] = &listener;
    return true;
}

void WorldStepModule::UnregisterListener(WorldStep::IWorldStepListener& listener)
{
    if (!GuardReentry("UnregisterListener"))
        return;
    for (uint32_t i = 0; i < mListenerCount; ++i)
    {
        if (mListeners[i] == &listener)
        {
            for (uint32_t j = i + 1; j < mListenerCount; ++j)
                mListeners[j - 1] = mListeners[j];
            mListeners[--mListenerCount] = nullptr;
            return;
        }
    }
}

void WorldStepModule::RaiseApplied(uint64_t applyKey, uint64_t stepDay)
{
    ++mHistorySeamCalls;   // counted even with no listener, so tests can prove a discarded step never reaches it
    if (mListenerCount == 0)
        return;
    WorldStep::WorldStepApplied applied;
    applied.applyKey = applyKey;
    applied.stepDay = stepDay;
    applied.stageCount = mChain.EnabledCount();
    mRaising = true;
    for (uint32_t i = 0; i < mListenerCount; ++i)
        mListeners[i]->OnWorldStepApplied(applied);
    mRaising = false;
}

void WorldStepModule::NoteApplied(uint64_t applyKey)
{
    mLastStepMs = static_cast<double>(mStepAccumNs.load()) / 1.0e6;
    mStepAccumNs.store(0);
    mLastApplyKey = applyKey;
    ++mApplyCount;
    mRestoreFailed = false;   // a real step landed: the restore problem, if any, is behind us
}

// ---------------------------------------------------------------------------------------------
// Chain observer: per-stage profile scopes and CPU summation
// ---------------------------------------------------------------------------------------------

void WorldStepModule::OnStageBegin(uint32_t index, WorldStep::StagePhase phase)
{
    const char* name = "WorldStep.Stage.Commit";
    if (phase == WorldStep::StagePhase::Capture)
        name = "WorldStep.Stage.Capture";
    else if (phase == WorldStep::StagePhase::Run)
        name = "WorldStep.Stage.Run";
    mZone.emplace(StringCRC(name), Dia::Observation::Profile::Category::kNone,
                  static_cast<uint32_t>(mChain.StageIdAt(index).Value()));
    mStageBeginNs = NowNs();
}

void WorldStepModule::OnStageEnd(uint32_t /*index*/, WorldStep::StagePhase /*phase*/)
{
    const uint64_t elapsed = NowNs() - mStageBeginNs;
    mStepAccumNs.fetch_add(elapsed, std::memory_order_relaxed);
    mZone.reset();
}

// ---------------------------------------------------------------------------------------------
// Telemetry and health
// ---------------------------------------------------------------------------------------------

void WorldStepModule::PublishTelemetry()
{
    // A stall is counted by the scheduler right after Apply, so it is picked up here, in the same sim step.
    if (mScheduler != nullptr && mChannelRegistered)
    {
        const Dia::DeferredStep::SchedulerStats& stats = mScheduler->GetStats();
        if (stats.stalls > mSeenStalls)
        {
            const uint64_t newStalls = stats.stalls - mSeenStalls;
            mLastStallMs = static_cast<double>(stats.totalStallMicros - mSeenStallMicros) / 1000.0;
            mSeenStalls = stats.stalls;
            mSeenStallMicros = stats.totalStallMicros;
            mPendingBad = true;
            mBadReason = "step_stall";
            if (mStallsCounter != nullptr)
                mStallsCounter->Inc(newStalls);
            DIA_LOG_WARNING(kLog, "WorldStepModule: the step stalled SimPU for %.2f ms at its apply key", mLastStallMs);
        }
    }

    bool applied = false;
    while (mPublishedApplies < mApplyCount)
    {
        mBadBits = ((mBadBits << 1) | (mPendingBad ? 1u : 0u)) & ((1u << kHealthWindow) - 1u);
        mPendingBad = false;
        ++mPublishedApplies;
        applied = true;
    }
    if (applied)
    {
        double slack = 0.0;
        Dia::DeferredStep::ChannelStats channel;
        if (mConfig.deferred && mScheduler != nullptr && mChannelRegistered && mScheduler->GetChannelStats(kChannelId, channel))
            slack = static_cast<double>(channel.lastSlackKeys);
        mLastSlackDays = slack;
        if (mStepMsGauge != nullptr)
            mStepMsGauge->Set(mLastStepMs);
        if (mSlackGauge != nullptr)
            mSlackGauge->Set(slack);
    }

    if (mLateKicksCounter != nullptr && mLateKicks > mPublishedLate)
    {
        mLateKicksCounter->Inc(mLateKicks - mPublishedLate);
        mPublishedLate = mLateKicks;
    }
    if (mLostWeeksCounter != nullptr && mLostWeeks > mPublishedLost)
    {
        mLostWeeksCounter->Inc(mLostWeeks - mPublishedLost);
        mPublishedLost = mLostWeeks;
    }
    UpdateHealth();
}

void WorldStepModule::UpdateHealth()
{
    if (!mRunning)
        return;
    if (mMissingCount > 0)
        mHealth.SetFailing(StringCRC("stage_missing"));
    else if (mRetiredCount > 0)
        mHealth.SetDegraded(StringCRC("stage_retired"));
    else if (mRestoreFailed)
        mHealth.SetDegraded(StringCRC("restore_failed"));
    else if (mBadBits != 0 || mPendingBad)
        mHealth.SetDegraded(StringCRC(mBadReason));
    else
        mHealth.SetOK();
}

// ---------------------------------------------------------------------------------------------
// Sim-hosted names
// ---------------------------------------------------------------------------------------------

bool WorldStepModule::RegisterNames()
{
    using Dia::API::ValueParam;
    using Dia::DebugServer::SimHelpInfo;

    static const ValueParam kStepParams[] = {
        { "action", "string", false, "kick (default): force one kick now, ignoring the week gate; set_mode: switch Sync/Deferred" },
        { "mode",   "string", false, "With action set_mode: sync or deferred" } };

    SimHelpInfo statusHelp;
    statusHelp.description = "World step status: mode, k, stages in order with readiness, retired and missing stages, day keys, pending week and in-flight state";
    statusHelp.category = "worldstep";
    statusHelp.owner = "WorldStepModule";
    statusHelp.noArgs = true;

    SimHelpInfo stepHelp;
    stepHelp.description = "Force one world step now regardless of the week gate (refused while a step is in flight, a week is pending or stages are not ready), or switch Sync/Deferred mode";
    stepHelp.category = "worldstep";
    stepHelp.owner = "WorldStepModule";
    stepHelp.params = kStepParams;
    stepHelp.paramCount = 2;

    const bool statusOk = mQueryHost->RegisterQuery(kStatusQueryName, statusHelp,
        [this](const Json::Value&) { return BuildStatusPayload(); });
    if (!statusOk)
    {
        DIA_LOG_ERROR(kLog, "WorldStepModule: registration of %s was rejected by the sim query host", kStatusQueryName);
        return false;
    }
    const bool stepOk = mQueryHost->RegisterCommand(kStepCommandName, stepHelp,
        [this](const Json::Value& args) { return RunStepCommand(args); });
    if (!stepOk)
    {
        // All or nothing: remove only what this call registered.
        mQueryHost->UnregisterQuery(kStatusQueryName);
        DIA_LOG_ERROR(kLog, "WorldStepModule: registration of %s was rejected, %s rolled back",
                      kStepCommandName, kStatusQueryName);
        return false;
    }
    mNamesRegistered = true;
    return true;
}

void WorldStepModule::UnregisterNames()
{
    if (!mNamesRegistered)
        return;
    if (mQueryHost != nullptr)
    {
        mQueryHost->UnregisterQuery(kStatusQueryName);
        mQueryHost->UnregisterQuery(kStepCommandName);
    }
    mNamesRegistered = false;
}

Json::Value WorldStepModule::BuildStatusPayload() const
{
    Json::Value v(Json::objectValue);
    v["success"] = true;
    v["mode"] = mConfig.deferred ? "deferred" : "sync";
    v["applyAfterDays"] = static_cast<Json::UInt>(mConfig.applyAfterDays);
    v["running"] = mRunning;
    v["ready"] = mChain.AllReady();

    Json::Value stages(Json::arrayValue);
    for (uint32_t i = 0; i < mChain.EnabledCount(); ++i)
    {
        Json::Value s(Json::objectValue);
        s["id"] = mChain.StageIdAt(i).AsChar();
        const WorldStep::IWorldStepStage* stage = mChain.StageAt(i);
        s["ready"] = stage != nullptr && stage->IsReady();
        stages.append(s);
    }
    v["stages"] = stages;

    Json::Value retired(Json::arrayValue);
    Json::Value missing(Json::arrayValue);
    WorldStep::StageIdList absent;
    mChain.MissingStages(absent);
    for (uint32_t i = 0; i < absent.Size(); ++i)
    {
        if (EverRegistered(absent[i].crc))
            retired.append(absent[i].name.AsCStr());
        else
            missing.append(absent[i].name.AsCStr());
    }
    v["retired"] = retired;
    v["missing"] = missing;

    v["latestDay"] = static_cast<Json::UInt64>(mLatestDay);
    v["lastKickDay"] = mHasKicked ? Json::Value(static_cast<Json::UInt64>(mLastKickDay)) : Json::Value();
    v["lastApplyKey"] = mApplyCount > 0 ? Json::Value(static_cast<Json::UInt64>(mLastApplyKey)) : Json::Value();
    v["pendingWeek"] = mPendingDay >= 0 ? Json::Value(static_cast<Json::Int64>(mPendingDay)) : Json::Value();
    v["inFlight"] = WorldStepInFlight();
    v["lastStallMs"] = mLastStallMs;
    v["kicks"] = static_cast<Json::UInt64>(mKickCount);
    v["applies"] = static_cast<Json::UInt64>(mApplyCount);
    v["lateKicks"] = static_cast<Json::UInt64>(mLateKicks);
    v["lostWeeks"] = static_cast<Json::UInt64>(mLostWeeks);
    return v;
}

Json::Value WorldStepModule::RunStepCommand(const Json::Value& args)
{
    std::string action = "kick";
    std::string mode;
    if (args.isObject())
    {
        if (args.isMember("action") && args["action"].isString())
            action = args["action"].asString();
        if (args.isMember("mode") && args["mode"].isString())
            mode = args["mode"].asString();
    }

    if (!GuardReentry("RunStepCommand"))
        return Refuse("called from inside the history signal");
    if (!mRunning)
        return Refuse("the world step module is not running");

    if (action == "set_mode")
    {
        if (mode != "sync" && mode != "deferred")
            return Refuse("mode must be \"sync\" or \"deferred\"");
        const char* reason = nullptr;
        if (!SetDeferred(mode == "deferred", &reason))
            return Refuse(reason != nullptr ? reason : "mode switch refused");
        Json::Value ok(Json::objectValue);
        ok["success"] = true;
        ok["mode"] = mConfig.deferred ? "deferred" : "sync";
        return ok;
    }
    if (action != "kick")
        return Refuse("unknown action, expected \"kick\" or \"set_mode\"");

    if (mMissingCount > 0 || !mChain.AllReady())
        return Refuse("stages are not ready");

    Json::Value ok(Json::objectValue);
    if (!mConfig.deferred)
    {
        // Synchronous: there is no in-flight state, so the step runs inline regardless of the week gate.
        if (!RunSynchronousStep(mLatestDay))
            return Refuse("the synchronous step was refused by the chain");
        ok["success"] = true;
        ok["mode"] = "sync";
        ok["day"] = static_cast<Json::UInt64>(mLatestDay);
        return ok;
    }

    if (WorldStepInFlight())
        return Refuse("a step is in flight");
    if (mPendingDay >= 0)
        return Refuse("a missed week is pending");
    if (TryKick(mLatestDay, mLatestDay) != KickOutcome::Launched)
        return Refuse("the scheduler rejected the kick");
    ok["success"] = true;
    ok["mode"] = "deferred";
    ok["day"] = static_cast<Json::UInt64>(mLatestDay);
    ok["applyKey"] = static_cast<Json::UInt64>(mKickApplyKey);
    return ok;
}

} // namespace CoW

namespace { using WorldStepModule_ = CoW::WorldStepModule; }
DIA_MODULE(WorldStepModule_);
DIA_DESCRIBE(WorldStepModule_::kTypeId, "World step coordinator: kicks the chained world-sim stages once per game week on the world deferred-step channel.");
