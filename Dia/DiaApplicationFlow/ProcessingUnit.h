#pragma once
#include <DiaApplicationFlow/Module.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Memory/UniquePtr.h>
#include <atomic>
#include <thread>

namespace Dia { namespace ApplicationFlow {

    class Application;

    class ProcessingUnit {
    public:
        ProcessingUnit(const Dia::Core::StringCRC& instanceId,
                       float frequencyHz,
                       bool dedicatedThread);
        ~ProcessingUnit();

        ProcessingUnit(const ProcessingUnit&) = delete;
        ProcessingUnit& operator=(const ProcessingUnit&) = delete;

        [[nodiscard]] const Dia::Core::StringCRC& GetInstanceId() const;
        [[nodiscard]] float GetFrequencyHz() const;
        [[nodiscard]] bool IsDedicatedThread() const;

        // Module management (called by Application during Start)
        void AddModule(Dia::Core::UniquePtr<Module> module,
                       float startTimeoutMs,
                       float stopTimeoutMs);

        Module* FindModule(const Dia::Core::StringCRC& instanceId);
        const Module* FindModule(const Dia::Core::StringCRC& instanceId) const;

        // Application back-pointer (set by Application after construction)
        void SetApplication(Application* app);
        Application* GetApplication() const;

        // Update — called by dedicated thread loop, or by Application::Update() for main PU
        void Update(float deltaTime);

        // Thread entry point (dedicated thread only)
        void operator()();

        // Request stop of dedicated thread
        void RequestStop();
        [[nodiscard]] bool IsStopRequested() const;

    private:
        static constexpr unsigned int kMaxModules = 32;

        struct ModuleEntry {
            Dia::Core::UniquePtr<Module> module;
            float startTimeoutMs = 0.0f;
            float stopTimeoutMs  = 0.0f;

            ModuleEntry() = default;
            ModuleEntry(ModuleEntry&&) = default;
            ModuleEntry& operator=(ModuleEntry&&) = default;

            ModuleEntry(const ModuleEntry&) = delete;
            ModuleEntry& operator=(const ModuleEntry&) = delete;
        };

        Dia::Core::StringCRC mInstanceId;
        float mFrequencyHz;
        bool mDedicatedThread;

        // Fixed capacity: kMaxModules per PU (matches spec)
        ModuleEntry     mModules[kMaxModules];
        unsigned int    mModuleCount = 0;

        Application* mApplication = nullptr;
        std::atomic<bool> mStopRequested{false};
    };

}} // namespace Dia::ApplicationFlow
