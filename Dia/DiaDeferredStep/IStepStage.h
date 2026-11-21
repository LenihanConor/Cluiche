// Step chain stage contract.
// Spec: docs/specs/applications/dia/systems/diadeferredstep/step-chain.md
#pragma once

#include <DiaDeferredStep/DeferredStepTypes.h>
#include <DiaDeferredStep/IParallelExecutor.h>

namespace Dia::SaveGame
{
    class SaveContext;
    class LoadContext;
}

namespace Dia::DeferredStep
{
    // Threading contract:
    //  - IsReady, Capture, Commit, SerializePending, DeserializePending and ValidatePending are
    //    called on SimPU only.
    //  - Run is called on ONE worker thread, never concurrently with Capture/Commit, and may call
    //    ParallelFor on the executor repeatedly. Run reads the captured working copy only.
    //  - A stage's own readiness flag must not be written during Run.
    //  - A failing stage reports through its own health and returns false from IsReady().
    class IStepStage
    {
    public:
        virtual ~IStepStage() = default;

        virtual bool IsReady() const = 0;                                      // initialised and spun up; SimPU
        virtual void Capture(StepKey key) = 0;                                 // SimPU
        virtual void Run(IParallelExecutor& executor) = 0;                     // worker thread(s); working copy only
        virtual void Commit() = 0;                                             // SimPU
        virtual void SerializePending(Dia::SaveGame::SaveContext& ctx) const = 0;
        virtual void DeserializePending(Dia::SaveGame::LoadContext& ctx) = 0;
        virtual bool ValidatePending(Dia::SaveGame::LoadContext& ctx) const = 0;   // read-only pre-check of the pending blob
    };
}
