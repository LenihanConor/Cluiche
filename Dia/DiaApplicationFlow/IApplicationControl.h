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
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <functional>

namespace Json { class Value; }

namespace Dia { namespace ApplicationFlow {

    class Module;

    enum class GuardResult { Allow, Hold };

    // Guard callback — idempotent, side-effect-free, called from main thread.
    // Returns Hold to keep the pending transition waiting; Allow to let it proceed.
    using TransitionGuardFn = std::function<GuardResult()>;

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

        // Read-only: what stages can the given stage transition to.
        virtual void GetStageTransitions(const Dia::Core::StringCRC& stage,
            Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 32>& out) const = 0;

        // Register a transition guard tied to `owner`.  The guard is called each
        // frame while a transition is pending.  Returns false if registry is full
        // (kMaxGuards exceeded — asserts in debug).
        // Main-thread-only.  Call from DoStart; unregister from DoStop or destructor.
        virtual bool RegisterTransitionGuard(Module* owner, TransitionGuardFn fn) = 0;

        // Remove all guards registered against `owner`.  Idempotent.
        // Main-thread-only.
        virtual void UnregisterTransitionGuards(Module* owner) = 0;

        // Read-only: returns the "config" block from the .diagame file.
        // Null if no .diagame config was present. Safe from any lifecycle method.
        [[nodiscard]] virtual const Json::Value* GetDiagameConfig() const = 0;
    };

}} // namespace Dia::ApplicationFlow
