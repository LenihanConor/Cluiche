#pragma once
#include <DiaApplicationFlow/Module.h>
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Memory/UniquePtr.h>
#include <DiaCore/SimTime/SimTimeContext.h>
#include <DiaCore/SimTime/SimTimeDomain.h>
#include <DiaCore/SimTime/SimTimeDomainRegistry.h>
#include <DiaObservation/Profile/PerfSummary.h>
#include <DiaObservation/Profile/TripDetector.h>
#include <DiaObservation/Profile/TripRecord.h>
#include <DiaObservation/Profile/UnitTimingTable.h>
#include <DiaObservation/Profile/CoarseChunkBuilder.h>
#include <DiaObservation/Profile/JobChunkBuilder.h>
#include <DiaObservation/Profile/JobTimingTable.h>
#include <DiaObservation/Job/JobInbox.h>
#include <atomic>
#include <thread>
#ifdef _DEBUG
#include <DiaApplicationFlow/PerfPause.h>
#endif

namespace Dia { namespace Observation { namespace Metric { class Gauge; class Counter; } } }
namespace Dia { namespace Observation { namespace Profile { class TripReporter; } } }
namespace Dia { namespace ApplicationFlow { template<typename T> class EventStreamStore; } }
namespace Dia { namespace Observation { namespace Profile { struct CoarseChunk; struct JobChunk; } } }

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

        // Perf Inspector A2b � per-forward-pass timing (one entry per RunForwardPass;
        // on Sim, one per fixed step). Slot = index in this PU's module array.
        [[nodiscard]] const Dia::Observation::Profile::UnitTimingTable& GetUnitTimingTable() const { return mTimingTable; }
        // Fixed steps the most recent Update() ran (Sim; 1 for other affinities).
        [[nodiscard]] unsigned int GetLastUpdateStepCount() const { return mLastUpdateStepCount; }

        // Post-tick hook: called by Application once during Start() to register
        // a callback that flushes EventStreamStores owned by this PU.
        // The callback is invoked at the end of every Update() call.
        using PostTickFn = std::function<void()>;
        void SetPostTickFn(PostTickFn fn);

        // Perf Inspector A3c - over-budget trip reporting. The reporter is injected (plain pointer,
        // not owned; null = counter only, no log/no record). Thread-safe to set while the unit runs.
        void SetTripReporter(Dia::Observation::Profile::TripReporter* reporter) { mTripReporter.store(reporter, std::memory_order_release); }
        // Apply trip tuning (task 8 config block). Call before the unit starts ticking.
        void SetTripConfig(const Dia::Observation::Profile::TripConfig& config) { mTripDetector.SetConfig(config); }
        [[nodiscard]] const Dia::Observation::Profile::TripDetector& GetTripDetector() const { return mTripDetector; }

        // Perf Inspector A4b (AA9): per-module summary rows that outlive the module. A row is banked when a
        // module leaves kStopping; capacity kCapacity rows (overflow is dropped and counted).
        [[nodiscard]] const Dia::Observation::Profile::PerfSummaryAccumulator& GetPerfSummaryAccumulator() const { return mPerfAccumulator; }
        // Perf Inspector task 1b: framework-owned "$perfcoarse" sink. Each forward pass is added to
        // a chunk; a chunk is sent at 64 entries or every 100 ms. Null sink = no sends. Set before Start.
        void SetPerfCoarseSink(EventStreamStore<Dia::Observation::Profile::CoarseChunk>* sink) { mCoarseSink = sink; }
        // Perf Inspector task 13: framework-owned "$perfjobs" sink for JobChunk (jobs and workers). Null = no sends.
        void SetPerfJobsSink(EventStreamStore<Dia::Observation::Profile::JobChunk>* sink) { mJobSink = sink; }
        // Per-PU job timing (drained from this PU's JobInbox once per Update()).
        [[nodiscard]] const Dia::Observation::Profile::JobTimingTable& GetJobTimingTable() const { return mJobTable; }
        // Kill switch (observation.perf.jobs): false = tier 1 counters only. Set before the unit starts ticking.
        void SetPerJobRecording(bool on) { mJobTable.SetPerJobRecording(on); }
        // The inbox JobObserver delivers this PU's finished job records to (installed on the ticking thread for the duration of each Update()).
        [[nodiscard]] Dia::Observation::Job::JobInbox& GetJobInbox() { return mJobInbox; }

        // Appends this unit's summary to `out`: banked rows (stopped modules) merged with the live
        // modules' current stats. Intended for shutdown; reads the table from the caller's thread.
        void CollectPerfSummary(Dia::Observation::Profile::PerfSummary& out) const;

