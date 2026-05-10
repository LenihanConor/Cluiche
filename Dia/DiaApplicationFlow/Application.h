////////////////////////////////////////////////////////////////////////////////
// Filename: Application.h
// DiaApplicationFlow — v2 Application
//
// Top-level orchestrator.  Takes a validated ApplicationManifestV2 and a
// TypeRegistry, creates ProcessingUnits and Modules, manages stage transitions
// and shutdown.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include <DiaApplicationFlow/Manifest/ApplicationManifestV2.h>
#include <DiaApplicationFlow/TypeRegistry.h>
#include <DiaApplicationFlow/ProcessingUnit.h>
#include <DiaApplicationFlow/Streams/IStreamStore.h>
#include <DiaApplicationFlow/IApplicationInspectable.h>
#include <DiaApplicationFlow/IApplicationControl.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Memory/UniquePtr.h>
#include <atomic>
#include <mutex>
#include <thread>

namespace Dia { namespace ApplicationFlow {

    // ---------------------------------------------------------------------------
    // Application
    //
    // Lifecycle:
    //   Application app(manifest, registry);
    //   if (!app.Start()) { /* handle validation / creation errors */ }
    //   while (app.Update(dt)) {}
    //
    // Thread safety:
    //   TransitionTo() and RequestShutdown() are safe to call from any thread.
    //   Update() must be called from the main thread.
    // ---------------------------------------------------------------------------
    class Application : public IApplicationInspectable
                      , public IApplicationControl
    {
    public:
        Application(const ApplicationManifestV2& manifest,
                    TypeRegistry& registry);
        ~Application();

        Application(const Application&) = delete;
        Application& operator=(const Application&) = delete;

        // Validates manifest, creates PUs / modules, starts dedicated threads,
        // enters the initial stage.  Returns false on any validation or creation
        // failure.
        bool Start();

        // Tick the main (first) PU inline.  Applies any pending stage transition
        // at the top of the frame.  Returns false once all modules are inactive
        // (i.e. the application has fully shut down).
        bool Update(float deltaTime);

        // IApplicationControl — thread-safe; queues a stage transition to
        // execute at the top of the next Update() call.
        void TransitionTo(const Dia::Core::StringCRC& stageId) override;

        // IApplicationControl — thread-safe; stops all dedicated threads and
        // begins stopping every active module.
        void RequestShutdown() override;

        // IApplicationInspectable — returns self (Application implements the interface directly).
        IApplicationInspectable* GetInspectable() { return this; }

        // IApplicationInspectable overrides
        [[nodiscard]] Dia::Core::StringCRC GetCurrentStage() const override;
        [[nodiscard]] bool IsTransitioning() const override;
        [[nodiscard]] TransitionInfo GetTransitionInfo() const override;
        void GetAllStages(Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 16>& out) const override;
        void GetProcessingUnits(Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 4>& out) const override;
        void GetActiveModules(const Dia::Core::StringCRC& puId,
                              Dia::Core::Containers::DynamicArrayC<ModuleStateInfo, 64>& out) const override;
        void GetStreamInfo(Dia::Core::Containers::DynamicArrayC<StreamInfo, 16>& out) const override;
        [[nodiscard]] bool IsShuttingDown() const override;

        // Stream store registry — called from module's OnConnectStreams override.
        //
        // FindOrRegisterStreamStore: takes ownership of newStore. If a store with
        // the same ID already exists, returns the existing one (first-registrant
        // wins) and the passed-in store is discarded. Always returns non-null on
        // success; returns null only if capacity is exceeded (asserts in debug).
        IStreamStore* FindOrRegisterStreamStore(Dia::Core::UniquePtr<IStreamStore> newStore);

        // FindStreamStore: lookup only, no creation. Returns null if not found.
        IStreamStore* FindStreamStore(const Dia::Core::StringCRC& id) const;

    private:
        // Maximum number of ProcessingUnits supported (matches spec).
        static constexpr unsigned int kMaxProcessingUnits = 4;

        // Maximum number of stream stores (frame + event combined).
        static constexpr unsigned int kMaxStreams = 16;

        // --- Helpers -----------------------------------------------------------

        // Build PUs and modules from the manifest using the type registry.
        // Returns false on error.
        bool BuildFromManifest();

        // Walk every module in every PU and call OnConnectStreams(*this).
        // Called once in Start(), after BuildFromManifest() and before launching
        // dedicated threads.
        void ConnectModuleStreams();

        // Apply the pending stage transition (called at the top of Update).
        // Only kicks off the stop phase; sets mTransitionDraining = true.
        void ApplyPendingTransition();

        // Continue a draining transition: once all outgoing modules are kInactive,
        // commits the new stage and starts incoming modules.
        void TickTransitionDrain();

        // Begin stopping all active modules across all PUs (shutdown path).
        void BeginStopAllActive();

        // Returns true if every module in every PU is kInactive or kFailed.
        bool AllModulesInactive() const;

        // Returns true if any module is in kFailed state.
        bool AnyModuleFailed() const;

        // "In stage" predicate: module is in stage S if its stages array
        // contains S or contains StringCRC("all").
        static bool ModuleIsInStage(const ModuleDeclaration& decl,
                                    const Dia::Core::StringCRC& stage);

        // --- State -------------------------------------------------------------
        const ApplicationManifestV2& mManifest;
        TypeRegistry&                mRegistry;

        // Fixed-capacity PU array (no heap allocation for PU storage).
        Dia::Core::UniquePtr<ProcessingUnit> mProcessingUnits[kMaxProcessingUnits];
        unsigned int                         mProcessingUnitCount = 0;

        // Type-erased stream store registry — owned by Application.
        // Populated during ConnectModuleStreams() (called from Start()).
        Dia::Core::UniquePtr<IStreamStore> mStreamStores[kMaxStreams];
        unsigned int                       mStreamStoreCount = 0;

        // One std::thread per dedicated PU (index 0 = main PU, no thread).
        // Max 3 dedicated threads (kMaxProcessingUnits - 1).
        std::thread  mDedicatedThreads[kMaxProcessingUnits - 1];
        unsigned int mDedicatedThreadCount = 0;

        Dia::Core::StringCRC mCurrentStage;

        // Pending stage transition — written by TransitionTo(), read by Update().
        mutable std::mutex   mTransitionMutex;
        Dia::Core::StringCRC mPendingStage;
        std::atomic<bool>    mHasPendingTransition{false};

        std::atomic<bool>    mShuttingDown{false};
        bool                 mStarted = false;
        bool                 mShutdownInitiated = false;  // tracks whether BeginStopAllActive has been called

        // Transition drain state: set when outgoing modules are being stopped before
        // incoming modules can be started. Cleared once all outgoing are kInactive.
        bool                 mTransitionDraining = false;
        Dia::Core::StringCRC mDrainingToStage;

        // Rollback retry state: on a non-boot module failure the release-build
        // policy is to stop and restart the current stage.  If the failure is
        // deterministic we'd otherwise loop forever — cap it and shut down.
        static constexpr unsigned int kMaxRollbackAttempts = 3;
        Dia::Core::StringCRC mLastRollbackStage;     // which stage we last rolled back in
        unsigned int         mRollbackAttempts = 0;  // consecutive attempts in that stage
    };

}} // namespace Dia::ApplicationFlow
