////////////////////////////////////////////////////////////////////////////////
// Filename: ProcessingUnit.cpp
// DiaApplicationFlow — v2 ProcessingUnit
////////////////////////////////////////////////////////////////////////////////
#include "DiaApplicationFlow/ProcessingUnit.h"
#include "DiaApplicationFlow/TypeRegistry.h"

#include <DiaCore/Core/Assert.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Log/Logger.h>
#include <DiaObservation/Metric/Counter.h>
#include <DiaObservation/Metric/Gauge.h>
#include <DiaObservation/Metric/MetricRegistry.h>
#include <DiaObservation/Profile/DiaProfile.h>
#include <DiaObservation/Profile/Profiler.h>
#include <DiaObservation/Profile/TripReporter.h>
#include <DiaObservation/Trace/DiaTrace.h>

#include <chrono>
#include <string>

namespace Dia { namespace ApplicationFlow {

    //--------------------------------------------------------------------------
    ProcessingUnit::ProcessingUnit(const Dia::Core::StringCRC& instanceId,
                                   float frequencyHz,
                                   bool dedicatedThread,
                                   unsigned int maxCatchUpTicksPerFrame)
        : mInstanceId(instanceId)
        , mAffinity(PUAffinity::kAny)
        , mFrequencyHz(frequencyHz)
        , mDedicatedThread(dedicatedThread)
        , mModuleCount(0)
        // "world" is a literal here, not yet a shared constant — a later task
        // (SimTimeDomainRegistry) formally declares kWorldId = StringCRC{"world"};
        // this is the same literal, just not yet centralized.
        , mWorldDomain(Dia::Core::StringCRC("world"), frequencyHz, Dia::Core::TimeAbsolute::Zero())
        , mDomainRegistry(mWorldDomain)
        , mMaxCatchUpTicksPerFrame(maxCatchUpTicksPerFrame)
    {
        // Map well-known PU instance IDs to their affinity enum.
        if      (instanceId == Dia::Core::StringCRC("MainPU"))   mAffinity = PUAffinity::kMain;
        else if (instanceId == Dia::Core::StringCRC("SimPU"))    mAffinity = PUAffinity::kSim;
        else if (instanceId == Dia::Core::StringCRC("RenderPU")) mAffinity = PUAffinity::kRender;
        else                                                      mAffinity = PUAffinity::kAny;

        // Profile subject name (one-time; hot path only passes the CRC value).
        Dia::Observation::Profile::Profiler::RegisterSubjectName(mInstanceId.Value(), mInstanceId.AsChar());

        DIA_LOG_INFO("Application", "ProcessingUnit '%s' created (%.0fHz, dedicated=%d)",
                     mInstanceId.AsChar(), static_cast<double>(mFrequencyHz),
                     mDedicatedThread ? 1 : 0);

        // Task 34 — register per-PU tick-duration gauge
        {
            std::string metricName = "pu.";
            metricName += mInstanceId.AsChar();
            metricName += ".last_tick_ms";
            mMetricLastTickMs = Dia::Observation::Metric::MetricRegistry::Instance()
                                    .RegisterGauge(Dia::Core::StringCRC(metricName.c_str()));
        }

        // DiaSimTime (Task 1.4) — single global counter for sim-accumulator backlog
        // drops (matches the spec's metrics table key exactly). Registered
        // unconditionally (same simple pattern as the gauge above) even though it's
        // only ever incremented for kSim PUs.
        mMetricDroppedTicks = Dia::Observation::Metric::MetricRegistry::Instance()
                                  .RegisterCounter(Dia::Core::StringCRC("simtime.accumulator.dropped_ticks"));

        // Perf Inspector A2b: names built once, off the hot path.
        {
            std::string name = "pu.";
            name += mInstanceId.AsChar();
            name += ".catchup_updates";
            mMetricCatchupUpdates = Dia::Observation::Metric::MetricRegistry::Instance()
                                        .RegisterCounter(Dia::Core::StringCRC(name.c_str()));
            mMetricSelfMs = Dia::Observation::Metric::MetricRegistry::Instance()
                                .RegisterGauge(Dia::Core::StringCRC("perf.self_ms"));

            // Perf Inspector A3c: replaces the per-frame pu.over_budget log line.
            name = "pu.";
            name += mInstanceId.AsChar();
            name += ".over_budget_frames";
            mMetricOverBudgetFrames = Dia::Observation::Metric::MetricRegistry::Instance()
                                          .RegisterCounter(Dia::Core::StringCRC(name.c_str()));
        }
    }

