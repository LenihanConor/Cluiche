// World Step history signal: raised once per applied step, after every stage has committed.
// Spec: docs/specs/applications/cow/systems/worldstep/save-and-history.md
#pragma once

#include <DiaDeferredStep/DeferredStepTypes.h>

#include <cstdint>

namespace CoW::WorldStep
{
    using Dia::DeferredStep::StepKey;

    struct WorldStepApplied
    {
        StepKey  applyKey   = 0;   // the day the result became visible
        StepKey  stepDay    = 0;   // the day the step stands for (what the stages computed)
        uint32_t stageCount = 0;   // enabled stages that committed
    };

    // A listener must not call back into the module from OnWorldStepApplied (no RegisterListener,
    // UnregisterListener, RegisterStage, UnregisterStage or kick): debug assert, release no-op returning false.
    class IWorldStepListener
    {
    public:
        virtual ~IWorldStepListener() = default;
        virtual void OnWorldStepApplied(const WorldStepApplied& applied) = 0;   // SimPU, synchronous
    };
}
