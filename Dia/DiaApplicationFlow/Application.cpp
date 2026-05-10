////////////////////////////////////////////////////////////////////////////////
// Filename: Application.cpp
// DiaApplicationFlow — v2 Application
////////////////////////////////////////////////////////////////////////////////
#include "DiaApplicationFlow/Application.h"
#include "DiaApplicationFlow/Module.h"
#include "DiaApplicationFlow/Manifest/ManifestValidatorV2.h"
#include "DiaApplicationFlow/IApplicationInspectable.h"

#include <DiaCore/Core/Assert.h>
#include <DiaLogger/DiaLog.h>

namespace Dia { namespace ApplicationFlow {

    //--------------------------------------------------------------------------
    // Constructor / Destructor
    //--------------------------------------------------------------------------

    Application::Application(const ApplicationManifestV2& manifest,
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
    }

    //--------------------------------------------------------------------------
    Application::~Application()
    {
        // If threads are still running, stop them now (safety net — callers
        // should call RequestShutdown() and drain Update() before destroying).
        if (mStarted && !mShuttingDown.load())
        {
            RequestShutdown();
        }

        // Destructor is the last chance to join — blocking here is the
        // expected behaviour (Update() wasn't drained).
        JoinDedicatedThreads();
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

        // --- 2. Build PUs and modules from manifest --------------------------
        if (!BuildFromManifest())
        {
            return false;
        }

        // --- 2b. Connect stream handles across all modules -------------------
        ConnectModuleStreams();

        // --- 3. Start dedicated threads (non-main PUs) -----------------------
        // PU index 0 is the main PU — it runs inline in Update().
        for (unsigned int p = 1; p < mProcessingUnitCount; ++p)
        {
            ProcessingUnit* pu = mProcessingUnits[p].Get();
            if (pu->IsDedicatedThread())
            {
                DIA_ASSERT(mDedicatedThreadCount < kMaxDedicatedThreads,
                           "Application::Start() — dedicated thread capacity exceeded");
                mDedicatedThreads[mDedicatedThreadCount] = std::thread(std::ref(*pu));
                ++mDedicatedThreadCount;
            }
        }

        mStarted = true;

        // --- 4. Enter initial stage ------------------------------------------
        if (mManifest.initialStage.Value() != 0)
        {
            DIA_LOG_INFO("Application", "Entering initial stage '%s'",
                         mManifest.initialStage.AsChar());

            mCurrentStage = mManifest.initialStage;

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
                BeginStopAllActive();
            }

            // Tick the main PU so stopping modules can wind down.
            if (mProcessingUnitCount > 0)
            {
                mProcessingUnits[0]->Update(deltaTime);
            }

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
        if (mProcessingUnitCount > 0)
        {
            mProcessingUnits[0]->Update(deltaTime);
        }

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
        std::lock_guard<std::mutex> lock(mTransitionMutex);
        mPendingStage = stageId;
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
        Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 16>& out) const
    {
        for (unsigned int i = 0; i < mManifest.stages.Size() && !out.IsFull(); ++i)
            out.Add(mManifest.stages[i].name);
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
                info.instanceId = puDecl.modules[m].instanceId;
                info.typeId     = puDecl.modules[m].typeId;
                info.state      = mod->GetState();
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
            si.type        = sd.type;
            si.fromPU      = sd.fromPU;
            si.toPU        = sd.toPU;
            si.multiWriter = sd.multiWriter;
            out.Add(si);
        }
    }

    //--------------------------------------------------------------------------
    // BuildFromManifest  (private)
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
                                   puDecl.dedicatedThread));

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
        // Consume the pending transition under the mutex.
        Dia::Core::StringCRC newStage;
        {
            std::lock_guard<std::mutex> lock(mTransitionMutex);
            if (!mHasPendingTransition.load())
                return;
            newStage = mPendingStage;
            mPendingStage = Dia::Core::StringCRC();
            mHasPendingTransition.store(false);
        }

        // No-op if already in that stage.
        if (mCurrentStage == newStage)
            return;

        DIA_LOG_INFO("Application", "Stage transition: '%s' -> '%s'",
                     mCurrentStage.AsChar(), newStage.AsChar());

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
                        module->BeginStart();
                }
            }
        }

        // Auto-advance: if newStage is an autoStage, queue the next stage.
        for (unsigned int i = 0; i < mManifest.autoStages.Size(); ++i)
        {
            if (mManifest.autoStages[i] == newStage)
            {
                for (unsigned int s = 0; s < mManifest.stages.Size(); ++s)
                {
                    if (mManifest.stages[s].name == newStage)
                    {
                        unsigned int nextIdx = s + 1;
                        if (nextIdx < mManifest.stages.Size())
                        {
                            DIA_LOG_INFO("Application", "Auto-advancing from stage '%s' to '%s'",
                                         newStage.AsChar(), mManifest.stages[nextIdx].name.AsChar());
                            TransitionTo(mManifest.stages[nextIdx].name);
                        }
                        break;
                    }
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
    // ConnectModuleStreams  (private)
    //--------------------------------------------------------------------------

    void Application::ConnectModuleStreams()
    {
        // Stream-store registration is only permitted inside this window.
        // See FindOrRegisterStreamStoreAtStartup for the rationale.
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
    // FindOrRegisterStreamStoreAtStartup  (public, main-thread, startup-only)
    //--------------------------------------------------------------------------

    IStreamStore* Application::FindOrRegisterStreamStoreAtStartup(Dia::Core::UniquePtr<IStreamStore> newStore)
    {
        DIA_ASSERT(mConnectingStreams,
                   "FindOrRegisterStreamStoreAtStartup called outside OnConnectStreams — "
                   "mStreamStores is only main-thread-safe during startup");

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
            return nullptr;
        }

        IStreamStore* raw = newStore.Get();
        mStreamStores[mStreamStoreCount] = std::move(newStore);
        ++mStreamStoreCount;
        return raw;
    }

}} // namespace Dia::ApplicationFlow
