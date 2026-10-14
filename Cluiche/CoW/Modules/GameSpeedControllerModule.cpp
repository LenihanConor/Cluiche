#include "Modules/GameSpeedControllerModule.h"

#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaApplicationFlow/ProcessingUnit.h>
#include <DiaCore/SimTime/SimTimeDomain.h>
#include <DiaCore/Json/external/json/json.h>
#include <DiaObservation/Health/HealthRegistry.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Profile/DiaProfile.h>
#include <DiaObservation/Trace/DiaTrace.h>

namespace CoW {

const Dia::Core::StringCRC GameSpeedControllerModule::kTypeId("GameSpeedControllerModule");

namespace {
    using Dia::Observation::Metric::MetricRegistry;

    // Reads "wall_seconds_per_day": { "slow", "normal", "fast", "fastest" }. Missing keys keep defaults.
    // Returns false if the block is present but malformed.
    bool ParseConfig(const Json::Value& root, SpeedPresetConfig& out)
    {
        if (!root.isMember("wall_seconds_per_day"))
            return true;
        const Json::Value& block = root["wall_seconds_per_day"];
        if (!block.isObject())
            return false;

        static const char* const kKeys[4] = { "slow", "normal", "fast", "fastest" };
        for (int i = 0; i < 4; ++i)
        {
            if (!block.isMember(kKeys[i]))
                continue;
            if (!block[kKeys[i]].isNumeric())
                return false;
            out.wallSecondsPerDay[i] = block[kKeys[i]].asFloat();
        }
        return true;
    }
}

GameSpeedControllerModule::GameSpeedControllerModule(const Dia::Core::StringCRC& instanceId)
    : SimModule(instanceId)
{}

// Out of line so the debug domain type is complete where it is destroyed.
GameSpeedControllerModule::~GameSpeedControllerModule() = default;

bool GameSpeedControllerModule::RequestSpeed(SpeedTier tier)
{
    Cluiche::AppFlow::MessageBusHostModule* host = mBus.Get();
    if (!host)
        return false;
    SpeedChangeRequested request;
    request.tier = tier;
    return host->GetBus().Broadcast(request);
}

void GameSpeedControllerModule::OnConfigure(const char* configJson)
{
    mPendingConfig = DefaultSpeedPresetConfig();
    mConfigParseFailed = false;
    if (configJson == nullptr || configJson[0] == '\0')
        return;

    Json::Value root;
    Json::Reader reader;
    if (!reader.parse(configJson, root) || !root.isObject() || !ParseConfig(root, mPendingConfig))
    {
        mConfigParseFailed = true;
        mPendingConfig = DefaultSpeedPresetConfig();
    }
}

Dia::ApplicationFlow::StartResult GameSpeedControllerModule::DoStart()
{
    using Dia::ApplicationFlow::StartResult;
    DIA_TRACE_ZONE("GameSpeedControllerModule::DoStart", Dia::Observation::Trace::Category::kNone);

    Cluiche::AppFlow::MessageBusHostModule* host = mBus.Get();
    if (!host)
        return StartResult::kLoading;

    auto& metrics = MetricRegistry::Instance();
    mDayGauge              = metrics.RegisterGauge(Dia::Core::StringCRC("cow.gamespeed.day"));
    mTierGauge             = metrics.RegisterGauge(Dia::Core::StringCRC("cow.gamespeed.tier"));
    mPausedGauge           = metrics.RegisterGauge(Dia::Core::StringCRC("cow.gamespeed.paused"));
    mModalDepthGauge       = metrics.RegisterGauge(Dia::Core::StringCRC("cow.gamespeed.modal_depth"));
    mCarryGauge            = metrics.RegisterGauge(Dia::Core::StringCRC("cow.gamespeed.carry_days"));
    mTickCounter           = metrics.RegisterCounter(Dia::Core::StringCRC("cow.gamespeed.ticks"));
    mStepCounter           = metrics.RegisterCounter(Dia::Core::StringCRC("cow.gamespeed.steps"));
    mStepRejectedCounter   = metrics.RegisterCounter(Dia::Core::StringCRC("cow.gamespeed.steps_rejected"));
    mSpeedChangeCounter    = metrics.RegisterCounter(Dia::Core::StringCRC("cow.gamespeed.speed_changes"));
    mPublishDroppedCounter = metrics.RegisterCounter(Dia::Core::StringCRC("cow.gamespeed.tick_publish_dropped"));
    mClockBackwardsCounter = metrics.RegisterCounter(Dia::Core::StringCRC("cow.gamespeed.clock_backwards"));
    mConfigFallbackCounter = metrics.RegisterCounter(Dia::Core::StringCRC("cow.gamespeed.config_fallbacks"));

    // Validate the preset config against the SimPU rate; fall back to defaults if invalid.
    const float simHz = GetProcessingUnit() ? GetProcessingUnit()->GetFrequencyHz() : 30.0f;
    SpeedPresetConfig config = mPendingConfig;
    mUsingFallbackConfig = false;
    if (mConfigParseFailed)
    {
        DIA_LOG_ERROR("CoW", "GameSpeedControllerModule: config block malformed, using default presets");
        mUsingFallbackConfig = true;
    }
    else
    {
        const PresetConfigError error = ValidateSpeedPresetConfig(config, simHz);
        if (error != PresetConfigError::None)
        {
            DIA_LOG_ERROR("CoW", "GameSpeedControllerModule: invalid preset config (%s) at %.1f Hz, using default presets",
                          PresetConfigErrorName(error), simHz);
            mUsingFallbackConfig = true;
        }
    }
    if (mUsingFallbackConfig)
    {
        config = DefaultSpeedPresetConfig();
        mConfigFallbackCounter->Inc();
    }
    mController = GameSpeedController(config);
    mDayClock.Reset();
    mAppliedValid = false;
    mStepPending = false;
    mClockResetPending = true;
    mPublishFailed = false;
    mBackwardsSeen = 0;

    Dia::MessageBus::Bus& bus = host->GetBus();
    bus.RegisterType<SimulationTick, 16>();
    bus.RegisterType<SpeedChangeRequested, 16>();
    bus.RegisterType<ManualAdvanceRequested, 16>();
    bus.RegisterType<ModalOpened, 16>();
    bus.RegisterType<ModalClosed, 16>();
    bus.RegisterType<WorldGenerationComplete, 4>();
    bus.RegisterProducer<SimulationTick>(GetInstanceId());

    mSubscriptions[0] = bus.Subscribe<SpeedChangeRequested>(GetInstanceId(),
        [this](const SpeedChangeRequested& msg) {
            const GameSpeedState before = mController.GetState();
            mController.OnSpeedChangeRequested(msg.tier);
            LogTransition("SpeedChangeRequested", before);
        });
    mSubscriptions[1] = bus.Subscribe<ManualAdvanceRequested>(GetInstanceId(),
        [this](const ManualAdvanceRequested&) {
            if (mController.OnManualAdvanceRequested())
            {
                mStepPending = true;
            }
            else
            {
                mStepRejectedCounter->Inc();
                DIA_LOG_WARNING("CoW", "GameSpeedControllerModule: manual advance rejected, game is running (%s)",
                                SpeedTierName(mController.GetState().speedTier));
            }
        });
    mSubscriptions[2] = bus.Subscribe<ModalOpened>(GetInstanceId(),
        [this](const ModalOpened&) {
            const GameSpeedState before = mController.GetState();
            mController.OnModalOpened();
            LogTransition("ModalOpened", before);
        });
    mSubscriptions[3] = bus.Subscribe<ModalClosed>(GetInstanceId(),
        [this](const ModalClosed&) {
            const GameSpeedState before = mController.GetState();
            mController.OnModalClosed();
            LogTransition("ModalClosed", before);
        });
    mSubscriptions[4] = bus.Subscribe<WorldGenerationComplete>(GetInstanceId(),
        [this](const WorldGenerationComplete&) {
            const GameSpeedState before = mController.GetState();
            mController.OnNewGame();
            mStepPending = false;
            mClockResetPending = true;
            LogTransition("WorldGenerationComplete", before);
        });

    bool subscriptionsOk = true;
    for (const auto& handle : mSubscriptions)
        subscriptionsOk = subscriptionsOk && handle.IsValid();
    if (!subscriptionsOk)
    {
        DIA_LOG_ERROR("CoW", "GameSpeedControllerModule: a bus subscription failed (handler pool full?)");
        mHealth.IncrementErrors();
    }
    mSubscriptionsFailed = !subscriptionsOk;

#ifdef DIA_DEBUG
    mDebugDomain = std::make_unique<GameSpeedDebugDomain>(*this);
    mDomainRegistered = false;
#endif

    Dia::Observation::Health::HealthRegistry::Instance().Register(&mHealth);
    mHealthRegistered = true;
    UpdateGauges();
    UpdateHealth();

    DIA_LOG_INFO("CoW", "GameSpeedControllerModule: started at %.1f Hz, presets slow=%.3f normal=%.3f fast=%.3f fastest=%.3f s/day%s",
                 simHz, config.wallSecondsPerDay[0], config.wallSecondsPerDay[1], config.wallSecondsPerDay[2],
                 config.wallSecondsPerDay[3], mUsingFallbackConfig ? " (fallback)" : "");
    return StartResult::kReady;
}

void GameSpeedControllerModule::RestoreState(const GameSpeedState& saved)
{
    const GameSpeedState before = mController.GetState();
    mController.RestoreState(saved);
    mStepPending = false;
    mClockResetPending = true;
    LogTransition("RestoreState", before);
}

void GameSpeedControllerModule::ApplyClockState()
{
    Dia::ApplicationFlow::ProcessingUnit* pu = GetProcessingUnit();
    if (!pu)
        return;
    Dia::SimTime::SimTimeDomain& domain = pu->GetWorldDomain();

    // Pause is scale 0, NOT SimTimeDomain::Pause(): a paused world domain stops the whole SimPU
    // forward pass, so no module (including this one and the bus) would run to resume it.
    // The scale takes effect on the following sim tick (TimeServer queues it).
    const float scale = mController.GetTimeScale();
    if (!mAppliedValid || scale != mAppliedScale)
    {
        domain.SetScale(scale);
        mAppliedScale = scale;
    }
    mAppliedValid = true;
}

void GameSpeedControllerModule::LogTransition(const char* cause, const GameSpeedState& before) const
{
    const GameSpeedState& now = mController.GetState();
    if (now.speedTier != before.speedTier || now.pauseSource != before.pauseSource
        || now.modalDepth != before.modalDepth || now.dayCounter != before.dayCounter)
    {
        DIA_LOG_INFO("CoW", "GameSpeedController: %s -> tier %s->%s, pause %d->%d, modalDepth %u->%u, day %llu",
                     cause, SpeedTierName(before.speedTier), SpeedTierName(now.speedTier),
                     static_cast<int>(before.pauseSource), static_cast<int>(now.pauseSource),
                     static_cast<unsigned>(before.modalDepth), static_cast<unsigned>(now.modalDepth),
                     static_cast<unsigned long long>(now.dayCounter));
        if (mSpeedChangeCounter && now.speedTier != before.speedTier)
            mSpeedChangeCounter->Inc();
    }
    else
    {
        DIA_LOG_DEBUG("CoW", "GameSpeedController: %s ignored (no state change)", cause);
    }
}

void GameSpeedControllerModule::PublishTick()
{
    Cluiche::AppFlow::MessageBusHostModule* host = mBus.Get();
    const uint64_t day = mController.AdvanceDay();

    SimulationTick tick;
    tick.day  = day;
    tick.band = ResolveBand(day);

    if (host && host->GetBus().Broadcast(tick))
    {
        mPublishFailed = false;
        mTickCounter->Inc();
        if (tick.band >= TimeBand::Season)
        {
            DIA_LOG_INFO("CoW", "GameSpeedController: day %llu (%s)", static_cast<unsigned long long>(day), TimeBandName(tick.band));
        }
    }
    else
    {
        mPublishFailed = true;
        mPublishDroppedCounter->Inc();
        mHealth.IncrementErrors();
        DIA_LOG_ERROR("CoW", "GameSpeedController: SimulationTick for day %llu could not be published",
                      static_cast<unsigned long long>(day));
    }
}

void GameSpeedControllerModule::UpdateGauges()
{
    const GameSpeedState& s = mController.GetState();
    mDayGauge->Set(static_cast<double>(s.dayCounter));
    mTierGauge->Set(static_cast<double>(static_cast<int>(s.speedTier)));
    mPausedGauge->Set(mController.IsPaused() ? 1.0 : 0.0);
    mModalDepthGauge->Set(static_cast<double>(s.modalDepth));
    mCarryGauge->Set(static_cast<double>(mDayClock.CarryDays()));
}

void GameSpeedControllerModule::UpdateHealth()
{
    if (mSubscriptionsFailed)
        mHealth.SetFailing(Dia::Core::StringCRC("subscription_failed"));
    else if (mPublishFailed)
        mHealth.SetFailing(Dia::Core::StringCRC("tick_publish_failed"));
    else if (mDayClock.CarryDays() > 0)
        mHealth.SetDegraded(Dia::Core::StringCRC("day_ticks_lagging"));
    else if (mUsingFallbackConfig)
        mHealth.SetDegraded(Dia::Core::StringCRC("fallback_preset_config"));
    else
        mHealth.SetOK();
}

void GameSpeedControllerModule::DoUpdate(const Dia::SimTime::SimTimeContext& ctx)
{
    DIA_TRACE_ZONE("GameSpeedControllerModule::DoUpdate", Dia::Observation::Trace::Category::kNone);
    DIA_PROFILE_SCOPE("GameSpeedControllerModule::DoUpdate", Dia::Observation::Trace::Category::kNone);

    if (mClockResetPending)
    {
        mDayClock.Reset();
        mClockResetPending = false;
    }

    // Bus handlers ran earlier this frame (the module depends on MessageBusHostModule).
    ApplyClockState();

    if (mStepPending)
    {
        mStepPending = false;
        if (Dia::ApplicationFlow::ProcessingUnit* pu = GetProcessingUnit())
        {
            // Advance exactly to the next day boundary; the tick is published on the next sim frame.
            pu->GetWorldDomain().Step(Dia::Core::TimeRelative::CreateFromMicroseconds(
                static_cast<float>(mDayClock.RemainingUs())));
            mStepCounter->Inc();
            DIA_LOG_DEBUG("CoW", "GameSpeedController: manual step to next day boundary");
        }
    }

    if (mDayClock.Advance(ctx.gameTime.AsLongLongInMicroseconds()))
        PublishTick();

    const uint32_t backwards = mDayClock.GetBackwardsCount();
    if (backwards != mBackwardsSeen)
    {
        mClockBackwardsCounter->Inc(backwards - mBackwardsSeen);
        DIA_LOG_WARNING("CoW", "GameSpeedController: game clock moved backwards (%u total)", backwards);
        mBackwardsSeen = backwards;
    }

    UpdateGauges();
    UpdateHealth();
#ifdef DIA_DEBUG
    TryRegisterDebugDomain();
#endif
}

#ifdef DIA_DEBUG
void GameSpeedControllerModule::TryRegisterDebugDomain()
{
    if (mDomainRegistered || !mDebugDomain)
        return;
    if (Cluiche::AppFlow::VisualDebuggerModule* vd = mVisualDebuggerRef.Get())
    {
        vd->RegisterDomain(*mDebugDomain);
        mDomainRegistered = true;
        DIA_LOG_INFO("CoW", "GameSpeedControllerModule: debug domain registered with the visual debugger");
    }
}

void GameSpeedControllerModule::UnregisterDebugDomain()
{
    if (!mDomainRegistered || !mDebugDomain)
        return;
    if (Cluiche::AppFlow::VisualDebuggerModule* vd = mVisualDebuggerRef.Get())
        vd->UnregisterDomain(*mDebugDomain);
    mDomainRegistered = false;
}
#endif

Dia::ApplicationFlow::StopResult GameSpeedControllerModule::DoStop()
{
#ifdef DIA_DEBUG
    UnregisterDebugDomain();
    mDebugDomain.reset();
#endif
    for (auto& handle : mSubscriptions)
        handle = Dia::MessageBus::BusSubscriptionHandle();

    if (mHealthRegistered)
    {
        Dia::Observation::Health::HealthRegistry::Instance().Unregister(&mHealth);
        mHealthRegistered = false;
    }
    mDayGauge = mTierGauge = mPausedGauge = mModalDepthGauge = mCarryGauge = nullptr;
    mTickCounter = mStepCounter = mStepRejectedCounter = mSpeedChangeCounter = nullptr;
    mPublishDroppedCounter = mClockBackwardsCounter = mConfigFallbackCounter = nullptr;

    DIA_LOG_INFO("CoW", "GameSpeedControllerModule: stopped at day %llu",
                 static_cast<unsigned long long>(mController.GetState().dayCounter));
    return Dia::ApplicationFlow::StopResult::kDone;
}

} // namespace CoW

namespace { using GameSpeedControllerModule_ = CoW::GameSpeedControllerModule; }
DIA_MODULE(GameSpeedControllerModule_);
DIA_DESCRIBE(GameSpeedControllerModule_::kTypeId, "Game speed presets, pause rules and daily SimulationTick.");
