////////////////////////////////////////////////////////////////////////////////
// Filename: Application.cpp
// DiaApplicationFlow — v2 Application
////////////////////////////////////////////////////////////////////////////////
#include "DiaApplicationFlow/Application.h"
#include "DiaApplicationFlow/Module.h"
#include "DiaApplicationFlow/Manifest/ManifestValidatorV2.h"
#include "DiaApplicationFlow/IApplicationInspectable.h"
#include "DiaApplicationFlow/LifecycleEvent.h"
#include <DiaStreams/Event.h>
#include <DiaStreams/IStreamStore.h>
#include <DiaCore/Time/TimeAbsolute.h>
#include <DiaCore/Core/Assert.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Profile/DiaProfile.h>
#include <DiaObservation/Profile/Profiler.h>
#include <DiaObservation/Trace/DiaTrace.h>
#include <DiaObservation/Perf/PerfMarkers.h>
#include <DiaObservation/Metric/MetricRegistry.h>
#include <DiaObservation/Metric/Counter.h>
#include <DiaObservation/Metric/Gauge.h>
#include <DiaAPI/CommandRegistry/CommandRegistry.h>
#include <DiaCore/Json/external/json/json.h>
#include <cstring>

#ifdef _WIN32
// Declared here (not via windows.h) to keep that header out of this file; identical to the SDK declarations.
extern "C" __declspec(dllimport) unsigned long __stdcall GetCurrentThreadId(void);
extern "C" __declspec(dllimport) unsigned long __stdcall GetThreadId(void* thread);
#endif

namespace Dia { namespace ApplicationFlow {

    //--------------------------------------------------------------------------
    // Constructor / Destructor
    //--------------------------------------------------------------------------

    Application::Application(const ApplicationManifestV3& manifest,
                             TypeRegistry& registry)
        : mManifest(manifest)
        , mRegistry(registry)
        , mProcessingUnitCount(0)
        , mDedicatedThreadCount(0)
        , mCurrentStage()
        , mPendingStage()
        , mHasPendingTransition(false)
        , mShuttingDown(false)
        , mStarted(false)
        , mShutdownInitiated(false)
    {
        // Resolve registry-owned stream metrics per instance (registry Reset() frees them).
        auto& reg = Observation::Metric::MetricRegistry::Instance();
        mServiceStreamCommitCount    = reg.RegisterCounter(Core::StringCRC("dia.stream.service_commit_count"));
        mServiceStreamResetCount     = reg.RegisterCounter(Core::StringCRC("dia.stream.service_reset_count"));
        mServiceStreamCommittedGauge = reg.RegisterGauge(Core::StringCRC("dia.stream.service_committed"));
    }

    //--------------------------------------------------------------------------
    Application::~Application()
    {
        // Stop the span sink first: it runs on the tracer's drain thread and sends to mPerfSpansStore.
        if (mSpanSink)
        {
            Dia::Observation::Trace::Tracer::Instance().UnregisterTraceSink(mSpanSink.get());
            mSpanSink.reset();
        }

        // If threads are still running, stop them now (safety net — callers
        // should call RequestShutdown() and drain Update() before destroying).
        if (mStarted && !mShuttingDown.load())
        {
            RequestShutdown();
        }

        // Destructor is the last chance to join — blocking here is the
        // expected behaviour (Update() wasn't drained).
        JoinDedicatedThreads();

        if (mRegisteredMainTraceBuffer)
            Dia::Observation::Trace::Tracer::Instance().UnregisterThreadSpanBuffer();
        if (mRegisteredMainProfileBuffer)
            Dia::Observation::Profile::Profiler::Instance().UnregisterThreadScopeBuffer();
    }

    //--------------------------------------------------------------------------
    // JoinDedicatedThreads  (private)
    //
    // Idempotent — called from Update()'s shutdown tail and from the
    // destructor as a safety net.  After calling, mDedicatedThreadCount is 0.
    //--------------------------------------------------------------------------

    void Application::JoinDedicatedThreads()
    {
        for (unsigned int i = 0; i < mDedicatedThreadCount; ++i)
        {
            if (mDedicatedThreads[i].joinable())
            {
                mDedicatedThreads[i].join();
            }
        }
        mDedicatedThreadCount = 0;
    }

    //--------------------------------------------------------------------------
    // Start
    //--------------------------------------------------------------------------

