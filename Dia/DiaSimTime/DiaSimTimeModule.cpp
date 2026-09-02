#include <DiaSimTime/DiaSimTimeModule.h>

#include <DiaSimTime/ISimTimeBudgetedSystem.h>
#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/ProcessingUnit.h>
#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaObservation/Metric/MetricRegistry.h>
#include <DiaObservation/Metric/Gauge.h>
#include <DiaObservation/Metric/Counter.h>

#include <json/json.h>

namespace Dia::SimTime {

    const Dia::Core::StringCRC DiaSimTimeModule::kTypeId("DiaSimTime");

    DiaSimTimeModule::DiaSimTimeModule(const Dia::Core::StringCRC& instanceId)
        : Dia::ApplicationFlow::SimModule(instanceId)
        , mScheduler()
        , mBudget()
        , mRegistry(mScheduler)
    {
    }

    // --- Config ---------------------------------------------------------------
    void DiaSimTimeModule::OnConfigure(const char* configJson)
    {
        if (!configJson || configJson[0] == '\0')
        {
            return;
        }

        Json::Value  root;
        Json::Reader reader;
        if (!reader.parse(configJson, root))
        {
            return;
        }

        // budget: per-tier per-tick ms. Missing fields keep SimTimeBudget's own
        // documented defaults.
        if (root.isMember("budget") && root["budget"].isObject())
        {
            const Json::Value& b = root["budget"];
            const float criticalMs   = b.isMember("criticalMs")   && b["criticalMs"].isNumeric()   ? b["criticalMs"].asFloat()   : SimTimeBudget::kDefaultCriticalMs;
            const float highMs       = b.isMember("highMs")       && b["highMs"].isNumeric()       ? b["highMs"].asFloat()       : SimTimeBudget::kDefaultHighMs;
            const float normalMs     = b.isMember("normalMs")     && b["normalMs"].isNumeric()     ? b["normalMs"].asFloat()     : SimTimeBudget::kDefaultNormalMs;
            const float backgroundMs = b.isMember("backgroundMs") && b["backgroundMs"].isNumeric() ? b["backgroundMs"].asFloat() : SimTimeBudget::kDefaultBackgroundMs;
            mBudget.SetTierBudgets(criticalMs, highMs, normalMs, backgroundMs);
        }

        // tier_hz: LOD-tier Hz for kHigh/kMedium/kLow. A missing field is passed
        // as a non-positive Hz, which SetTierHz treats as "leave unchanged" so
        // the registry's ctor default is preserved.
        if (root.isMember("tier_hz") && root["tier_hz"].isObject())
        {
            const Json::Value& t = root["tier_hz"];
            const float highHz   = t.isMember("high")   && t["high"].isNumeric()   ? t["high"].asFloat()   : -1.0f;
            const float mediumHz = t.isMember("medium") && t["medium"].isNumeric() ? t["medium"].asFloat() : -1.0f;
            const float lowHz    = t.isMember("low")    && t["low"].isNumeric()    ? t["low"].asFloat()    : -1.0f;
            mRegistry.SetTierHz(highHz, mediumHz, lowHz);
        }
    }

    // --- Lifecycle ------------------------------------------------------------
    Dia::ApplicationFlow::StartResult DiaSimTimeModule::DoStart()
    {
        auto& registry = Dia::Observation::Metric::MetricRegistry::Instance();

        // simtime.budget.* are registered internally by SimTimeBudget itself.
        mMetricQueueDepth    = registry.RegisterGauge  (Dia::Core::StringCRC("simtime.scheduler.queue_depth"));
        mMetricSleepingCount = registry.RegisterGauge  (Dia::Core::StringCRC("simtime.registry.sleeping_count"));
        mMetricTick          = registry.RegisterCounter(Dia::Core::StringCRC("simtime.tick"));

        // TODO(Task 4.7): register with SaveRegistry / build SimTimeSaveState here.

        return Dia::ApplicationFlow::StartResult::kReady;
    }

