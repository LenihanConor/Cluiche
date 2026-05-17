////////////////////////////////////////////////////////////////////////////////
// Filename: ProcessingUnit.cpp
// DiaApplicationFlow — v2 ProcessingUnit
////////////////////////////////////////////////////////////////////////////////
#include "DiaApplicationFlow/ProcessingUnit.h"

#include <DiaCore/Core/Assert.h>
#include <DiaLogger/DiaLog.h>

#include <chrono>

namespace Dia { namespace ApplicationFlow {

    //--------------------------------------------------------------------------
    ProcessingUnit::ProcessingUnit(const Dia::Core::StringCRC& instanceId,
                                   float frequencyHz,
                                   bool dedicatedThread)
        : mInstanceId(instanceId)
        , mFrequencyHz(frequencyHz)
        , mDedicatedThread(dedicatedThread)
        , mModuleCount(0)
    {
        DIA_LOG_INFO("Application", "ProcessingUnit '%s' created (%.0fHz, dedicated=%d)",
                     mInstanceId.AsChar(), static_cast<double>(mFrequencyHz),
                     mDedicatedThread ? 1 : 0);
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
