////////////////////////////////////////////////////////////////////////////////
// Filename: IApplicationControl.h
// DiaApplicationFlow — narrow control interface exposed to modules
//
// Modules need a small set of Application methods: signal a stage transition,
// request shutdown, and observe the current stage.  Exposing the full
// Application pointer would let modules reach stream-store registration,
// inspection data, and internals — none of which they should touch at runtime.
// This interface is the only thing Module::GetApplication() returns.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include <DiaCore/CRC/StringCRC.h>

namespace Dia { namespace ApplicationFlow {

    class IApplicationControl {
    public:
        virtual ~IApplicationControl() = default;

        // Queue a transition to the given stage.  Thread-safe.  Applied at the
        // top of the next Update().
        virtual void TransitionTo(const Dia::Core::StringCRC& stageId) = 0;

        // Request orderly shutdown of all dedicated threads and all modules.
        // Thread-safe.  Idempotent.
        virtual void RequestShutdown() = 0;

        // Read-only: which stage is the Application currently in.
        [[nodiscard]] virtual Dia::Core::StringCRC GetCurrentStage() const = 0;
    };

}} // namespace Dia::ApplicationFlow