    bool Application::Start()
    {
        DIA_ASSERT(!mStarted, "Application::Start() called more than once");

        // --- 1. Validate manifest --------------------------------------------
        ManifestValidatorV2 validator(mRegistry);
        validator.Validate(mManifest);

        if (validator.HasErrors())
        {
            const auto& results = validator.GetResults();
            for (unsigned int i = 0; i < results.Size(); ++i)
            {
                if (results[i].severity == ValidationSeverity::kError)
                {
                    DIA_LOG_ERROR("Application", "Manifest validation error [%s]: %s",
                                  results[i].code.AsChar(),
                                  results[i].message.AsCStr());
                }
            }
            DIA_LOG_ERROR("Application", "Application::Start() aborted — manifest has errors");
            return false;
        }

        if (validator.HasWarnings())
        {
            const auto& results = validator.GetResults();
            for (unsigned int i = 0; i < results.Size(); ++i)
            {
                if (results[i].severity == ValidationSeverity::kWarning)
                {
                    DIA_LOG_WARNING("Application", "Manifest validation warning [%s]: %s",
                                    results[i].code.AsChar(),
                                    results[i].message.AsCStr());
                }
            }
        }

        // --- 1b. Create $lifecycle stream (framework-owned, bypasses manifest gating) ---
        {
            static const Dia::Core::StringCRC kLifecycleId("$lifecycle");
            auto* lifecycleStore = new EventStreamStore<LifecycleEvent>(
                kLifecycleId,
                Dia::Core::StringCRC("LifecycleEvent"),
                /*capacity=*/1024,
                /*maxReaders=*/8);

            DIA_ASSERT(mStreamStoreCount < kMaxStreams, "Application: stream capacity exceeded");
            mLifecycleStore = lifecycleStore;
            mStreamStores[mStreamStoreCount] = Dia::Core::UniquePtr<IStreamStore>(lifecycleStore);
            ++mStreamStoreCount;
        }

        // --- 1b2. Create $perfcoarse stream (framework-owned, all builds; Perf Inspector task 1b) ---
        // Every PU sends CoarseChunk timing chunks (~8.5 KB, ~10 Hz per PU); a Main-side reader
        // rebuilds the long coarse ring. Drop-oldest: a slow or absent reader never blocks a PU.
        {
            auto* coarseStore = new EventStreamStore<Dia::Observation::Profile::CoarseChunk>(
                Dia::Core::StringCRC("$perfcoarse"),
                Dia::Core::StringCRC("CoarseChunk"),
                /*capacity=*/64,
                /*maxReaders=*/2,
                OverflowPolicy::kDropOldest);
            DIA_ASSERT(mStreamStoreCount < kMaxStreams, "Application: stream capacity exceeded");
            mPerfCoarseStore = coarseStore;
            mStreamStores[mStreamStoreCount] = Dia::Core::UniquePtr<IStreamStore>(coarseStore);
            ++mStreamStoreCount;
        }

        // --- 1b3. Create $perfjobs stream (framework-owned, all builds; Perf Inspector task 13) ---
        // Every PU sends JobChunk job timing chunks (<= ~8.7 KB, <= 10 Hz, only when jobs ran).
        {
            auto* jobsStore = new EventStreamStore<Dia::Observation::Profile::JobChunk>(
                Dia::Core::StringCRC("$perfjobs"),
                Dia::Core::StringCRC("JobChunk"),
                /*capacity=*/64,
                /*maxReaders=*/2,
                OverflowPolicy::kDropOldest);
            DIA_ASSERT(mStreamStoreCount < kMaxStreams, "Application: stream capacity exceeded");
            mPerfJobsStore = jobsStore;
            mStreamStores[mStreamStoreCount] = Dia::Core::UniquePtr<IStreamStore>(jobsStore);
            ++mStreamStoreCount;
        }

        // --- 1b3b. Create $perfnames stream (framework-owned, all builds) ---
        // crc -> name announcements for ids Main cannot derive from module instance ids (deferred-step channels).
        // Tiny and rare; announcers resend periodically because late-registering readers get no replay.
        {
            auto* namesStore = new EventStreamStore<Dia::Observation::Profile::ChannelNameAnnounce>(
                Dia::Core::StringCRC("$perfnames"),
                Dia::Core::StringCRC("ChannelNameAnnounce"),
                /*capacity=*/64,
                /*maxReaders=*/4,
                OverflowPolicy::kDropOldest);
            DIA_ASSERT(mStreamStoreCount < kMaxStreams, "Application: stream capacity exceeded");
            mPerfNamesStore = namesStore;
            mStreamStores[mStreamStoreCount] = Dia::Core::UniquePtr<IStreamStore>(namesStore);
            ++mStreamStoreCount;
        }

        // --- 1b4. Create $perftrips stream (framework-owned, all builds; Perf Inspector task 3b) ---
        // One TripChunk per emitted over-budget trip (rate limited per unit), so tiny traffic.
        {
            auto* tripsStore = new EventStreamStore<Dia::Observation::Profile::TripChunk>(
                Dia::Core::StringCRC("$perftrips"),
                Dia::Core::StringCRC("TripChunk"),
                /*capacity=*/64,
                /*maxReaders=*/2,
                OverflowPolicy::kDropOldest);
            DIA_ASSERT(mStreamStoreCount < kMaxStreams, "Application: stream capacity exceeded");
            mPerfTripsStore = tripsStore;
            mStreamStores[mStreamStoreCount] = Dia::Core::UniquePtr<IStreamStore>(tripsStore);
            ++mStreamStoreCount;
        }

        // --- 1b5. Create $perfspans stream (framework-owned, all builds; Perf Inspector row 6b) ---
        // Closed trace spans, batched (<= 64 per chunk, flushed when full or 50 ms old) by a sink on the tracer's
        // drain thread. Only produced while tracing is on; drop-oldest so a missing Main reader never blocks it.
        {
            auto* spansStore = new EventStreamStore<Dia::Observation::Trace::SpanChunk>(
                Dia::Core::StringCRC("$perfspans"),
                Dia::Core::StringCRC("SpanChunk"),
                /*capacity=*/32,
                /*maxReaders=*/2,
                OverflowPolicy::kDropOldest);
            DIA_ASSERT(mStreamStoreCount < kMaxStreams, "Application: stream capacity exceeded");
            mPerfSpansStore = spansStore;
            mStreamStores[mStreamStoreCount] = Dia::Core::UniquePtr<IStreamStore>(spansStore);
            ++mStreamStoreCount;

            mSpanSink = std::make_unique<Dia::Observation::Trace::SpanChunkSink>(
                [spansStore](const Dia::Observation::Trace::SpanChunk& chunk)
                {
                    auto ev = std::make_unique<Event<Dia::Observation::Trace::SpanChunk>>();
                    ev->payload = chunk;
                    spansStore->Send(*ev);
                });
            Dia::Observation::Trace::Tracer::Instance().RegisterTraceSink(mSpanSink.get());
        }

#ifdef _DEBUG
        // --- 1c. Create $perfpause stream (Debug only, framework-owned) ---
        {
            auto* pauseStore = new FrameStreamStore<PerfPauseSet>(Dia::Core::StringCRC("$perfpause"));
            DIA_ASSERT(mStreamStoreCount < kMaxStreams, "Application: stream capacity exceeded");
            mPerfPauseStore = pauseStore;
            mStreamStores[mStreamStoreCount] = Dia::Core::UniquePtr<IStreamStore>(pauseStore);
            ++mStreamStoreCount;
            mPerfPause = Dia::Core::UniquePtr<PerfPauseController>(new PerfPauseController(pauseStore));
            mPerfPause->SetModuleResolver(&Application::ResolveModuleType, this);
        }
#endif

        // --- 2. Build PUs and modules from manifest --------------------------
        if (!BuildFromManifest())
        {
            return false;
        }

        for (unsigned int p = 0; p < mProcessingUnitCount; ++p)
            mProcessingUnits[p]->SetPerfCoarseSink(mPerfCoarseStore);
        for (unsigned int p = 0; p < mProcessingUnitCount; ++p)
            mProcessingUnits[p]->SetPerfJobsSink(mPerfJobsStore);
        for (unsigned int p = 0; p < mProcessingUnitCount; ++p)
            mProcessingUnits[p]->SetPerfTripsSink(mPerfTripsStore);

#ifdef _DEBUG
        for (unsigned int p = 0; p < mProcessingUnitCount; ++p)
            mProcessingUnits[p]->SetPerfPauseSource(mPerfPauseStore);
#endif

        // --- 2b. Connect stream handles across all modules -------------------
        ConnectModuleStreams();

        if (mConnectFailed)
        {
            DIA_LOG_ERROR("Application",
                          "Application::Start() — one or more stream connections failed manifest validation");
            return false;
        }

        // --- 2c. Wire post-tick flush callbacks for EventStreamStores ---------
        // For each PU, collect the EventStreamStore IDs whose fromPU matches
        // that PU's instanceId, then register a post-tick lambda that calls
        // Flush() on each of them.  This gives consumers frame-boundary
        // isolation: events sent this tick are only visible on the next tick.
        for (unsigned int p = 0; p < mProcessingUnitCount; ++p)
        {
            const Dia::Core::StringCRC& puId = mManifest.processingUnits[p].instanceId;
            ProcessingUnit* pu = mProcessingUnits[p].Get();

            // Build a fixed-capacity list of EventStreamStore pointers for this PU.
            // We capture raw pointers — stores are owned by mStreamStores[] which
            // outlives the PU; the lambda is destroyed when the PU is destroyed.
            Dia::Core::Containers::DynamicArrayC<IStreamStore*, 64> ownedStores;
            for (unsigned int si = 0; si < mManifest.streams.Size(); ++si)
            {
                const StreamDeclaration& sd = mManifest.streams[si];
                if (sd.kind != Dia::Core::StringCRC("EventStream"))
                    continue;
                if (sd.fromPU != puId)
                    continue;
                IStreamStore* store = FindStreamStore(sd.id);
                if (store)
                {
                    DIA_ASSERT(!ownedStores.IsFull(), "Application: PU owns more EventStreams than the flush list capacity; raise ownedStores size");
                    if (!ownedStores.IsFull())
                        ownedStores.Add(store);
                }
            }

            if (ownedStores.IsEmpty())
                continue;

            // Copy the list into a lambda (fixed-size array avoids heap).
            // DynamicArrayC is copyable, so the capture works directly.
            pu->SetPostTickFn([ownedStores]() {
                for (unsigned int i = 0; i < ownedStores.Size(); ++i)
                    ownedStores[i]->Flush();
            });
        }

        // --- 3. Start dedicated threads (non-main PUs) -----------------------
#ifdef _WIN32
        mUnitThreadIds[0] = static_cast<uint32_t>(GetCurrentThreadId());   // main PU runs inline on this thread
#endif
        {
            // Main PU runs inline on this thread: give it a profiler scope buffer named after the PU.
            auto& profiler = Dia::Observation::Profile::Profiler::Instance();
            if (!profiler.IsThreadScopeBufferRegistered())
            {
                profiler.RegisterNamedThreadScopeBuffer(mProcessingUnits[0]->GetInstanceId().AsChar());
                mRegisteredMainProfileBuffer = true;
            }

            auto& tracer = Dia::Observation::Trace::Tracer::Instance();
            if (!tracer.IsThreadSpanBufferRegistered())
            {
                tracer.RegisterThreadSpanBuffer();
                mRegisteredMainTraceBuffer = true;
            }
        }
        // PU index 0 is the main PU — it runs inline in Update().
        for (unsigned int p = 1; p < mProcessingUnitCount; ++p)
        {
            ProcessingUnit* pu = mProcessingUnits[p].Get();
            if (pu->IsDedicatedThread())
            {
                DIA_ASSERT(mDedicatedThreadCount < kMaxDedicatedThreads,
                           "Application::Start() — dedicated thread capacity exceeded");
                mDedicatedThreads[mDedicatedThreadCount] = std::thread(std::ref(*pu));
#ifdef _WIN32
                mUnitThreadIds[p] = static_cast<uint32_t>(GetThreadId(mDedicatedThreads[mDedicatedThreadCount].native_handle()));
#endif
                ++mDedicatedThreadCount;
            }
        }

        mStarted = true;

        // --- 3b. Register baseline DiaAPI commands --------------------------
        RegisterBaselineCommands();

        // --- 4. Enter initial stage ------------------------------------------
        if (mManifest.initialStage.Value() != 0)
        {
            DIA_LOG_INFO("Application", "Entering initial stage '%s'",
                         mManifest.initialStage.AsChar());

            mCurrentStage = mManifest.initialStage;

            ValidateCostagingRequirements(mManifest.initialStage);

            // Start modules that belong to the initial stage.
            const Dia::Core::StringCRC& stage = mCurrentStage;
            for (unsigned int p = 0; p < mProcessingUnitCount; ++p)
            {
                ProcessingUnit* pu = mProcessingUnits[p].Get();
                const ProcessingUnitDeclaration& puDecl = mManifest.processingUnits[p];

                // Forward dep order = array order (spec: array order is dep order).
                for (unsigned int m = 0; m < puDecl.modules.Size(); ++m)
                {
                    const ModuleDeclaration& modDecl = puDecl.modules[m];
                    if (ModuleIsInStage(modDecl, stage))
                    {
                        Module* module = pu->FindModule(modDecl.instanceId);
                        DIA_ASSERT(module != nullptr,
                                   "Application::Start() — module '%s' not found in PU '%s'",
                                   modDecl.instanceId.AsChar(), puDecl.instanceId.AsChar());
                        if (module && module->GetState() == ModuleState::kInactive)
                        {
                            DeliverStageLaunch(module, stage, Dia::Core::StringCRC());
                            module->BeginStart();
                        }
                    }
                }
            }
        }

        return true;
    }

