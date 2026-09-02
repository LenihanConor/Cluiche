#pragma once
#include <DiaApplicationFlow/Module.h>
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Memory/UniquePtr.h>
#include <DiaCore/SimTime/SimTimeContext.h>
#include <DiaCore/SimTime/SimTimeDomain.h>
#include <DiaCore/SimTime/SimTimeDomainRegistry.h>
#include <atomic>
#include <thread>

namespace Dia { namespace Observation { namespace Metric { class Gauge; class Counter; } } }

namespace Dia { namespace ApplicationFlow {

    // A ProcessingUnit is a scheduler: it owns its Modules and drives their
    // FrameTick in array order at a fixed rate.  It deliberately does NOT
    // hold a back-pointer to Application — modules that need to reach the
    // Application use Module::GetApplication() (narrow IApplicationControl).
    class ProcessingUnit {
    public:
        ProcessingUnit(const Dia::Core::StringCRC& instanceId,
                       float frequencyHz,
                       bool dedicatedThread,
                       unsigned int maxCatchUpTicksPerFrame = 5);
        ~ProcessingUnit();

        ProcessingUnit(const ProcessingUnit&) = delete;
        ProcessingUnit& operator=(const ProcessingUnit&) = delete;

        [[nodiscard]] const Dia::Core::StringCRC& GetInstanceId() const;
        [[nodiscard]] float GetFrequencyHz() const;
        [[nodiscard]] bool IsDedicatedThread() const;
        [[nodiscard]] PUAffinity GetAffinity() const { return mAffinity; }

        // Time contexts — cached each Update() and read by the typed module bases
        // (SimModule / RenderModule / MainModule) when they forward DoUpdate.
        [[nodiscard]] const Dia::SimTime::SimTimeContext&    GetSimTimeContext()    const { return mSimTimeContext; }
        [[nodiscard]] const Dia::SimTime::RenderTimeContext& GetRenderTimeContext() const { return mRenderTimeContext; }
        [[nodiscard]] const Dia::SimTime::MainTimeContext&   GetMainTimeContext()   const { return mMainTimeContext; }

        // Framework-internal — called by DiaRenderTime (a later task) after it reads FetchLatest()
        // from the sim-time FrameStream. Not part of the public Module API; sibling RenderModules
        // only ever read via GetRenderTimeContext() above.
        void SetRenderTimeContext(const Dia::SimTime::RenderTimeContext& ctx) { mRenderTimeContext = ctx; }

        // The SimPU owns the world clock directly. A later task (SimTimeDomainRegistry) wraps this
        // same instance rather than owning a separate copy — do not construct a second world domain
        // anywhere else.
        [[nodiscard]] Dia::SimTime::SimTimeDomain& GetWorldDomain() { return mWorldDomain; }

        // Task 2.2 — named clock-tree registry bound to mWorldDomain (never copies it).
        // Owns any sub-domains created via Create(); Find(kWorldId) returns the exact
        // same instance as GetWorldDomain() above.
        [[nodiscard]] Dia::SimTime::SimTimeDomainRegistry& GetDomainRegistry() { return mDomainRegistry; }

        // Module management (called by Application during Start)
        void AddModule(Dia::Core::UniquePtr<Module> module,
                       float startTimeoutMs,
                       float stopTimeoutMs);

        Module* FindModule(const Dia::Core::StringCRC& instanceId);
        const Module* FindModule(const Dia::Core::StringCRC& instanceId) const;

        // Update — called by dedicated thread loop, or by Application::Update() for main PU
        void Update(float deltaTime);

        // Thread entry point (dedicated thread only)
        void operator()();

        // Request stop of dedicated thread. The thread will keep ticking
        // until every module has reached kInactive/kFailed, so that modules
        // flipped to kStopping by Application::RequestShutdown can complete
        // their DoStop on this PU's thread.
        void RequestStop();
        [[nodiscard]] bool IsStopRequested() const;

        // True iff every module owned by this PU is kInactive or kFailed.
        [[nodiscard]] bool AllModulesSettled() const;

        // Post-tick hook: called by Application once during Start() to register
        // a callback that flushes EventStreamStores owned by this PU.
        // The callback is invoked at the end of every Update() call.
        using PostTickFn = std::function<void()>;
        void SetPostTickFn(PostTickFn fn);

    private:
        static constexpr unsigned int kMaxModules = 64;

        // Two-pass tick bodies extracted from Update() (see Update() for the
        // forward/reverse dependency-order rationale). RunForwardPass ticks
        // kStarting/kActive modules; RunReversePass ticks kStopping modules.
        void RunForwardPass(float deltaTime);
        void RunReversePass(float deltaTime);

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
        PUAffinity           mAffinity;
        float mFrequencyHz;
        bool mDedicatedThread;

        // Fixed capacity: kMaxModules per PU (matches spec)
        ModuleEntry     mModules[kMaxModules];
        unsigned int    mModuleCount = 0;

        std::atomic<bool> mStopRequested{false};
        PostTickFn        mPostTickFn;

        // Task 34 — per-tick timing gauge
        float mLastTickMs = 0.0f;
        Dia::Observation::Metric::Gauge* mMetricLastTickMs = nullptr;
        unsigned int mTickCount = 0;

        // --- DiaSimTime (Task 1.4) -------------------------------------------
        // The SimPU owns the world clock directly. Constructed in the ctor
        // init-list (needs frequencyHz).
        Dia::SimTime::SimTimeDomain     mWorldDomain;

        // Task 2.2 — registry binds to mWorldDomain by reference; must be declared
        // AFTER mWorldDomain (member init order follows declaration order).
        Dia::SimTime::SimTimeDomainRegistry mDomainRegistry;

        float                           mSimAccumulatorSec = 0.0f;
        unsigned int                    mMaxCatchUpTicksPerFrame;   // set in ctor init-list from the new param
        Dia::Observation::Metric::Counter* mMetricDroppedTicks = nullptr;

        // TimeAbsolute/TimeRelative expose only a private default constructor
        // (factory-only via Zero()/CreateFrom*()), so SimTimeContext/RenderTimeContext
        // have no usable default constructor — supply Zero() explicitly here (MSVC C2512).
        Dia::SimTime::SimTimeContext    mSimTimeContext{ Dia::Core::TimeAbsolute::Zero(), Dia::Core::TimeRelative::Zero(), 0, 1.0f, false };
        Dia::SimTime::RenderTimeContext mRenderTimeContext{ 0.0f, Dia::Core::TimeAbsolute::Zero(), 0 };
        Dia::SimTime::MainTimeContext   mMainTimeContext{ 0.0f };
    };

}} // namespace Dia::ApplicationFlow
