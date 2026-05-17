#pragma once
#include <DiaCore/CRC/StringCRC.h>
#include <atomic>

namespace Dia { namespace ApplicationFlow {

    class ProcessingUnit;
    class Application;
    class IApplicationControl;

    enum class StartResult { kReady, kLoading, kFailed };
    enum class StopResult  { kDone, kStopping };
    enum class ModuleState { kInactive, kStarting, kActive, kStopping, kFailed };

    // Lifecycle resource rule
    // ------------------------
    // All thread-affined or externally-owned resources (GPU handles, audio
    // voices, file handles, sockets, registrations on shared services) MUST be
    // acquired in DoStart and released in DoStop. Do NOT hold them across the
    // module destructor.
    //
    // Why: by the time ~Module runs the owning PU thread may already have
    // exited, sibling modules that owned shared services (e.g. KernelModule's
    // RenderWindow + GL context) may be destroyed, and there is no guarantee
    // the framework can route work back to the correct thread. DoStop runs
    // on the PU's own thread while sibling-PU modules are still settling —
    // it is the only safe place to release thread-affined state.
    //
    // The destructor should only release plain CPU-side memory owned by the
    // module's own members (which UniquePtr / DynamicArrayC handle for free).
    class Module
    {
    public:
        explicit Module(const Dia::Core::StringCRC& instanceId);
        virtual ~Module() = default;

        Module(const Module&) = delete;
        Module& operator=(const Module&) = delete;

        [[nodiscard]] const Dia::Core::StringCRC& GetInstanceId() const;

        // The PU is a scheduler, not a directory.  The only mutating API it
        // exposes is AddModule, which is private to Application.  Cross-PU
        // lookups are not provided by design.
        [[nodiscard]] ProcessingUnit* GetProcessingUnit() const;

        [[nodiscard]] ModuleState GetState() const;

        // Narrow control handle: TransitionTo, RequestShutdown, GetCurrentStage.
        // Modules cannot reach Application internals (stream registration,
        // introspection) through this interface.
        [[nodiscard]] IApplicationControl* GetApplication() const;

        void TransitionTo(const Dia::Core::StringCRC& stageId);

    protected:
        virtual StartResult DoStart() = 0;
        virtual void        DoUpdate(float deltaTime) = 0;
        virtual StopResult  DoStop() = 0;

        // Called by Application after the module is created but before any
        // dedicated threads start, once per module.  Default no-op.  Receives
        // the module's `config` JSON string from its manifest declaration
        // (ModuleDeclaration::configJson).  Empty string if the manifest has
        // no config for this module.  Runs on the main thread.
        virtual void OnConfigure(const char* /*configJson*/) {}

        // Called by Application after all PUs and modules are built, before
        // the initial stage is entered.  Override to call Connect() on any
        // StreamWriter / StreamReader / EventStreamWriter / EventStreamReader
        // handles declared by this module.  Receives the full Application& so
        // stream-store registration can take ownership of newly-created stores.
        virtual void OnConnectStreams(Application& /*app*/) {}

    private:
        friend class ProcessingUnit;
        friend class Application;

        void SetProcessingUnit(ProcessingUnit* pu);
        void SetApplication(Application* app);

        // FrameTick: called by ProcessingUnit each frame — drives the state machine.
        // deltaTime: wall-clock delta (seconds).
        // startTimeoutMs / stopTimeoutMs: from manifest config, passed by ProcessingUnit.
        void FrameTick(float deltaTime, float startTimeoutMs, float stopTimeoutMs);
        void BeginStart();
        void BeginStop();

        Dia::Core::StringCRC     mInstanceId;
        ProcessingUnit*          mProcessingUnit  = nullptr;
        // Stored as concrete Application* for framework-internal use (e.g.
        // OnConnectStreams needs the full Application&).  Exposed to module
        // code only through GetApplication(), which returns the narrow
        // IApplicationControl* view.
        Application*             mApplication     = nullptr;
        // mState is accessed across threads: written on the PU's own thread
        // during FrameTick(), and written on the main thread by BeginStart() /
        // BeginStop().  It is also read on the main thread by Application's
        // bookkeeping (AllModulesInactive, AnyModuleFailed, introspection).
        // Atomic + acq/rel avoids a formal data race on the enum.
        std::atomic<ModuleState> mState{ModuleState::kInactive};
        float                    mStateElapsedMs  = 0.0f;
        bool                     mStartLogged     = false;
        bool                     mStopLogged      = false;
    };

}} // namespace Dia::ApplicationFlow