    //--------------------------------------------------------------------------
    // Update
    //--------------------------------------------------------------------------

    bool Application::Update(float deltaTime)
    {
        DIA_ASSERT(mStarted, "Application::Update() called before Start()");

        // --- Shutdown path: begin stopping all active modules ----------------
        if (mShuttingDown.load())
        {
            // Stop all active modules once — BeginStop is safe to call only
            // if the module is kActive.  Subsequent calls will find it in
            // kStopping or kInactive.
            if (!mShutdownInitiated)
            {
                mShutdownInitiated = true;
#ifdef _DEBUG
                if (mPerfPause) mPerfPause->ReleaseOwner(PerfPauseController::kLocalOwner);
#endif
                BeginStopAllActive();
            }

            // Tick the main PU so stopping modules can wind down.
            UpdateInlineProcessingUnits(deltaTime);

            // Return false once everything has settled.
            if (AllModulesInactive())
            {
                // Join dedicated threads now that all modules are down and no
                // further work will be scheduled on them.  Deferred here (not
                // in RequestShutdown) so callers don't block on thread joins.
                JoinDedicatedThreads();

                DIA_LOG_INFO("Application", "Application shutdown complete — all modules inactive");
                return false;
            }
            return true;
        }

        // --- Drain in-progress transition ------------------------------------
        // Must run before ApplyPendingTransition so the stop phase completes
        // before any new pending transition is consumed.
        if (mTransitionDraining)
        {
            TickTransitionDrain();
        }

        // --- Apply pending stage transition (queues stop phase) --------------
        if (mHasPendingTransition.load())
        {
            ApplyPendingTransition();
        }

        // --- Tick the main PU ------------------------------------------------
        UpdateInlineProcessingUnits(deltaTime);

        // --- Commit gate: make registered ServiceStreams visible to consumers ---
        CommitReadyServiceStreams();

        // --- Error policy: check for failed modules --------------------------
        if (AnyModuleFailed())
        {
            // Boot stage: always shut down.
            if (mCurrentStage == mManifest.initialStage)
            {
                DIA_LOG_ERROR("Application", "Module failed during boot stage '%s' — shutting down",
                              mCurrentStage.AsChar());
                RequestShutdown();
            }
            else
            {
#if defined(NDEBUG)
                // Release, non-boot: attempt rollback by re-entering the stage.
                // A deterministic failure (missing asset, bad config) would
                // otherwise loop forever — cap retries per stage and escalate
                // to shutdown if we exceed the cap.
                if (mLastRollbackStage == mCurrentStage)
                {
                    ++mRollbackAttempts;
                }
                else
                {
                    mLastRollbackStage = mCurrentStage;
                    mRollbackAttempts  = 1;
                }

                if (mRollbackAttempts > kMaxRollbackAttempts)
                {
                    DIA_LOG_ERROR("Application",
                                  "Module failed in stage '%s' — rollback exceeded %u attempts, shutting down",
                                  mCurrentStage.AsChar(), kMaxRollbackAttempts);
                    RequestShutdown();
                }
                else
                {
                    DIA_LOG_ERROR("Application",
                                  "Module failed in stage '%s' — attempting rollback (attempt %u/%u)",
                                  mCurrentStage.AsChar(), mRollbackAttempts, kMaxRollbackAttempts);

                    {
                        LifecycleEvent ev;
                        ev.kind             = LifecycleEventKind::kRollbackAttempted;
                        ev.fromStage        = mCurrentStage;
                        ev.toStage          = mCurrentStage;
                        ev.rollbackAttempt  = mRollbackAttempts;
                        EmitLifecycleEvent(ev);
                    }

                    // Stop all active modules in the current stage.
                    for (unsigned int p = 0; p < mProcessingUnitCount; ++p)
                    {
                        ProcessingUnit* pu = mProcessingUnits[p].Get();
                        const ProcessingUnitDeclaration& puDecl = mManifest.processingUnits[p];
                        for (unsigned int m = 0; m < puDecl.modules.Size(); ++m)
                        {
                            const ModuleDeclaration& modDecl = puDecl.modules[m];
                            if (ModuleIsInStage(modDecl, mCurrentStage))
                            {
                                Module* module = pu->FindModule(modDecl.instanceId);
                                if (module && module->GetState() == ModuleState::kActive)
                                {
                                    module->BeginStop();
                                }
                            }
                        }
                    }

                    // Re-queue the current stage so modules restart next transition.
                    TransitionTo(mCurrentStage);
                }
#else
                // Debug: assert hard.
                DIA_ASSERT(false, "Module failed in stage '%s' — investigate and fix",
                           mCurrentStage.AsChar());
#endif
            }
        }

        // Continue running while not all modules are inactive.
        // (Auto-advance is handled inside ApplyPendingTransition when a transition commits.)
        return true;
    }

    //--------------------------------------------------------------------------
    // TransitionTo  (thread-safe)
    //--------------------------------------------------------------------------

    void Application::TransitionTo(const Dia::Core::StringCRC& stageId)
    {
        TransitionToInternal(stageId, Dia::Core::StringCRC());
    }

    void Application::TransitionTo(const Dia::Core::StringCRC& stageId,
                                   const Dia::Core::StringCRC& presetId)
    {
        if (presetId.Value() != 0 && !mManifest.HasPreset(stageId, presetId))
        {
            DIA_LOG_ERROR("Application", "Unknown preset '%s' for stage '%s', transition refused",
                          presetId.AsChar(), stageId.AsChar());
            return;
        }
        TransitionToInternal(stageId, presetId);
    }

    void Application::SetInitialLaunch(const Dia::Core::StringCRC& stageId, const Dia::Core::StringCRC& presetId)
    {
        std::lock_guard<std::mutex> lock(mTransitionMutex);
        mInitialStage     = stageId;
        mInitialPreset    = presetId;
        mHasInitialLaunch = stageId.Value() != 0;
        if (mHasInitialLaunch)
            DIA_LOG_INFO("Application", "initial launch recorded: stage='%s' preset='%s'", stageId.AsChar(), presetId.AsChar());
    }

    bool Application::TakeInitialLaunch(Dia::Core::StringCRC& stageId, Dia::Core::StringCRC& presetId)
    {
        std::lock_guard<std::mutex> lock(mTransitionMutex);
        if (!mHasInitialLaunch) return false;
        mHasInitialLaunch = false;
        stageId  = mInitialStage;
        presetId = mInitialPreset;
        return true;
    }

    Dia::Core::StringCRC Application::GetCurrentPreset() const
    {
        return mCurrentPreset;
    }

    void Application::DeliverStageLaunch(Module* module, const Dia::Core::StringCRC& stage,
                                         const Dia::Core::StringCRC& preset)
    {
        const char* json = "";
        if (preset.Value() != 0)
        {
            if (const PresetDeclaration* p = mManifest.FindPreset(stage, preset))
                json = mManifest.GetPresetOverrideJson(*p, module->GetInstanceId());
        }
        if (json[0] != '\0')
        {
            DIA_LOG_INFO("Application", "Delivering launch override to module '%s' (%u bytes)",
                         module->GetInstanceId().AsChar(),
                         static_cast<unsigned int>(std::strlen(json)));
        }
        module->OnStageLaunch(json);
    }