    //--------------------------------------------------------------------------
    ProcessingUnit::~ProcessingUnit()
    {
        // UniquePtr members in mModules[] handle cleanup automatically
    }

    //--------------------------------------------------------------------------
    const Dia::Core::StringCRC& ProcessingUnit::GetInstanceId() const
    {
        return mInstanceId;
    }

    //--------------------------------------------------------------------------
    float ProcessingUnit::GetFrequencyHz() const
    {
        return mFrequencyHz;
    }

    //--------------------------------------------------------------------------
    bool ProcessingUnit::IsDedicatedThread() const
    {
        return mDedicatedThread;
    }

    //--------------------------------------------------------------------------
    void ProcessingUnit::AddModule(Dia::Core::UniquePtr<Module> module,
                                   float startTimeoutMs,
                                   float stopTimeoutMs)
    {
        DIA_ASSERT(module != nullptr, "ProcessingUnit::AddModule — module must not be null");
        DIA_ASSERT(mModuleCount < kMaxModules, "ProcessingUnit::AddModule — module capacity exceeded");

        {
            PUAffinity moduleAffinity = TypeRegistry::Global().GetAllowedPUs(module->GetTypeId());
            if (moduleAffinity != PUAffinity::kAny)
            {
                // A kAny-affinity PU (any custom-named PU — instance ID isn't
                // "MainPU"/"SimPU"/"RenderPU") has no declared role, so it can never
                // satisfy a module that DOES declare a specific role. Do not fall through
                // to the bitmask check below for this case — PUAffinity::kAny is 0xFF,
                // so HasAffinity(moduleAffinity, kAny) is unconditionally true and would
                // silently pass every role-specific module through.
                const bool mismatched = (mAffinity == PUAffinity::kAny)
                    ? true
                    : !HasAffinity(moduleAffinity, mAffinity);

                if (mismatched)
                {
                    DIA_LOG_ERROR("ApplicationFlow",
                        "Module '%s' placed on wrong PU '%s' (type '%s')",
                        module->GetInstanceId().AsChar(),
                        GetInstanceId().AsChar(),
                        module->GetTypeId().AsChar());
                    RELEASE_DIA_ASSERT(!mismatched,
                        "Module '%s' (type '%s') not allowed on PU '%s' — kAllowedPUs mismatch",
                        module->GetInstanceId().AsChar(),
                        module->GetTypeId().AsChar(),
                        GetInstanceId().AsChar());
                }
            }
        }

        ModuleEntry& entry = mModules[mModuleCount];
        entry.module          = std::move(module);
        entry.startTimeoutMs  = startTimeoutMs;
        entry.stopTimeoutMs   = stopTimeoutMs;

        entry.module->SetProcessingUnit(this);

        // Per-module EWMA gauge. MetricRegistry has no unregister: registered once here
        // (same name on stage re-entry returns the existing gauge), zeroed when the module stops.
        {
            std::string name = "pu.";
            name += mInstanceId.AsChar();
            name += ".module.";
            name += entry.module->GetInstanceId().AsChar();
            name += ".ms";
            entry.metricEwmaMs = Dia::Observation::Metric::MetricRegistry::Instance()
                                     .RegisterGauge(Dia::Core::StringCRC(name.c_str()));
        }

        ++mModuleCount;
    }

    //--------------------------------------------------------------------------
    Module* ProcessingUnit::FindModule(const Dia::Core::StringCRC& instanceId)
    {
        for (unsigned int i = 0; i < mModuleCount; ++i)
        {
            if (mModules[i].module->GetInstanceId() == instanceId)
                return mModules[i].module.Get();
        }
        return nullptr;
    }

    //--------------------------------------------------------------------------
    const Module* ProcessingUnit::FindModule(const Dia::Core::StringCRC& instanceId) const
    {
        for (unsigned int i = 0; i < mModuleCount; ++i)
        {
            if (mModules[i].module->GetInstanceId() == instanceId)
                return mModules[i].module.Get();
        }
        return nullptr;
    }

