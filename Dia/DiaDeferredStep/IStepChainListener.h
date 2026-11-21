// Step chain signal: raised once per applied step, after every stage has committed.
// Spec: docs/specs/applications/dia/systems/diadeferredstep/step-chain.md
#pragma once

#include <DiaDeferredStep/DeferredStepTypes.h>

#include <cstdint>

namespace Dia::DeferredStep
{
    struct StepChainApplied
    {
        StepKey  applyKey   = 0;   // the key at which the result became visible
        StepKey  stepKey    = 0;   // the key the step stands for (what the stages computed)
        uint32_t stageCount = 0;   // enabled stages that committed
    };

    // A listener must not call back into its owner from OnStepChainApplied (no register/unregister of
    // listeners or stages, no kick): debug assert, release no-op returning false.
    class IStepChainListener
    {
    public:
        virtual ~IStepChainListener() = default;
        virtual void OnStepChainApplied(const StepChainApplied& applied) = 0;   // SimPU, synchronous
    };
}
