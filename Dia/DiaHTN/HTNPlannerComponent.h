#pragma once

#include <DiaEntity/IComponent.h>
#include <DiaEntity/ComponentMacros.h>
#include <DiaHTN/HTNPlanner.h>
#include <DiaHTN/HTNDomain.h>
#include <DiaHTN/HTNPlan.h>
#include <DiaHTN/OperatorRegistry.h>
#include <DiaHTN/TaskResult.h>
#include <DiaCondition/IConditionContext.h>
#include <DiaSimTime/SimTimeBudget.h>

namespace Dia
{
    namespace HTN
    {
        //-------------------------------------------------------------------------------------------
        // HTNPlannerComponent
        //
        // IComponent wrapper attaching a domain + registry + active plan to a DiaEntity.
        // Caller drives Tick() each frame and calls HasDiverged() at their chosen frequency.
        //
        // SD-004: No automatic re-planning — caller decides when to re-plan on divergence/failure.
        // PD-001: kUniqueId uses StringCRC (DIA_COMPONENT macro).
        // AD-003: Dia::HTN:: namespace.
        // DIA_READONLY: Domain drives data; module calls Tick() explicitly.
        //-------------------------------------------------------------------------------------------
        class HTNPlannerComponent : public Dia::Entity::IComponent
        {
            DIA_COMPONENT(HTNPlannerComponent, "htn-planner-component", 1)
            DIA_READONLY

        public:
            HTNPlannerComponent() = default;
            ~HTNPlannerComponent();

            // Shared domain — must outlive the component. Caller retains ownership.
            void SetDomain(const HTNDomain* domain);

            // Operator registry — must outlive the component. Caller retains ownership.
            void SetRegistry(const OperatorRegistry* registry);

            void SetRootTask(Dia::Core::StringCRC taskId);

            // Re-plan synchronously; replaces active plan.
            void Replan(Dia::Condition::IConditionContext& ctx);

            // Re-plan asynchronously; active plan continues until callback fires.
            // Safe if the component is destroyed before budget drains the one-shot: the
            // pending context is cancelled and the callback becomes a no-op.
            void ReplanAsync(Dia::Condition::IConditionContext& ctx,
                             Dia::SimTime::SimTimeBudget& budget);

            // Tick the active plan: calls current operator, advances on kSucceeded.
            // Returns kFailed if the operator fails. Returns kSucceeded if plan is
            // complete or not set (no-op).
            TaskResult Tick(void* operatorContext);

            bool HasDiverged(Dia::Condition::IConditionContext& ctx) const;

            const HTNPlan* GetActivePlan() const;
            bool HasActivePlan() const;

        private:
            // Cancel token: heap-allocated, owned jointly by the component and the work item.
            // Component destructor marks it cancelled so the callback becomes a no-op.
            struct AsyncContext
            {
                HTNPlannerComponent* component = nullptr;
                bool cancelled = false;
            };

            static void OnAsyncPlanReady(HTNPlan plan, void* userData);

            const HTNDomain*      mDomain         = nullptr;
            const OperatorRegistry* mRegistry     = nullptr;
            Dia::Core::StringCRC  mRootTask;
            HTNPlanner            mPlanner;
            HTNPlan               mActivePlan;
            bool                  mHasPlan        = false;
            AsyncContext*         mPendingContext  = nullptr; // owned by the live work item
        };

    } // namespace HTN
} // namespace Dia