    void Application::TransitionToInternal(const Dia::Core::StringCRC& stageId,
                                           const Dia::Core::StringCRC& presetId)
    {
        {
            LifecycleEvent ev;
            ev.kind      = LifecycleEventKind::kStageTransitionRequested;
            ev.fromStage = mCurrentStage;
            ev.toStage   = stageId;
            EmitLifecycleEvent(ev);
        }

        std::lock_guard<std::mutex> lock(mTransitionMutex);
        if (mPendingStage != stageId)
            mPendingHeldByGuardEmitted = false;  // new target — re-arm held-by-guard event
        mPendingStage = stageId;
        mPendingPreset = presetId;
        mHasPendingTransition.store(true);
    }

    //--------------------------------------------------------------------------
    // RequestShutdown  (thread-safe)
    //--------------------------------------------------------------------------

    void Application::RequestShutdown()
    {
        if (mShuttingDown.exchange(true))
        {
            return; // Already requested.
        }

        DIA_LOG_INFO("Application", "Application shutdown requested");

        // Emit lifecycle event before signalling threads.
        {
            LifecycleEvent ev;
            ev.kind = LifecycleEventKind::kShutdownRequested;
            EmitLifecycleEvent(ev);
        }

        // Unblock any kBlock EventStreamStore writers waiting on condvar.
        for (unsigned int i = 0; i < mStreamStoreCount; ++i)
        {
            mStreamStores[i]->NotifyShutdown();
        }

        // Signal dedicated-PU threads to stop their loops.  Do NOT join here
        // — RequestShutdown is thread-safe and may be invoked from inside a
        // module's DoUpdate on the main PU; joining inline would make the
        // caller block on every dedicated thread finishing its current frame.
        // The joins happen in Update()'s shutdown tail (or the destructor as
        // a safety net) once AllModulesInactive() has settled.
        for (unsigned int p = 1; p < mProcessingUnitCount; ++p)
        {
            mProcessingUnits[p]->RequestStop();
        }
    }

    //--------------------------------------------------------------------------
    // RegisterTransitionGuard
    //--------------------------------------------------------------------------

    bool Application::RegisterTransitionGuard(Module* owner, TransitionGuardFn fn)
    {
        DIA_ASSERT(!mGuards.IsFull(),
                   "Application::RegisterTransitionGuard — kMaxGuards (%u) exceeded; "
                   "increase capacity or remove an unused guard",
                   kMaxGuards);
        if (mGuards.IsFull())
            return false;

        GuardEntry entry;
        entry.owner = owner;
        entry.fn    = std::move(fn);
        mGuards.Add(entry);
        return true;
    }

    //--------------------------------------------------------------------------
    // UnregisterTransitionGuards
    //--------------------------------------------------------------------------

    void Application::UnregisterTransitionGuards(Module* owner)
    {
        for (unsigned int i = 0; i < mGuards.Size(); )
        {
            if (mGuards[i].owner == owner)
                mGuards.RemoveAt(i);
            else
                ++i;
        }
    }

    //--------------------------------------------------------------------------
    // CheckGuards  (private)
    //--------------------------------------------------------------------------

    GuardResult Application::CheckGuards()
    {
        for (unsigned int i = 0; i < mGuards.Size(); ++i)
        {
            if (mGuards[i].fn() == GuardResult::Hold)
            {
                mLastGuardCheckResult = GuardResult::Hold;
                return GuardResult::Hold;
            }
        }
        mLastGuardCheckResult = GuardResult::Allow;
        return GuardResult::Allow;
    }

    //--------------------------------------------------------------------------
    // IsShuttingDown
    //--------------------------------------------------------------------------

    bool Application::IsShuttingDown() const
    {
        return mShuttingDown.load();
    }

    //--------------------------------------------------------------------------
    // GetCurrentStage
    //--------------------------------------------------------------------------

    Dia::Core::StringCRC Application::GetCurrentStage() const
    {
        return mCurrentStage;
    }

    //--------------------------------------------------------------------------
    // IsTransitioning
    //--------------------------------------------------------------------------

    bool Application::IsTransitioning() const
    {
        // True while a queued transition hasn't been consumed yet, or while we are
        // waiting for outgoing modules to go inactive before committing the new stage.
        // Initial module startup (after Start()) is NOT a transition.
        return mHasPendingTransition.load() || mTransitionDraining;
    }

    //--------------------------------------------------------------------------
    // GetTransitionInfo
    //--------------------------------------------------------------------------

    TransitionInfo Application::GetTransitionInfo() const
    {
        TransitionInfo info;
        {
            std::lock_guard<std::mutex> lock(mTransitionMutex);
            info.inProgress = mHasPendingTransition.load();
            info.fromStage  = mCurrentStage;
            info.toStage    = mPendingStage;
        }
        // heldByGuards: use cached result from last frame's CheckGuards() call to
        // avoid calling guard fns from an inspectable getter (AC10).
        info.heldByGuards = info.inProgress
                            && !mGuards.IsEmpty()
                            && mLastGuardCheckResult == GuardResult::Hold;
        // Populate starting/stopping by scanning module states
        for (unsigned int p = 0; p < mProcessingUnitCount; ++p)
        {
            const ProcessingUnitDeclaration& puDecl = mManifest.processingUnits[p];
            for (unsigned int m = 0; m < puDecl.modules.Size(); ++m)
            {
                const Module* mod = mProcessingUnits[p]->FindModule(puDecl.modules[m].instanceId);
                if (!mod) continue;
                if (mod->GetState() == ModuleState::kStarting && !info.modulesStarting.IsFull())
                    info.modulesStarting.Add(puDecl.modules[m].instanceId);
                else if (mod->GetState() == ModuleState::kStopping && !info.modulesStopping.IsFull())
                    info.modulesStopping.Add(puDecl.modules[m].instanceId);
            }
        }
        return info;
    }

    //--------------------------------------------------------------------------
    // GetAllStages
    //--------------------------------------------------------------------------

    void Application::GetAllStages(
        Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 32>& out) const
    {
        for (unsigned int i = 0; i < mManifest.stages.Size() && !out.IsFull(); ++i)
            out.Add(mManifest.stages[i].name);
    }

    //--------------------------------------------------------------------------
    // GetStageTransitions
    //--------------------------------------------------------------------------

    void Application::GetStageTransitions(const Dia::Core::StringCRC& stage,
        Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 32>& out) const
    {
        for (unsigned int i = 0; i < mManifest.stages.Size(); ++i)
        {
            if (mManifest.stages[i].name == stage)
            {
                if (mManifest.stages[i].transitions.Size() > 0)
                {
                    // Explicit transitions declared — return them as-is.
                    for (unsigned int t = 0; t < mManifest.stages[i].transitions.Size() && !out.IsFull(); ++t)
                        out.Add(mManifest.stages[i].transitions[t]);
                }
                else
                {
                    // No transitions declared — auto-derive: every stage except this one.
                    for (unsigned int s = 0; s < mManifest.stages.Size() && !out.IsFull(); ++s)
                    {
                        if (mManifest.stages[s].name != stage)
                            out.Add(mManifest.stages[s].name);
                    }
                }
                return;
            }
        }
    }

    //--------------------------------------------------------------------------
    // GetProcessingUnits
    //--------------------------------------------------------------------------

    void Application::GetProcessingUnits(
        Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 4>& out) const
    {
        for (unsigned int p = 0; p < mProcessingUnitCount && !out.IsFull(); ++p)
            out.Add(mManifest.processingUnits[p].instanceId);
    }

    bool Application::ResolveUnitForThread(uint32_t threadId, Dia::Core::StringCRC& outUnitId) const
    {
        if (threadId == 0)
            return false;
        for (unsigned int p = 0; p < mProcessingUnitCount; ++p)
        {
            if (mUnitThreadIds[p] == threadId)
            {
                outUnitId = mProcessingUnits[p]->GetInstanceId();
                return true;
            }
        }
        return false;
    }

#ifdef _DEBUG
    bool Application::ResolveModuleType(void* ctx, const Dia::Core::StringCRC& instanceId, Dia::Core::StringCRC& outTypeId)
    {
        const Application* self = static_cast<const Application*>(ctx);
        for (unsigned int p = 0; p < self->mProcessingUnitCount; ++p)
        {
            const Module* m = self->mProcessingUnits[p]->FindModule(instanceId);
            if (m) { outTypeId = m->GetTypeId(); return true; }
        }
        return false;
    }
#endif

