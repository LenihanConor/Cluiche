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
    void ProcessingUnit::Update(float deltaTime)
    {
        for (unsigned int i = 0; i < mModuleCount; ++i)
        {
            ModuleEntry& entry = mModules[i];
            entry.module->FrameTick(deltaTime, entry.startTimeoutMs, entry.stopTimeoutMs);
        }
    }

    //--------------------------------------------------------------------------
    void ProcessingUnit::operator()()
    {
        DIA_LOG_INFO("Application", "ProcessingUnit '%s' thread starting (%.0fHz)",
                     mInstanceId.AsChar(), static_cast<double>(mFrequencyHz));

        float targetIntervalMs = (mFrequencyHz > 0.0f) ? (1000.0f / mFrequencyHz) : 0.0f;
        auto lastTime = std::chrono::high_resolution_clock::now();

        while (!mStopRequested.load())
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