    void DiaSimTimeModule::DoUpdate(const SimTimeContext& ctx)
    {
        // 1. Advance all non-world clocks (world was already advanced this
        //    Update() by the owning ProcessingUnit).
        GetProcessingUnit()->GetDomainRegistry().TickAll();

        // 2. Fire scheduled events into the SimTimeSchedulerFire EventStream.
        mScheduler.Tick(ctx.gameTime);

        // 2b. Drain wake-on-time fires so systems woken this tick are eligible.
        mRegistry.ProcessWakeEvents();

        // 3. Partition the per-tick CPU budget by priority tier.
        mBudget.AllocateTick(ctx);

        // 4. Gate loop (Pattern C): sleep -> LOD tier -> budget capacity -> run.
        const int count = mRegistry.GetRegisteredCount();
        for (int i = 0; i < count; ++i)
        {
            const SimTimeRegistryEntryView entry = mRegistry.GetEntryAt(i);
            if (entry.system == nullptr)                                           continue;
            if (mRegistry.GetState(entry.systemId) == SimTimeState::kSleeping)     continue;   // 4a
            if (!mRegistry.DueThisTick(entry.systemId, ctx.gameTime))             continue;   // 4b
            if (mBudget.RunIfCapacity(entry.system))                                            // 4c
            {
                mRegistry.MarkRan(entry.systemId, ctx.gameTime);
            }
        }

        // Metrics.
        ++mTickCounter;
        if (mMetricTick)          mMetricTick->Inc();
        if (mMetricQueueDepth)    mMetricQueueDepth->Set(static_cast<double>(mScheduler.GetQueueDepth()));
        if (mMetricSleepingCount) mMetricSleepingCount->Set(static_cast<double>(mRegistry.GetSleepingCount()));

        // 5. Publish the SimTimeContext onto the "SimTime" FrameStream.
        mSimTimeWriter.Write(ctx, ctx.gameTime);
    }

    Dia::ApplicationFlow::StopResult DiaSimTimeModule::DoStop()
    {
        mMetricQueueDepth    = nullptr;
        mMetricSleepingCount = nullptr;
        mMetricTick          = nullptr;
        return Dia::ApplicationFlow::StopResult::kDone;
    }

    void DiaSimTimeModule::OnConnectStreams(Dia::ApplicationFlow::Application& app)
    {
        // mScheduler creates the SimTimeSchedulerFire EventStreamStore; mRegistry
        // finds that same store and registers another reader slot on it (intended
        // multi-reader fan-out). mSimTimeWriter wires the "SimTime" FrameStream.
        mScheduler.Connect(app);
        mRegistry.Connect(app);
        mSimTimeWriter.Connect(app);
    }

    // --- Public delegation ----------------------------------------------------
    void DiaSimTimeModule::Register(ISimTimeBudgetedSystem* system, const SimTimePolicy& policy)
    {
        mRegistry.Register(system, policy);   // sleep / LOD state
        mBudget.Register(system);             // CPU-budget gating (independent sibling)
    }

    void DiaSimTimeModule::Unregister(ISimTimeBudgetedSystem* system)
    {
        mRegistry.Unregister(system);
        mBudget.Unregister(system);
    }

    void DiaSimTimeModule::Sleep(Dia::Core::StringCRC systemId)            { mRegistry.Sleep(systemId); }
    void DiaSimTimeModule::Wake(Dia::Core::StringCRC systemId)             { mRegistry.Wake(systemId); }
    SimTimeState DiaSimTimeModule::GetState(Dia::Core::StringCRC systemId) const { return mRegistry.GetState(systemId); }

    void DiaSimTimeModule::RegisterWakeOnTime(Dia::Core::StringCRC systemId, Dia::Core::TimeAbsolute at)
    {
        mRegistry.RegisterWakeOnTime(systemId, at);
    }

    void DiaSimTimeModule::RegisterWakeOnMessage(Dia::Core::StringCRC systemId, Dia::Core::StringCRC eventType)
    {
        mRegistry.RegisterWakeOnMessage(systemId, eventType);
    }

    Dia::SimTime::SimTimeDomainRegistry& DiaSimTimeModule::GetDomainRegistry()
    {
        return GetProcessingUnit()->GetDomainRegistry();
    }

    SimTimeScheduler& DiaSimTimeModule::GetScheduler()
    {
        return mScheduler;
    }

} // namespace Dia::SimTime

namespace { using DiaSimTimeModule_ = Dia::SimTime::DiaSimTimeModule; }
DIA_MODULE(DiaSimTimeModule_);
DIA_DESCRIBE(DiaSimTimeModule_::kTypeId, "Umbrella SimPU module: assembles the sim-time scheduler, budget, and registry into the per-tick gate loop and publishes SimTimeContext onto the SimTime FrameStream.");