    //--------------------------------------------------------------------------
    // GetActiveModules
    //--------------------------------------------------------------------------

    void Application::GetActiveModules(
        const Dia::Core::StringCRC& puId,
        Dia::Core::Containers::DynamicArrayC<ModuleStateInfo, 64>& out) const
    {
        for (unsigned int p = 0; p < mProcessingUnitCount; ++p)
        {
            if (mManifest.processingUnits[p].instanceId != puId)
                continue;
            const ProcessingUnitDeclaration& puDecl = mManifest.processingUnits[p];
            for (unsigned int m = 0; m < puDecl.modules.Size() && !out.IsFull(); ++m)
            {
                const Module* mod = mProcessingUnits[p]->FindModule(puDecl.modules[m].instanceId);
                if (!mod) continue;
                ModuleStateInfo info;
                info.instanceId  = puDecl.modules[m].instanceId;
                info.typeId      = puDecl.modules[m].typeId;
                info.state       = mod->GetState();
                info.allowedPUs  = mRegistry.GetAllowedPUs(info.typeId);
                info.description = mRegistry.GetDescription(info.typeId);
                out.Add(info);
            }
            break;
        }
    }

    //--------------------------------------------------------------------------
    // GetStreamInfo
    //--------------------------------------------------------------------------

    void Application::GetStreamInfo(
        Dia::Core::Containers::DynamicArrayC<StreamInfo, 16>& out) const
    {
        for (unsigned int i = 0; i < mManifest.streams.Size() && !out.IsFull(); ++i)
        {
            const StreamDeclaration& sd = mManifest.streams[i];
            StreamInfo si;
            si.id          = sd.id;
            si.type        = sd.kind;  // kind maps to the legacy 'type' field in StreamInfo
            si.fromPU      = sd.fromPU;
            si.toPU        = sd.toPU;
            si.multiWriter = sd.multiWriter;

            // F4 runtime fields — look up the live store.
            const IStreamStore* store = FindStreamStore(sd.id);
            if (store != nullptr)
            {
                si.kind                = store->GetKind();
                si.overflowPolicy      = store->GetOverflowPolicy();
                si.currentSequence     = store->GetLastSequence();
                si.attachedReaderCount = store->GetRegisteredReaderCount();
                si.attachedTapCount    = store->GetTapCount();
            }
            else
            {
                // Store not yet registered (pre-Start) — fill safe defaults.
                si.kind                = StreamKind::kEvent;
                si.overflowPolicy      = sd.overflowPolicy;
                si.currentSequence     = 0;
                si.attachedReaderCount = 0;
                si.attachedTapCount    = 0;
            }

            out.Add(si);
        }
    }

    //--------------------------------------------------------------------------
    // BuildFromManifest  (private)
    //--------------------------------------------------------------------------

    void Application::SetTripReporter(Dia::Observation::Profile::TripReporter* reporter)
    {
        for (unsigned int p = 0; p < mProcessingUnitCount; ++p)
            mProcessingUnits[p]->SetTripReporter(reporter);
    }

    void Application::SetTripConfig(const Dia::Observation::Profile::TripConfig& config)
    {
        for (unsigned int p = 0; p < mProcessingUnitCount; ++p)
            mProcessingUnits[p]->SetTripConfig(config);
    }

    void Application::CollectPerfSummary(Dia::Observation::Profile::PerfSummary& out) const
    {
        for (unsigned int p = 0; p < mProcessingUnitCount; ++p)
            mProcessingUnits[p]->CollectPerfSummary(out);
    }

    //--------------------------------------------------------------------------
    bool Application::BuildFromManifest()
    {
        DIA_ASSERT(mManifest.processingUnits.Size() <= kMaxProcessingUnits,
                   "Application::BuildFromManifest() — manifest has more PUs than kMaxProcessingUnits (%u)",
                   kMaxProcessingUnits);

        const unsigned int puCount =
            (mManifest.processingUnits.Size() <= kMaxProcessingUnits)
            ? mManifest.processingUnits.Size()
            : kMaxProcessingUnits;

        for (unsigned int p = 0; p < puCount; ++p)
        {
            const ProcessingUnitDeclaration& puDecl = mManifest.processingUnits[p];

            // Create the ProcessingUnit.  The PU is a pure scheduler — it does
            // NOT get a back-pointer to Application.  Modules that need to
            // reach the Application get one via Module::SetApplication below.
            Dia::Core::UniquePtr<ProcessingUnit> pu(
                new ProcessingUnit(puDecl.instanceId,
                                   puDecl.frequencyHz,
                                   puDecl.dedicatedThread,
                                   puDecl.maxCatchUpTicksPerFrame));

            // Create and add modules in manifest array order (= dep order).
            for (unsigned int m = 0; m < puDecl.modules.Size(); ++m)
            {
                const ModuleDeclaration& modDecl = puDecl.modules[m];

                Module* rawModule = mRegistry.Create(modDecl.typeId, modDecl.instanceId);
                if (rawModule == nullptr)
                {
                    DIA_LOG_ERROR("Application",
                                  "BuildFromManifest: failed to create module '%s' (type '%s')",
                                  modDecl.instanceId.AsChar(), modDecl.typeId.AsChar());
                    return false;
                }

                rawModule->SetApplication(this);
                rawModule->SetTypeId(modDecl.typeId);

                // Deliver per-module config from the manifest.  Runs once,
                // before BeginStart, on the main thread.
                DIA_LOG_DEBUG("module", "module.configure module_id=%s", modDecl.instanceId.AsChar());
                rawModule->OnConfigure(modDecl.configJson.AsCStr());

                Dia::Core::UniquePtr<Module> modulePtr(rawModule);
                pu->AddModule(std::move(modulePtr),
                              modDecl.startTimeoutMs,
                              modDecl.stopTimeoutMs);
            }

            mProcessingUnits[p] = std::move(pu);
            ++mProcessingUnitCount;
        }

        return true;
    }

    //--------------------------------------------------------------------------
    // ApplyPendingTransition  (private, called from Update)
    //--------------------------------------------------------------------------

    void Application::ApplyPendingTransition()
    {
        // Peek at the pending stage without consuming, so guards can re-evaluate
        // next frame if they return Hold.
        Dia::Core::StringCRC pendingStage;
        {
            std::lock_guard<std::mutex> lock(mTransitionMutex);
            if (!mHasPendingTransition.load())
                return;
            pendingStage = mPendingStage;
        }

        // No-op: already in target stage — consume and clear.
        if (mCurrentStage == pendingStage)
        {
            std::lock_guard<std::mutex> lock(mTransitionMutex);
            mPendingStage = Dia::Core::StringCRC();
            mPendingPreset = Dia::Core::StringCRC();
            mHasPendingTransition.store(false);
            mPendingHeldByGuardEmitted = false;
            return;
        }

        // Guard check — runs on main thread, guards must be idempotent.
        if (CheckGuards() == GuardResult::Hold)
        {
            if (!mPendingHeldByGuardEmitted)
            {
                LifecycleEvent ev;
                ev.kind      = LifecycleEventKind::kStageTransitionHeldByGuard;
                ev.fromStage = mCurrentStage;
                ev.toStage   = pendingStage;
                EmitLifecycleEvent(ev);
                mPendingHeldByGuardEmitted = true;
            }
            return;  // try again next frame
        }

        // All guards allowed — consume the pending transition.
        Dia::Core::StringCRC newStage;
        {
            std::lock_guard<std::mutex> lock(mTransitionMutex);
            newStage = mPendingStage;
            mLaunchPreset = mPendingPreset;
            mPendingStage = Dia::Core::StringCRC();
            mPendingPreset = Dia::Core::StringCRC();
            mHasPendingTransition.store(false);
            mPendingHeldByGuardEmitted = false;
        }

        DIA_LOG_INFO("Application", "Stage transition: '%s' -> '%s'",
                     mCurrentStage.AsChar(), newStage.AsChar());

#ifdef _DEBUG
        // Local (in-game/test) pauses never survive a stage exit.
        if (mPerfPause) mPerfPause->ReleaseOwner(PerfPauseController::kLocalOwner);
#endif

        {
            LifecycleEvent ev;
            ev.kind      = LifecycleEventKind::kStageTransitionStarted;
            ev.fromStage = mCurrentStage;
            ev.toStage   = newStage;
            EmitLifecycleEvent(ev);
        }

        const Dia::Core::StringCRC oldStage = mCurrentStage;

        // --- Stop modules active in old stage but NOT in new stage -----------
        // Iterate in REVERSE dep order (reverse array order).
        for (unsigned int p = 0; p < mProcessingUnitCount; ++p)
        {
            ProcessingUnit* pu = mProcessingUnits[p].Get();
            const ProcessingUnitDeclaration& puDecl = mManifest.processingUnits[p];

            for (int m = static_cast<int>(puDecl.modules.Size()) - 1; m >= 0; --m)
            {
                const ModuleDeclaration& modDecl = puDecl.modules[static_cast<unsigned int>(m)];
                if (ModuleIsInStage(modDecl, oldStage) && !ModuleIsInStage(modDecl, newStage))
                {
                    Module* module = pu->FindModule(modDecl.instanceId);
                    if (module && module->GetState() == ModuleState::kActive)
                        module->BeginStop();
                }
            }
        }

        // Begin drain: do NOT commit the stage or start incoming modules yet.
        // TickTransitionDrain() will do that once all outgoing modules are kInactive.
        mDrainingToStage    = newStage;
        mTransitionDraining = true;
    }

