////////////////////////////////////////////////////////////////////////////////
// Filename: AutomationService.h
// DiaAutomation — capability layer for external application driving
//
// Provides: checkpoint registry, navigation hold (transition guard),
// pause/resume callbacks, CI safety heartbeat, and dia.automation.* commands.
//
// Not a Module. Instantiated by AutomationModule (game-side).
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/IApplicationControl.h>
#include <DiaApplicationFlow/LifecycleEvent.h>
#include <DiaStreams/EventStreamStore.h>
#include <DiaStreams/IStreamStore.h>
#include <functional>

namespace Dia { namespace Automation {

    struct CheckpointResult
    {
        bool        passed      = false;
        const char* message     = "";
        float       durationMs  = 0.0f;
    };

    using CheckpointFn   = std::function<CheckpointResult()>;
    using PauseResumeFn  = std::function<void()>;

    class AutomationService
    {
    public:
        explicit AutomationService(Dia::ApplicationFlow::Application& app);
        ~AutomationService();

        AutomationService(const AutomationService&)            = delete;
        AutomationService& operator=(const AutomationService&) = delete;

        // --- Checkpoint Registry ---
        void             RegisterCheckpoint(Dia::ApplicationFlow::Module* owner,
                                            const Dia::Core::StringCRC& name,
                                            CheckpointFn fn);
        void             UnregisterCheckpoints(Dia::ApplicationFlow::Module* owner);
        CheckpointResult RunCheckpoint(const Dia::Core::StringCRC& name) const;
        bool             HasCheckpoint(const Dia::Core::StringCRC& name) const;

        // --- Pause/Resume Registry ---
        void RegisterPauseCallback(Dia::ApplicationFlow::Module* owner,
                                   PauseResumeFn pause,
                                   PauseResumeFn resume);
        void UnregisterPauseCallbacks(Dia::ApplicationFlow::Module* owner);
        void Pause();
        void Resume();
        bool IsPaused() const { return mPaused; }

        // --- Navigation Hold ---
        void EnableNavigationHold();
        void ReleaseNavigationHold(const Dia::Core::StringCRC& target,
                                   bool* outSuccess = nullptr,
                                   const char** outError = nullptr);
        bool IsHolding() const { return mHolding; }

        // --- CI Safety ---
        void EnableHeartbeatMonitor(float timeoutSeconds = 30.0f);
        void DisableHeartbeatMonitor();
        void ResetHeartbeat();
        void OnDisconnect();
        void TickHeartbeat(float deltaTime);
        bool IsHeartbeatEnabled() const { return mHeartbeatEnabled; }

        // --- Command Registration ---
        // Call once after Application::Start(). Registers dia.automation.* commands
        // with DiaAPI (JSON path). Each command wrapper calls ResetHeartbeat first.
        void RegisterCommands();

    private:
        struct CheckpointEntry
        {
            Dia::ApplicationFlow::Module* owner;
            Dia::Core::StringCRC          name;
            CheckpointFn                  fn;
        };

        struct PauseResumeEntry
        {
            Dia::ApplicationFlow::Module* owner;
            PauseResumeFn                 pause;
            PauseResumeFn                 resume;
        };

        Dia::ApplicationFlow::Application& mApp;

        Dia::Core::Containers::DynamicArrayC<CheckpointEntry,  64> mCheckpoints;
        Dia::Core::Containers::DynamicArrayC<PauseResumeEntry, 16> mPauseCallbacks;

        bool  mHolding          = false;
        bool  mHoldRegistered   = false;
        bool  mPaused           = false;
        bool  mHeartbeatEnabled = false;
        float mHeartbeatTimeout = 30.0f;
        float mHeartbeatElapsed = 0.0f;

        Dia::ApplicationFlow::TapHandle mLifecycleTap;
    };

}} // namespace Dia::Automation