#ifdef _DEBUG
        // Perf live control (task 3, Debug only): the framework-owned "$perfpause" stream. Read
        // once per Update(); null = no pause support (a bare PU built outside Application).
        void SetPerfPauseSource(const FrameStreamStore<PerfPauseSet>* source) { mPauseSource = source; }
#endif

    private:
        static constexpr unsigned int kMaxModules = 64;

        // Two-pass tick bodies extracted from Update() (see Update() for the
        // forward/reverse dependency-order rationale). RunForwardPass ticks
        // kStarting/kActive modules; RunReversePass ticks kStopping modules.
        void RunForwardPass(float deltaTime);
        void RunReversePass(float deltaTime);
        void BuildModuleRow(unsigned int index, Dia::Observation::Profile::PerfModuleRow& out) const;

        struct ModuleEntry {
            Dia::Core::UniquePtr<Module> module;
            float startTimeoutMs = 0.0f;
            float stopTimeoutMs  = 0.0f;
            Dia::Observation::Metric::Gauge* metricEwmaMs = nullptr;   // pu.<unit>.module.<id>.ms (registered once in AddModule)

            ModuleEntry() = default;
            ModuleEntry(ModuleEntry&&) = default;
            ModuleEntry& operator=(ModuleEntry&&) = default;

            ModuleEntry(const ModuleEntry&) = delete;
            ModuleEntry& operator=(const ModuleEntry&) = delete;
        };

#ifdef _DEBUG
        // Pause table: mPaused[i] is true when mModules[i] is in the latest pause snapshot. Rebuilt
        // only when the snapshot generation changes; RunForwardPass reads one bool per module.
        // Lives here (not in Module) so Module layout never depends on _DEBUG.
        void RefreshPauseTable();
        const FrameStreamStore<PerfPauseSet>* mPauseSource = nullptr;
        uint32_t                              mPauseGeneration = 0;
        bool                                  mPaused[kMaxModules] = {};
#endif

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

        // Perf Inspector A2b
        Dia::Observation::Profile::UnitTimingTable mTimingTable;
        Dia::Observation::Profile::CoarseChunkBuilder mCoarseChunk;   // A1b: chunk builder
        EventStreamStore<Dia::Observation::Profile::CoarseChunk>* mCoarseSink = nullptr;  // A1b: chunk sink
        // Perf Inspector task 13: jobs and workers
        Dia::Observation::Job::JobInbox                 mJobInbox{1024};
        Dia::Observation::Profile::JobTimingTable       mJobTable;
        Dia::Observation::Profile::JobChunkBuilder      mJobChunk;
        EventStreamStore<Dia::Observation::Profile::JobChunk>* mJobSink = nullptr;
        Dia::Observation::Profile::PerfSummaryAccumulator mPerfAccumulator;   // A4b: rows of stopped modules
        unsigned int                      mLastUpdateStepCount = 0;
        float                             mSelfMsAccum = 0.0f;   // table cost accumulated over this Update()'s passes
        Dia::Observation::Metric::Gauge*  mMetricSelfMs = nullptr;
        Dia::Observation::Metric::Counter* mMetricCatchupUpdates = nullptr;

        // Perf Inspector A3c - trip detection (one Observe() per committed table entry).
        Dia::Observation::Profile::TripDetector          mTripDetector;
        std::atomic<Dia::Observation::Profile::TripReporter*> mTripReporter{nullptr};
        Dia::Observation::Profile::TripRecord            mTripRecord;          // member buffer (~4 KB), PU thread only
        uint32_t                                         mOverBudgetReported = 0;
        Dia::Observation::Metric::Counter*               mMetricOverBudgetFrames = nullptr;

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