    //--------------------------------------------------------------------------
    // TickTransitionDrain  (private, called from Update while mTransitionDraining)
    //--------------------------------------------------------------------------

    void Application::TickTransitionDrain()
    {
        DIA_PROFILE_SCOPE("app.transition_drain", Observation::Profile::Category::kDiaApplicationFlow);
        DIA_TRACE_ZONE("app.transition_drain", Observation::Trace::Category::kDiaApplicationFlow);

        const Dia::Core::StringCRC oldStage = mCurrentStage;   // still the old stage
        const Dia::Core::StringCRC newStage = mDrainingToStage;

        // Check whether all outgoing modules (in old stage, not in new) are kInactive.
        for (unsigned int p = 0; p < mProcessingUnitCount; ++p)
        {
            const ProcessingUnitDeclaration& puDecl = mManifest.processingUnits[p];
            for (unsigned int m = 0; m < puDecl.modules.Size(); ++m)
            {
                const ModuleDeclaration& modDecl = puDecl.modules[m];
                if (ModuleIsInStage(modDecl, oldStage) && !ModuleIsInStage(modDecl, newStage))
                {
                    const Module* module = mProcessingUnits[p]->FindModule(modDecl.instanceId);
                    if (module)
                    {
                        ModuleState s = module->GetState();
                        if (s != ModuleState::kInactive && s != ModuleState::kFailed)
                            return; // Still draining — outgoing modules not yet down.
                    }
                }
            }
        }

        // All outgoing modules are down — commit stage and start incoming.
        mCurrentStage       = newStage;
        mCurrentPreset      = mLaunchPreset;
        mTransitionDraining = false;

        // A successful transition into a new stage resets the rollback counter:
        // rollback attempts are only counted consecutively within the same stage.
        if (mLastRollbackStage != newStage)
        {
            mLastRollbackStage = Dia::Core::StringCRC();
            mRollbackAttempts  = 0;
        }

        DIA_LOG_INFO("Application", "Stage transition drain complete, entering '%s'",
                     newStage.AsChar());

        {
            LifecycleEvent ev;
            ev.kind      = LifecycleEventKind::kStageTransitionCommitted;
            ev.fromStage = oldStage;
            ev.toStage   = newStage;
            EmitLifecycleEvent(ev);
        }

        // Reset ServiceStreams whose provider module is leaving the stage.
        {
            DIA_PROFILE_SCOPE("app.service_stream_reset", Observation::Profile::Category::kDiaApplicationFlow);
            DIA_TRACE_ZONE("app.service_stream_reset", Observation::Trace::Category::kDiaApplicationFlow);

            static const Dia::Core::StringCRC kProvides("provides");
            static const Dia::Core::StringCRC kServiceStream("ServiceStream");
            for (unsigned int i = 0; i < mStreamStoreCount; ++i)
            {
                IStreamStore* store = mStreamStores[i].Get();
                if (store->GetKind() != StreamKind::kService || !store->IsCommitted())
                    continue;

                const Dia::Core::StringCRC streamId = store->GetId();
                bool providerLeaving = false;

                for (unsigned int p = 0; p < mManifest.processingUnits.Size() && !providerLeaving; ++p)
                {
                    const ProcessingUnitDeclaration& puDecl = mManifest.processingUnits[p];
                    for (unsigned int m = 0; m < puDecl.modules.Size() && !providerLeaving; ++m)
                    {
                        const ModuleDeclaration& modDecl = puDecl.modules[m];
                        for (unsigned int c = 0; c < modDecl.channels.Size(); ++c)
                        {
                            if (modDecl.channels[c].id == streamId && modDecl.channels[c].role == kProvides)
                            {
                                if (ModuleIsInStage(modDecl, oldStage) && !ModuleIsInStage(modDecl, newStage))
                                    providerLeaving = true;
                                break;
                            }
                        }
                    }
                }

                if (providerLeaving)
                {
                    DIA_LOG_INFO("Application", "Resetting ServiceStream '%s' — provider leaving stage",
                                 streamId.AsChar());
                    store->Reset();
                    if (mServiceStreamResetCount)
                        mServiceStreamResetCount->Inc();
                }
            }
        }

        ValidateCostagingRequirements(newStage);

        // Start modules in new stage that are not already started (forward dep order).
        for (unsigned int p = 0; p < mProcessingUnitCount; ++p)
        {
            ProcessingUnit* pu = mProcessingUnits[p].Get();
            const ProcessingUnitDeclaration& puDecl = mManifest.processingUnits[p];
            for (unsigned int m = 0; m < puDecl.modules.Size(); ++m)
            {
                const ModuleDeclaration& modDecl = puDecl.modules[m];
                if (ModuleIsInStage(modDecl, newStage))
                {
                    Module* module = pu->FindModule(modDecl.instanceId);
                    if (module && module->GetState() == ModuleState::kInactive)
                    {
                        DeliverStageLaunch(module, newStage, mLaunchPreset);
                        module->BeginStart();
                    }
                }
            }
        }
        mLaunchPreset = Dia::Core::StringCRC();

        // Auto-advance: per-stage flag in v3 replaces the old top-level auto_stages array.
        for (unsigned int s = 0; s < mManifest.stages.Size(); ++s)
        {
            const StageDeclaration& decl = mManifest.stages[s];
            if (decl.name == newStage && decl.autoAdvance)
            {
                if (decl.transitions.Size() == 1)
                {
                    DIA_LOG_INFO("Application", "Auto-advancing from stage '%s' to '%s'",
                                 newStage.AsChar(), decl.transitions[0].AsChar());
                    TransitionTo(decl.transitions[0]);
                }
                else
                {
                    DIA_LOG_WARNING("Application",
                        "Stage '%s' has auto_advance=true but %u transitions (validator should have caught this)",
                        newStage.AsChar(), decl.transitions.Size());
                }
                break;
            }
        }
    }

    //--------------------------------------------------------------------------
    // BeginStopAllActive  (private)
    //--------------------------------------------------------------------------

    void Application::BeginStopAllActive()
    {
        DIA_LOG_INFO("Application", "Application shutdown: stopping all active modules");

        // Reverse order across all PUs.
        for (unsigned int p = 0; p < mProcessingUnitCount; ++p)
        {
            ProcessingUnit* pu = mProcessingUnits[p].Get();
            const ProcessingUnitDeclaration& puDecl = mManifest.processingUnits[p];

            for (int m = static_cast<int>(puDecl.modules.Size()) - 1; m >= 0; --m)
            {
                const ModuleDeclaration& modDecl = puDecl.modules[static_cast<unsigned int>(m)];
                Module* module = pu->FindModule(modDecl.instanceId);
                if (module && module->GetState() == ModuleState::kActive)
                {
                    module->BeginStop();
                }
            }
        }
    }

    //--------------------------------------------------------------------------
    // UpdateInlineProcessingUnits  (private)
    //
    // PU 0 always runs inline. A secondary PU declared dedicatedThread=false also
    // runs inline (it has no thread), so it is ticked here after PU 0, in manifest
    // order. Dedicated-thread PUs are driven by their own thread.
    //--------------------------------------------------------------------------

    void Application::UpdateInlineProcessingUnits(float deltaTime)
    {
        for (unsigned int p = 0; p < mProcessingUnitCount; ++p)
        {
            if (p == 0 || !mProcessingUnits[p]->IsDedicatedThread())
                mProcessingUnits[p]->Update(deltaTime);
        }
    }