    //--------------------------------------------------------------------------
    // Update — two-pass tick respecting dependency order.
    //
    // The manifest declares modules in dependency order (dep before dependent),
    // so mModules[] is stored in the same order. That order is correct for
    // start/run but inverted for stop: a module's DoStop must run before its
    // dependencies' DoStop, otherwise the dependency may be torn down (e.g.
    // KernelModule destroying RenderWindow + TextureHandler) before the
    // dependent (AssetServiceModule's mRuntime.Reset) finishes using it.
    //
    // Pass 1 (forward): tick modules in kStarting/kActive — respects deps.
    // Pass 2 (reverse): tick modules in kStopping — respects reverse-deps.
    //
    // Modules in other states (kInactive/kFailed) are skipped; FrameTick is a
    // no-op in those states anyway, but pre-filtering keeps the intent clear.
    //
    // A module that flips state mid-tick (kActive→kStopping via BeginStop
    // from another module's DoUpdate) will be picked up by the reverse pass
    // on the same Update call — it gets one DoStop tick rather than waiting
    // a whole frame.
    //--------------------------------------------------------------------------
    void ProcessingUnit::RunForwardPass(float deltaTime)
    {
        // Perf Inspector A2b: always on (not gated on profile categories). One steady_clock
        // read between modules (N+1 reads per pass); results are staged in stack arrays and
        // recorded after the loop so the table's own cost can be bracketed. No allocation.
        using Clock = std::chrono::steady_clock;
        float        ms[kMaxModules];
        bool         ran[kMaxModules];

        Clock::time_point prev = Clock::now();
        const Clock::time_point passStart = prev;
        bool moduleStarting = false;   // any module of this unit in kStarting during the pass (A3: skipped entry)
        for (unsigned int i = 0; i < mModuleCount; ++i)
        {
            ran[i] = false;
            const ModuleState s = mModules[i].module->GetState();
            if (s == ModuleState::kStarting)
                moduleStarting = true;
            if (s == ModuleState::kStarting || s == ModuleState::kActive)
            {
                ModuleEntry& entry = mModules[i];
                entry.module->FrameTick(deltaTime, entry.startTimeoutMs, entry.stopTimeoutMs);
                const Clock::time_point now = Clock::now();
                ms[i]  = std::chrono::duration<float, std::milli>(now - prev).count();
                ran[i] = true;
                prev   = now;
            }
        }

        // Table writes (bracketed for perf.self_ms; one extra clock read at the end).
        for (unsigned int i = 0; i < mModuleCount; ++i)
        {
            if (!ran[i]) continue;
            mTimingTable.RecordModule(i, mModules[i].module->GetInstanceId().Value(), ms[i]);
            if (mModules[i].metricEwmaMs)
                mModules[i].metricEwmaMs->Set(static_cast<double>(mTimingTable.GetEwma(i)));
        }
        const float unitTickMs = std::chrono::duration<float, std::milli>(prev - passStart).count();
        mTimingTable.RecordUnitTick(unitTickMs);
        mTimingTable.CommitEntry();

        // Perf Inspector A3c: one detector observation per committed entry. The budget is the
        // per-pass target (on Sim a pass is one fixed step, mFrequencyHz is the step rate).
        {
            using namespace Dia::Observation::Profile;
            const float budgetMs = (mFrequencyHz > 0.0f) ? (1000.0f / mFrequencyHz) : 0.0f;
            const double nowMs = std::chrono::duration<double, std::milli>(Clock::now().time_since_epoch()).count();
            const TripResult trip = mTripDetector.Observe(unitTickMs, budgetMs, moduleStarting, nowMs);

            const uint32_t overTotal = mTripDetector.GetOverBudgetFrames();
            if (overTotal != mOverBudgetReported)
            {
                if (mMetricOverBudgetFrames)
                    mMetricOverBudgetFrames->Inc(static_cast<uint64_t>(overTotal - mOverBudgetReported));
                mOverBudgetReported = overTotal;
            }

            if (trip.tripped)
            {
                TripReporter* reporter = mTripReporter.load(std::memory_order_acquire);
                if (reporter != nullptr)
                {
#ifdef _DEBUG
                    const char* buildConfig = "Debug";
#else
                    const char* buildConfig = "Release";
#endif
                    BuildTripRecord(mTripRecord, mInstanceId.AsChar(), trip, budgetMs, buildConfig, mTimingTable);
                    reporter->Report(mTripRecord);
                }
            }
        }
        mSelfMsAccum += std::chrono::duration<float, std::milli>(Clock::now() - prev).count();
    }

