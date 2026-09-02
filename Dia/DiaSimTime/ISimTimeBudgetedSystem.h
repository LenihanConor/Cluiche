#pragma once
#include <DiaAIBudget/IAIBudgetedSystem.h>
#include <DiaSimTime/SimTimePriority.h>

namespace Dia::SimTime {

    // Replaces IAIBudgetedSystem as the interface systems implement to receive a
    // CPU time budget slice under DiaSimTime. Inherits (rather than aliases)
    // Dia::AIBudget::IAIBudgetedSystem so that:
    //   - Dia::AIBudget::IAIBudgetedSystem and Dia/DiaAIBudget/ remain untouched.
    //   - The module dependency stays one-way (DiaSimTime -> DiaAIBudget), matching
    //     what AIBudgetScheduler extension (a later task) needs anyway.
    //   - An ISimTimeBudgetedSystem* can be passed directly (upcast) to the existing
    //     AIBudgetScheduler::Register(IAIBudgetedSystem*) with zero adapter code.
    //
    // GetSystemId() and UpdateBudgeted(float) are inherited pure-virtual from
    // IAIBudgetedSystem and must still be implemented by concrete subclasses.
    // UpdateBudgeted's contract (AB-005: budgetMs may be 0.0f, must be handled as a
    // no-op) is preserved unchanged from the base interface.
    class ISimTimeBudgetedSystem : public Dia::AIBudget::IAIBudgetedSystem
    {
    public:
        virtual SimTimePriority GetPriority() const { return SimTimePriority::kNormal; }
    };

}