    //--------------------------------------------------------------------------
    // AllModulesInactive  (private)
    //--------------------------------------------------------------------------

    bool Application::AllModulesInactive() const
    {
        for (unsigned int p = 0; p < mProcessingUnitCount; ++p)
        {
            const ProcessingUnitDeclaration& puDecl = mManifest.processingUnits[p];
            for (unsigned int m = 0; m < puDecl.modules.Size(); ++m)
            {
                const Module* module = mProcessingUnits[p]->FindModule(puDecl.modules[m].instanceId);
                if (module)
                {
                    const ModuleState s = module->GetState();
                    if (s != ModuleState::kInactive && s != ModuleState::kFailed)
                    {
                        return false;
                    }
                }
            }
        }

        return true;
    }

    //--------------------------------------------------------------------------
    // AnyModuleFailed  (private)
    //--------------------------------------------------------------------------

    bool Application::AnyModuleFailed() const
    {
        for (unsigned int p = 0; p < mProcessingUnitCount; ++p)
        {
            const ProcessingUnitDeclaration& puDecl = mManifest.processingUnits[p];
            for (unsigned int m = 0; m < puDecl.modules.Size(); ++m)
            {
                const Module* module = mProcessingUnits[p]->FindModule(puDecl.modules[m].instanceId);
                if (module && module->GetState() == ModuleState::kFailed)
                {
                    return true;
                }
            }
        }
        return false;
    }

    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    // RegisterBaselineCommands  (private)
    //--------------------------------------------------------------------------

    namespace {
        static const char* ModuleStateToString(ModuleState state)
        {
            switch (state)
            {
                case ModuleState::kInactive:  return "Inactive";
                case ModuleState::kStarting:  return "Starting";
                case ModuleState::kActive:    return "Active";
                case ModuleState::kStopping:  return "Stopping";
                case ModuleState::kFailed:    return "Failed";
            }
            return "Unknown";
        }
    }

    void Application::RegisterBaselineCommands()
    {
        {
            Dia::API::CommandInfoJson quitCmd;
            quitCmd.name        = Dia::Core::StringCRC("dia.app.quit");
            quitCmd.description = "Request application shutdown";
            quitCmd.category    = Dia::Core::StringCRC("dia.app");
            quitCmd.owner       = "DiaApplicationFlow";
            quitCmd.callback    = [this](const Json::Value&) -> Json::Value {
                RequestShutdown();
                return Json::Value(Json::objectValue);
            };
            Dia::API::RegisterCommandJson(quitCmd);
        }
        {
            Dia::API::CommandInfoJson reportCmd;
            reportCmd.name        = Dia::Core::StringCRC("dia.app.report");
            reportCmd.description = "Report current application state";
            reportCmd.category    = Dia::Core::StringCRC("dia.app");
            reportCmd.owner       = "DiaApplicationFlow";
            reportCmd.callback    = [this](const Json::Value&) -> Json::Value {
                Json::Value result;
                result["stage"] = GetCurrentStage().AsChar();

                // Modules from all PUs
                Json::Value modulesArr(Json::arrayValue);
                for (unsigned int p = 0; p < mProcessingUnitCount; ++p)
                {
                    const ProcessingUnitDeclaration& puDecl = mManifest.processingUnits[p];
                    for (unsigned int m = 0; m < puDecl.modules.Size(); ++m)
                    {
                        const Module* mod = mProcessingUnits[p]->FindModule(puDecl.modules[m].instanceId);
                        if (!mod) continue;
                        Json::Value entry;
                        entry["instanceId"] = puDecl.modules[m].instanceId.AsChar();
                        entry["typeId"]     = puDecl.modules[m].typeId.AsChar();
                        entry["state"]      = ModuleStateToString(mod->GetState());
                        modulesArr.append(entry);
                    }
                }
                result["modules"] = modulesArr;

                TransitionInfo ti = GetTransitionInfo();
                Json::Value transition;
                transition["inProgress"]  = ti.inProgress;
                transition["fromStage"]   = ti.fromStage.AsChar();
                transition["toStage"]     = ti.toStage.AsChar();
                transition["heldByGuards"] = ti.heldByGuards;
                result["transition"] = transition;

#ifdef _DEBUG
                // Perf live control: active module/domain pauses (Debug only). owner 0 = local, else connId + 1.
                {
                    Json::Value pausesArr(Json::arrayValue);
                    if (mPerfPause)
                    {
                        PerfPauseEntry entries[PerfPauseController::kMaxEntries];
                        const unsigned int n = mPerfPause->ActivePauses(entries, PerfPauseController::kMaxEntries);
                        for (unsigned int i = 0; i < n; ++i)
                        {
                            Json::Value e;
                            const bool isModule = entries[i].kind == PerfPauseKind::kModule;
                            e["kind"]  = isModule ? "module" : "domain";
                            e["crc"]   = entries[i].crc;
                            e["owner"] = entries[i].owner;
                            if (isModule)
                            {
                                // Names are not stored in the controller; recover them from the manifest.
                                for (unsigned int p = 0; p < mProcessingUnitCount; ++p)
                                {
                                    const ProcessingUnitDeclaration& puDecl = mManifest.processingUnits[p];
                                    for (unsigned int m = 0; m < puDecl.modules.Size(); ++m)
                                        if (puDecl.modules[m].instanceId.Value() == entries[i].crc)
                                            e["id"] = puDecl.modules[m].instanceId.AsChar();
                                }
                            }
                            pausesArr.append(e);
                        }
                    }
                    result["activePauses"] = pausesArr;
                }
#endif

                return result;
            };
            Dia::API::RegisterCommandJson(reportCmd);
        }
        {
            Dia::API::CommandInfoJson stagesCmd;
            stagesCmd.name        = Dia::Core::StringCRC("dia.manifest.stages");
            stagesCmd.description = "List stages navigable from Boot";
            stagesCmd.category    = Dia::Core::StringCRC("dia.manifest");
            stagesCmd.owner       = "DiaApplicationFlow";
            stagesCmd.callback    = [this](const Json::Value&) -> Json::Value {
                Json::Value result;
                Json::Value stagesArr(Json::arrayValue);

                Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 32> targets;
                GetStageTransitions(Dia::Core::StringCRC("Boot"), targets);
                for (unsigned int i = 0; i < targets.Size(); ++i)
                    stagesArr.append(targets[i].AsChar());

                result["stages"] = stagesArr;
                return result;
            };
            Dia::API::RegisterCommandJson(stagesCmd);
        }
    }

    //--------------------------------------------------------------------------
    // CommitReadyServiceStreams  (private)
    //
    // Called once per Update() tick after the main PU has processed its frame.
    // Any ServiceStreamStore that is registered (provider called Register())
    // but not yet committed is committed here, making it accessible to consumers.
    //
    // This implements the "collected-then-committed" gate from the spec:
    // providers call Register() from DoStart(), and this function commits all
    // of them as soon as they are registered.  The validator guarantees exactly
    // one provider per ServiceStream, so "registered = ready to commit".
    //--------------------------------------------------------------------------

    void Application::CommitReadyServiceStreams()
    {
        DIA_PROFILE_SCOPE("app.service_commit_gate", Observation::Profile::Category::kDiaApplicationFlow);
        DIA_TRACE_ZONE("app.service_commit_gate", Observation::Trace::Category::kDiaApplicationFlow);

        unsigned int committedTotal = 0;

        for (unsigned int i = 0; i < mStreamStoreCount; ++i)
        {
            IStreamStore* store = mStreamStores[i].Get();
            if (store->GetKind() != StreamKind::kService)
                continue;

            if (store->IsRegistered() && !store->IsCommitted())
            {
                store->Commit();
                if (mServiceStreamCommitCount)
                    mServiceStreamCommitCount->Inc();
                DIA_LOG_INFO("Application", "ServiceStream '%s' committed", store->GetId().AsChar());
            }

            if (store->IsCommitted())
                ++committedTotal;
        }

        if (mServiceStreamCommittedGauge)
            mServiceStreamCommittedGauge->Set(static_cast<double>(committedTotal));
    }

    // ModuleIsInStage  (private, static)
    //--------------------------------------------------------------------------

    /*static*/ bool Application::ModuleIsInStage(const ModuleDeclaration& decl,
                                                  const Dia::Core::StringCRC& stage)
    {
        static const Dia::Core::StringCRC kAll("all");
        for (unsigned int s = 0; s < decl.stages.Size(); ++s)
        {
            if (decl.stages[s] == kAll || decl.stages[s] == stage)
            {
                return true;
            }
        }
        return false;
    }