    //--------------------------------------------------------------------------
    void ProcessingUnit::RunReversePass(float deltaTime)
    {
        // Reverse pass: stopping modules (dependent-before-dependency).
        for (int i = static_cast<int>(mModuleCount) - 1; i >= 0; --i)
        {
            const ModuleState s = mModules[i].module->GetState();
            if (s == ModuleState::kStopping)
            {
                ModuleEntry& entry = mModules[i];
                entry.module->FrameTick(deltaTime, entry.startTimeoutMs, entry.stopTimeoutMs);
                if (entry.metricEwmaMs && mModules[i].module->GetState() != ModuleState::kStopping)
                    entry.metricEwmaMs->Set(0.0);   // module finished stopping: zero, leave registered
            }
        }
    }

    //--------------------------------------------------------------------------
    void ProcessingUnit::Update(float deltaTime)
    {
        DIA_PROFILE_SCOPE_SUBJECT("pu.update", Dia::Observation::Profile::Category::kDiaApplicationFlow, mInstanceId.Value());
        DIA_TRACE_ZONE("pu.update", Dia::Observation::Trace::Category::kDiaApplicationFlow);

        // Task 34 — measure tick duration
        const auto tickStart = std::chrono::steady_clock::now();
        mSelfMsAccum = 0.0f;
        mLastUpdateStepCount = 1;

        switch (mAffinity)
        {
            case PUAffinity::kSim:
            {
                // Fixed-timestep accumulator (ST-011): banks real elapsed time, drains it in
                // whole 1/GetFrequencyHz() steps, running the full forward pass once per step
                // (0, 1, or several times per real call) so every SimModule::DoUpdate always
                // sees a constant-size gameDt, never a raw variable frame delta. Capped at
                // mMaxCatchUpTicksPerFrame to avoid a spiral of death; excess backlog is
                // dropped (not deferred) and logged/counted.
                if (mWorldDomain.IsPaused())
                {
                    // Don't bank time while paused — resuming after a long pause must not
                    // trigger a catch-up burst.
                    mSimAccumulatorSec = 0.0f;
                }
                else
                {
                    mSimAccumulatorSec += deltaTime;
                }

                const float fixedStepSec = (mFrequencyHz > 0.0f) ? (1.0f / mFrequencyHz) : 0.0f;
                unsigned int ticksThisFrame = 0;
                if (fixedStepSec > 0.0f)
                {
                    while (mSimAccumulatorSec >= fixedStepSec && ticksThisFrame < mMaxCatchUpTicksPerFrame)
                    {
                        mWorldDomain.Tick();   // advances by fixedStep * current scale
                        mSimTimeContext = Dia::SimTime::SimTimeContext{
                            mWorldDomain.Now(), mWorldDomain.Step(), mWorldDomain.GetTick(),
                            mWorldDomain.GetScale(), mWorldDomain.IsPaused() };
                        RunForwardPass(fixedStepSec);   // fixedStepSec, not the real deltaTime —
                                                        // FrameTick's timeout bookkeeping should see
                                                        // the sum of fixed steps actually consumed
                        mSimAccumulatorSec -= fixedStepSec;
                        ++ticksThisFrame;
                    }
                    if (mSimAccumulatorSec >= fixedStepSec)
                    {
                        // Still behind after the cap: drop the backlog, don't defer it.
                        DIA_LOG_WARNING("pu", "pu.sim_catchup_dropped id=%s backlog_sec=%.3f",
                            mInstanceId.AsChar(), static_cast<double>(mSimAccumulatorSec));
                        if (mMetricDroppedTicks)
                            mMetricDroppedTicks->Inc();
                        mSimAccumulatorSec = 0.0f;
                    }
                }
                mLastUpdateStepCount = ticksThisFrame;
                if (ticksThisFrame > 1 && mMetricCatchupUpdates)
                    mMetricCatchupUpdates->Inc();
                break;   // forward pass already ran above (0..N times) — do not run it again below
            }
            case PUAffinity::kRender:
                // mRenderTimeContext is NOT computed here — DiaRenderTime (a later task) pushes
                // it via SetRenderTimeContext() during its own DoUpdate. Sibling RenderModules
                // read whatever was pushed last tick — one-tick-stale by design.
                RunForwardPass(deltaTime);
                break;
            case PUAffinity::kMain:
            default:
                // default here also covers PUAffinity::kAny (any custom-named PU, e.g. an
                // editor's tool PU) — those modules use MainModule too, so they need
                // mMainTimeContext populated the same way kMain does.
                mMainTimeContext = Dia::SimTime::MainTimeContext{ deltaTime };
                RunForwardPass(deltaTime);
                break;
        }

        // Always exactly once per real Update() call, regardless of how many fixed steps
        // drained above — shutdown sequencing must not stall just because 0 steps drained.
        RunReversePass(deltaTime);

        if (mPostTickFn)
            mPostTickFn();

        // Task 34 — record elapsed tick time and update gauge
        const auto tickEnd = std::chrono::steady_clock::now();
        mLastTickMs = std::chrono::duration<float, std::milli>(tickEnd - tickStart).count();
        if (mMetricLastTickMs)
            mMetricLastTickMs->Set(static_cast<double>(mLastTickMs));

        if (mMetricSelfMs)
            mMetricSelfMs->Set(static_cast<double>(mSelfMsAccum));

        ++mTickCount;
        // Over-budget detection moved to RunForwardPass (A3c trip detector + pu.<unit>.over_budget_frames).
    }

