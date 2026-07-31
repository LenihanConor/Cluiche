#pragma once

#include <DiaHTN/HTNPlan.h>
#include <DiaHTN/HTNDomain.h>
#include <DiaCondition/IConditionContext.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaAIBudget/AIBudgetScheduler.h>

namespace Dia
{
    namespace HTN
    {
        using PlanResultCallback = void(*)(HTNPlan plan, void* userData);

        //-------------------------------------------------------------------------------------------
        // HTNPlanner
        //
        // Stateless depth-first forward-chaining planner (SD-001, SD-007).
        // Plan() decomposes the root task into a flat sequence of primitive operators.
        // PlanAsync() submits the work to AIBudgetScheduler; result delivered via callback (SD-008).
        //
        // SD-001: Stateless — one instance can plan for any number of entities.
        // SD-007: Ordered method selection — first method whose precondition passes wins.
        // SD-008: Async path submits to AIBudgetScheduler; no internal threads.
        // AD-003: Dia::HTN:: namespace.
        //-------------------------------------------------------------------------------------------
        class HTNPlanner
        {
        public:
            // Sync planning. Returns a valid plan, or an empty plan on failure.
            // Takes a snapshot of ctx at plan time for HasDiverged() checks.
            HTNPlan Plan(Dia::Core::StringCRC rootTask,
                         const HTNDomain& domain,
                         Dia::Condition::IConditionContext& ctx) const;

            // Async planning — submits work to AIBudgetScheduler.
            // Callback fires when the scheduler drains the work item.
            // Returns false if submission fails (scheduler at capacity).
            bool PlanAsync(Dia::Core::StringCRC rootTask,
                           const HTNDomain& domain,
                           Dia::Condition::IConditionContext& ctx,
                           Dia::AIBudget::AIBudgetScheduler& scheduler,
                           PlanResultCallback callback,
                           void* callbackUserData);
        };

    } // namespace HTN
} // namespace Dia