    //--------------------------------------------------------------------------
    // ValidateCostagingRequirements  (private)
    //--------------------------------------------------------------------------

    void Application::ValidateCostagingRequirements(const Dia::Core::StringCRC& stage)
    {
        for (unsigned int p = 0; p < mProcessingUnitCount; ++p)
        {
            ProcessingUnit* pu = mProcessingUnits[p].Get();
            const ProcessingUnitDeclaration& puDecl = mManifest.processingUnits[p];
            for (unsigned int m = 0; m < puDecl.modules.Size(); ++m)
            {
                const ModuleDeclaration& modDecl = puDecl.modules[m];
                if (!ModuleIsInStage(modDecl, stage))
                    continue;

                const Module* module = pu->FindModule(modDecl.instanceId);
                if (!module)
                    continue;

                unsigned int reqCount = 0;
                const Dia::Core::StringCRC* reqs = module->GetRequiredModuleTypeIds(reqCount);
                for (unsigned int r = 0; r < reqCount; ++r)
                {
                    const Dia::Core::StringCRC& reqTypeId = reqs[r];
                    bool found = false;

                    for (unsigned int pp = 0; pp < mProcessingUnitCount && !found; ++pp)
                    {
                        const ProcessingUnitDeclaration& puDecl2 = mManifest.processingUnits[pp];
                        for (unsigned int mm = 0; mm < puDecl2.modules.Size() && !found; ++mm)
                        {
                            const ModuleDeclaration& modDecl2 = puDecl2.modules[mm];
                            if (ModuleIsInStage(modDecl2, stage) && modDecl2.typeId == reqTypeId)
                                found = true;
                        }
                    }

                    DIA_ASSERT(found,
                        "Module '%s' (type '%s') requires co-staged module of type '%s', "
                        "which is not active in stage '%s'",
                        modDecl.instanceId.AsChar(), modDecl.typeId.AsChar(),
                        reqTypeId.AsChar(), stage.AsChar());
                }
            }
        }
    }

    //--------------------------------------------------------------------------
    // ConnectModuleStreams  (private)
    //--------------------------------------------------------------------------

    void Application::ConnectModuleStreams()
    {
        // Stream-store registration is only permitted inside this window.
        // See RegisterOrFindStreamStore for the rationale.
        mConnectingStreams = true;

        for (unsigned int p = 0; p < mProcessingUnitCount; ++p)
        {
            const ProcessingUnitDeclaration& puDecl = mManifest.processingUnits[p];
            ProcessingUnit* pu = mProcessingUnits[p].Get();
            for (unsigned int m = 0; m < puDecl.modules.Size(); ++m)
            {
                Module* module = pu->FindModule(puDecl.modules[m].instanceId);
                if (module)
                {
                    DIA_LOG_DEBUG("module", "module.connect_streams module_id=%s", puDecl.modules[m].instanceId.AsChar());
                    module->OnConnectStreams(*this);
                }
            }
        }

        mConnectingStreams = false;
    }

    //--------------------------------------------------------------------------
    // FindStreamStore  (public)
    //--------------------------------------------------------------------------

    IStreamStore* Application::FindStreamStore(const Dia::Core::StringCRC& id) const
    {
        for (unsigned int i = 0; i < mStreamStoreCount; ++i)
        {
            if (mStreamStores[i]->GetId() == id)
            {
                return mStreamStores[i].Get();
            }
        }
        return nullptr;
    }

    //--------------------------------------------------------------------------
    // FindStream  (IApplicationInspectable override)
    //--------------------------------------------------------------------------

    IStreamStore* Application::FindStream(const Dia::Core::StringCRC& id)
    {
        return FindStreamStore(id);
    }

    //--------------------------------------------------------------------------
    // RegisterOrFindStreamStore  (public, main-thread, startup-only)
    //
    // Manifest-gating (F1 AC1): rejects streams not declared in the manifest.
    //--------------------------------------------------------------------------

    IStreamStore* Application::RegisterOrFindStreamStore(Dia::Core::UniquePtr<IStreamStore> newStore)
    {
        DIA_ASSERT(mConnectingStreams,
                   "RegisterOrFindStreamStore called outside OnConnectStreams — "
                   "mStreamStores is only main-thread-safe during startup");

        // --- Manifest-gating: stream ID must be declared in manifest ---------
        bool declaredInManifest = false;
        for (unsigned int i = 0; i < mManifest.streams.Size(); ++i)
        {
            if (mManifest.streams[i].id == newStore->GetId())
            {
                declaredInManifest = true;
                break;
            }
        }

        if (!declaredInManifest)
        {
            DIA_LOG_ERROR("Application",
                          "RegisterOrFindStreamStore: stream '%s' not declared in manifest",
                          newStore->GetId().AsChar());
            DIA_ASSERT(false,
                       "RegisterOrFindStreamStore: stream '%s' not declared in manifest — "
                       "add it to manifest.streams[]",
                       newStore->GetId().AsChar());
            mConnectFailed = true;
            return nullptr;
        }

        // If a store with this ID is already registered, discard the new one
        // and return the existing (first-registrant wins).
        IStreamStore* existing = FindStreamStore(newStore->GetId());
        if (existing)
        {
            return existing;
        }

        DIA_ASSERT(mStreamStoreCount < kMaxStreams,
                   "Application: stream store capacity exceeded (max %u)", kMaxStreams);
        if (mStreamStoreCount >= kMaxStreams)
        {
            mConnectFailed = true;
            return nullptr;
        }

        IStreamStore* raw = newStore.Get();
        mStreamStores[mStreamStoreCount] = std::move(newStore);
        ++mStreamStoreCount;
        return raw;
    }

    //--------------------------------------------------------------------------
    // GetDeclaredEventStreamConfig  (IStreamConnector override)
    //--------------------------------------------------------------------------

    bool Application::GetDeclaredEventStreamConfig(const Dia::Core::StringCRC& streamId,
                                                   unsigned int& outCapacity,
                                                   OverflowPolicy& outPolicy,
                                                   unsigned int& outBlockTimeoutMs) const
    {
        for (unsigned int i = 0; i < mManifest.streams.Size(); ++i)
        {
            const StreamDeclaration& sd = mManifest.streams[i];
            if (sd.id == streamId)
            {
                outCapacity       = sd.capacity;
                outPolicy         = sd.overflowPolicy;
                outBlockTimeoutMs = sd.blockTimeoutMs;
                return true;
            }
        }
        return false;
    }

    //--------------------------------------------------------------------------
    // EmitLifecycleEvent  (public)
    //--------------------------------------------------------------------------

    void Application::EmitLifecycleEvent(const LifecycleEvent& ev)
    {
        // Perf markers (perf-live-control AC7). Runs on whichever thread emits; a thread that has not
        // registered with PerfMarkers drops the mark silently.
        switch (ev.kind)
        {
            case LifecycleEventKind::kStageTransitionRequested:
                DIA_PERF_MARK_VALUE("stage.requested", ev.toStage.Value());
                break;
            case LifecycleEventKind::kStageTransitionStarted:
                DIA_PERF_MARK_VALUE("stage.started", ev.toStage.Value());
                break;
            case LifecycleEventKind::kStageTransitionCommitted:
                DIA_PERF_MARK_VALUE("stage.committed", ev.toStage.Value());
                break;
            case LifecycleEventKind::kModuleStateChanged:
                if (ev.newState == ModuleState::kActive)
                    DIA_PERF_MARK_VALUE("module.started", ev.moduleInstanceId.Value());
                else if (ev.newState == ModuleState::kInactive && ev.previousState == ModuleState::kStopping)
                    DIA_PERF_MARK_VALUE("module.stopped", ev.moduleInstanceId.Value());
                else if (ev.newState == ModuleState::kFailed)
                    DIA_PERF_MARK_VALUE("module.failed", ev.moduleInstanceId.Value());
                break;
            default:
                break;
        }

        if (!mLifecycleStore)
            return;

        static const Dia::Core::StringCRC kFrameworkId("$framework");

        Event<LifecycleEvent> wrapped;
        wrapped.timestampUs = Dia::Core::TimeAbsolute::GetSystemTime().AsLongLongInMicroseconds();
        wrapped.senderCrc   = kFrameworkId.Value();
        wrapped.sequence    = 0;  // stamped inside EventStreamStore::SendInternal
        wrapped.payload     = ev;
        mLifecycleStore->Send(wrapped);
    }

}} // namespace Dia::ApplicationFlow
