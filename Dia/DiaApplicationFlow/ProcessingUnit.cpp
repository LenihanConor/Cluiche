////////////////////////////////////////////////////////////////////////////////
// Filename: ProcessingUnit.cpp
// DiaApplicationFlow — v2 ProcessingUnit
////////////////////////////////////////////////////////////////////////////////
#include "DiaApplicationFlow/ProcessingUnit.h"
#include "DiaApplicationFlow/TypeRegistry.h"

#include <DiaCore/Core/Assert.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Log/Logger.h>
#include <DiaObservation/Metric/Gauge.h>
#include <DiaObservation/Metric/MetricRegistry.h>
#include <DiaObservation/Profile/DiaProfile.h>
#include <DiaObservation/Trace/DiaTrace.h>

#include <chrono>
#include <string>

namespace Dia { namespace ApplicationFlow {

    //--------------------------------------------------------------------------
    ProcessingUnit::ProcessingUnit(const Dia::Core::StringCRC& instanceId,
                                   float frequencyHz,
                                   bool dedicatedThread)
        : mInstanceId(instanceId)
        , mAffinity(PUAffinity::kAny)
        , mFrequencyHz(frequencyHz)
        , mDedicatedThread(dedicatedThread)
        , mModuleCount(0)
    {
        // Map well-known PU instance IDs to their affinity enum.
        if      (instanceId == Dia::Core::StringCRC("MainPU"))   mAffinity = PUAffinity::kMain;
        else if (instanceId == Dia::Core::StringCRC("SimPU"))    mAffinity = PUAffinity::kSim;
        else if (instanceId == Dia::Core::StringCRC("RenderPU")) mAffinity = PUAffinity::kRender;
        else                                                      mAffinity = PUAffinity::kAny;

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

#ifdef DIA_DEBUG
        {
            PUAffinity moduleAffinity = TypeRegistry::Global().GetAllowedPUs(module->GetTypeId());
            if (moduleAffinity != PUAffinity::kAny &&
                !HasAffinity(moduleAffinity, mAffinity) &&
                mAffinity != PUAffinity::kAny)
            {
                DIA_ASSERT(false,
                    "Module '%s' (type '%s') not allowed on PU '%s' — kAllowedPUs mismatch",
                    module->GetInstanceId().AsChar(),
                    module->GetTypeId().AsChar(),
                    GetInstanceId().AsChar());
                DIA_LOG_ERROR("ApplicationFlow",
                    "Module '%s' placed on wrong PU '%s' (type '%s')",
                    module->GetInstanceId().AsChar(),
                    GetInstanceId().AsChar(),
                    module->GetTypeId().AsChar());
            }
        }
#endif

        ModuleEntry& entry = mModules[mModuleCount];
        entry.module          = std::move(module);
        entry.startTimeoutMs  = startTimeoutMs;
        entry.stopTimeoutMs   = stopTimeoutMs;

        entry.module->SetProcessingUnit(this);

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
    void ProcessingUnit::Update(float deltaTime)
    {
        DIA_PROFILE_SCOPE("pu.update", Dia::Observation::Profile::Category::kDiaApplicationFlow);
        DIA_TRACE_ZONE("pu.update", Dia::Observation::Trace::Category::kDiaApplicationFlow);

        // Task 34 — measure tick duration
        const auto tickStart = std::chrono::high_resolution_clock::now();

        // Forward pass: starting/active modules.
        for (unsigned int i = 0; i < mModuleCount; ++i)
        {
            const ModuleState s = mModules[i].module->GetState();
            if (s == ModuleState::kStarting || s == ModuleState::kActive)
            {
                ModuleEntry& entry = mModules[i];
                entry.module->FrameTick(deltaTime, entry.startTimeoutMs, entry.stopTimeoutMs);
            }
        }

        // Reverse pass: stopping modules (dependent-before-dependency).
        for (int i = static_cast<int>(mModuleCount) - 1; i >= 0; --i)
        {
            const ModuleState s = mModules[i].module->GetState();
            if (s == ModuleState::kStopping)
            {
                ModuleEntry& entry = mModules[i];
                entry.module->FrameTick(deltaTime, entry.startTimeoutMs, entry.stopTimeoutMs);
            }
        }

        if (mPostTickFn)
            mPostTickFn();

        // Task 34 — record elapsed tick time and update gauge
        const auto tickEnd = std::chrono::high_resolution_clock::now();
        mLastTickMs = std::chrono::duration<float, std::milli>(tickEnd - tickStart).count();
        if (mMetricLastTickMs)
            mMetricLastTickMs->Set(static_cast<double>(mLastTickMs));

        // Task 35 — warn when tick exceeds target period
        const float targetMs = (mFrequencyHz > 0.0f) ? (1000.0f / mFrequencyHz) : 0.0f;
        if (targetMs > 0.0f && mLastTickMs > targetMs)
        {
            DIA_LOG_WARNING("pu", "pu.over_budget id=%s tick_ms=%.1f target_ms=%.1f",
                mInstanceId.AsChar(), mLastTickMs, targetMs);
        }
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
