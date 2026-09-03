#pragma once

#include <DiaHTN/HTNPlan.h>
#include <DiaHTN/HTNDomain.h>
#include <DiaCondition/IConditionContext.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaSimTime/SimTimeBudget.h>

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
        // PlanAsync() submits the work to SimTimeBudget's one-shot queue (ST-012);
        // result delivered via callback (SD-008).
        //
        // SD-001: Stateless — one instance can plan for any number of entities.
        // SD-007: Ordered method selection — first method whose precondition passes wins.
        // SD-008: Async path submits to SimTimeBudget; no internal threads.
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

            // Async planning — submits work to SimTimeBudget's one-shot queue.
            // Callback fires when SimTimeBudget::RunOneShots() drains the work item.
            // Always returns true (SimTimeBudget::SubmitOneShot asserts rather than
            // gracefully failing on overflow; its capacity is generous enough that
            // this is not a real runtime condition to check).
            bool PlanAsync(Dia::Core::StringCRC rootTask,
                           const HTNDomain& domain,
                           Dia::Condition::IConditionContext& ctx,
                           Dia::SimTime::SimTimeBudget& budget,
                           PlanResultCallback callback,
                           void* callbackUserData);
        };

    } // namespace HTN
} // namespace Dia
