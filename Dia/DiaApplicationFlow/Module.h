#pragma once
#include <DiaCore/CRC/StringCRC.h>
#include <atomic>

namespace Dia { namespace ApplicationFlow {

    class ProcessingUnit;
    class Application;

    enum class StartResult { kReady, kLoading, kFailed };
    enum class StopResult  { kDone, kStopping };
    enum class ModuleState { kInactive, kStarting, kActive, kStopping, kFailed };

    class Module
    {
    public:
        explicit Module(const Dia::Core::StringCRC& instanceId);
        virtual ~Module() = default;

        Module(const Module&) = delete;
        Module& operator=(const Module&) = delete;

        [[nodiscard]] const Dia::Core::StringCRC& GetInstanceId() const;
        [[nodiscard]] ProcessingUnit* GetProcessingUnit() const;
        [[nodiscard]] ModuleState GetState() const;

        void TransitionTo(const Dia::Core::StringCRC& stageId);

    protected:
        virtual StartResult DoStart() = 0;
        virtual void        DoUpdate(float deltaTime) = 0;
        virtual StopResult  DoStop() = 0;

        // Called by Application after all PUs and modules are built, before
        // the initial stage is entered.  Override to call Connect() on any
        // StreamWriter / StreamReader / EventStreamWriter / EventStreamReader
        // handles declared by this module.
        virtual void OnConnectStreams(Application& /*app*/) {}

    private:
        friend class ProcessingUnit;
        friend class Application;

        void SetProcessingUnit(ProcessingUnit* pu);

        // FrameTick: called by ProcessingUnit each frame — drives the state machine.
        // deltaTime: wall-clock delta (seconds).
        // startTimeoutMs / stopTimeoutMs: from manifest config, passed by ProcessingUnit.
        void FrameTick(float deltaTime, float startTimeoutMs, float stopTimeoutMs);
        void BeginStart();
        void BeginStop();

        Dia::Core::StringCRC     mInstanceId;
        ProcessingUnit*          mProcessingUnit  = nullptr;
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
