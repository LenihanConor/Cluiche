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
namespace Dia { namespace Observation { namespace Profile { class TripReporter; struct TripConfig; struct PerfSummary; } } }

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

        // Queue a transition to the given stage with a launch preset.  An empty preset
        // is identical to TransitionTo(stageId).  If the preset is non-empty and not
        // declared for the stage, the request is refused (error logged, nothing queued).
        // Thread-safe.
        virtual void TransitionTo(const Dia::Core::StringCRC& stageId,
                                  const Dia::Core::StringCRC& presetId) = 0;

        // Request orderly shutdown of all dedicated threads and all modules.
        // Thread-safe.  Idempotent.
        virtual void RequestShutdown() = 0;

        // Read-only: which stage is the Application currently in.
        [[nodiscard]] virtual Dia::Core::StringCRC GetCurrentStage() const = 0;

        // Read-only: launch preset of the running stage (empty at boot or when the stage
        // was entered without a preset).  Same threading contract as GetCurrentStage.
        [[nodiscard]] virtual Dia::Core::StringCRC GetCurrentPreset() const = 0;

        // One-shot hand-off of the launch requested on the command line (--stage/--preset, see
        // Application::SetInitialLaunch).  Returns true and fills the out params at most once;
        // false when none was requested or it was already taken.  Thread-safe.
        virtual bool TakeInitialLaunch(Dia::Core::StringCRC& /*stageId*/, Dia::Core::StringCRC& /*presetId*/) { return false; }

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

        // Perf Inspector A3c: hand every ProcessingUnit the over-budget trip reporter (plain pointer,
        // not owned; nullptr detaches). Called by the module that owns the observation session.
        virtual void SetTripReporter(Dia::Observation::Profile::TripReporter* /*reporter*/) {}

        // Perf Inspector A4a: apply trip detection config to all ProcessingUnits.
        // Call before any unit starts ticking (typically from ObservationModule::DoStart).
        virtual void SetTripConfig(const Dia::Observation::Profile::TripConfig& /*config*/) {}

        // Perf Inspector A4b: append one PerfUnitSummary per ProcessingUnit (every module, including
        // modules that already stopped). Called once at shutdown from the thread that owns the
        // observation session; not for per-frame use. Default: no-op.
        virtual void CollectPerfSummary(Dia::Observation::Profile::PerfSummary& /*out*/) const {}
    };

}} // namespace Dia::ApplicationFlow