    //--------------------------------------------------------------------------
    void ProcessingUnit::SetPostTickFn(PostTickFn fn)
    {
        mPostTickFn = std::move(fn);
    }

    //--------------------------------------------------------------------------
    bool ProcessingUnit::AllModulesSettled() const
    {
        for (unsigned int i = 0; i < mModuleCount; ++i)
        {
            const ModuleState s = mModules[i].module->GetState();
            if (s != ModuleState::kInactive && s != ModuleState::kFailed)
                return false;
        }
        return true;
    }

    //--------------------------------------------------------------------------
    void ProcessingUnit::operator()()
    {
        Dia::Observation::Log::Logger::Instance().RegisterThreadBuffer();

        DIA_LOG_INFO("Application", "ProcessingUnit '%s' thread starting (%.0fHz)",
                     mInstanceId.AsChar(), static_cast<double>(mFrequencyHz));

        float targetIntervalMs = (mFrequencyHz > 0.0f) ? (1000.0f / mFrequencyHz) : 0.0f;
        auto lastTime = std::chrono::high_resolution_clock::now();

        // Keep ticking until either:
        //   (a) no stop has been requested (normal run), OR
        //   (b) stop requested AND every module has reached a settled state
        //       (kInactive/kFailed). The settle-after-stop loop is essential:
        //       Application::RequestShutdown calls BeginStop on modules from
        //       MainPU, which flips them to kStopping. Only this thread can
        //       call FrameTick on them to reach kInactive — exiting early
        //       would strand kStopping modules forever and Application::Update
        //       would spin in AllModulesInactive() on MainPU.
        while (!mStopRequested.load() || !AllModulesSettled())
        {
            auto now = std::chrono::high_resolution_clock::now();
            float dt = std::chrono::duration<float>(now - lastTime).count();
            lastTime = now;

            Update(dt);

            if (targetIntervalMs > 0.0f)
            {
                auto elapsed = std::chrono::duration<float, std::milli>(
                    std::chrono::high_resolution_clock::now() - lastTime).count();
                float sleepMs = targetIntervalMs - elapsed;
                if (sleepMs > 0.0f)
                    std::this_thread::sleep_for(
                        std::chrono::duration<float, std::milli>(sleepMs));
            }
        }

        DIA_LOG_INFO("Application", "ProcessingUnit '%s' thread exiting",
                     mInstanceId.AsChar());

        Dia::Observation::Log::Logger::Instance().UnregisterThreadBuffer();
    }

    //--------------------------------------------------------------------------
    void ProcessingUnit::RequestStop()
    {
        mStopRequested.store(true);
    }

    //--------------------------------------------------------------------------
    bool ProcessingUnit::IsStopRequested() const
    {
        return mStopRequested.load();
    }

}} // namespace Dia::ApplicationFlow
