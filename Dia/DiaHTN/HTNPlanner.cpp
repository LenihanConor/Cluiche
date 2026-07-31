#include "HTNPlanner.h"
#include "HTNPlan.h"

#include <DiaAIBudget/IAIBudgetedSystem.h>
#include <DiaCore/Core/Assert.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Trace/DiaTrace.h>
#include <DiaObservation/Profile/DiaProfile.h>

#include <vector>

namespace Dia
{
    namespace HTN
    {
        // -----------------------------------------------------------------------
        // RecordingContext
        //
        // Wraps an IConditionContext and records every (slot, field, value) pair
        // queried during precondition evaluation for the divergence snapshot.
        // -----------------------------------------------------------------------
        class RecordingContext : public Dia::Condition::IConditionContext
        {
        public:
            explicit RecordingContext(Dia::Condition::IConditionContext& inner)
                : mInner(inner) {}

            float GetFloat(Dia::Core::StringCRC slot, Dia::Core::StringCRC field) const override
            {
                const float v = mInner.GetFloat(slot, field);
                const unsigned long long key = makeKey(slot, field);
                for (const FloatEntry& e : mFloats)
                    if (e.key == key) return v;
                mFloats.push_back({key, slot, field, v});
                return v;
            }

            bool GetBool(Dia::Core::StringCRC slot, Dia::Core::StringCRC field) const override
            {
                const bool v = mInner.GetBool(slot, field);
                const unsigned long long key = makeKey(slot, field);
                for (const BoolEntry& e : mBools)
                    if (e.key == key) return v;
                mBools.push_back({key, slot, field, v});
                return v;
            }

            struct FloatEntry { unsigned long long key; Dia::Core::StringCRC slot, field; float value; };
            struct BoolEntry  { unsigned long long key; Dia::Core::StringCRC slot, field; bool  value; };

            const std::vector<FloatEntry>& GetFloats() const { return mFloats; }
            const std::vector<BoolEntry>&  GetBools()  const { return mBools;  }

        private:
            static unsigned long long makeKey(Dia::Core::StringCRC slot, Dia::Core::StringCRC field)
            {
                return (static_cast<unsigned long long>(slot.Value()) << 32)
                    | static_cast<unsigned long long>(field.Value());
            }

            Dia::Condition::IConditionContext& mInner;
            mutable std::vector<FloatEntry> mFloats;
            mutable std::vector<BoolEntry>  mBools;
        };

        // -----------------------------------------------------------------------
        // Depth-first forward-chaining decomposition (SD-001, SD-007).
        //
        // For compound tasks: iterate methods in order. For each method whose
        // precondition passes, try to decompose all sub-tasks. If any sub-task
        // fails, backtrack and try the next method.
        //
        // For primitive tasks: emit an HTNTask directly.
        // -----------------------------------------------------------------------
        static bool DecomposeTask(
            Dia::Core::StringCRC taskId,
            const HTNDomain& domain,
            RecordingContext& ctx,
            std::vector<HTNTask>& outTasks,
            std::vector<unsigned int>& visitStack)
        {
            if (domain.IsPrimitive(taskId))
            {
                PrimitiveInfo info = domain.GetPrimitiveInfo(taskId);
                if (!info.valid) return false;

                HTNTask task;
                task.operatorId = info.operatorId;
                for (unsigned int i = 0; i < info.params.Size(); ++i)
                    if (!task.params.IsFull()) task.params.Add(info.params[i]);

                outTasks.push_back(std::move(task));
                return true;
            }

            if (domain.IsCompound(taskId))
            {
                // Cycle guard: if we're already expanding this task, it's a cycle → fail.
                for (unsigned int id : visitStack)
                    if (id == taskId.Value()) return false;

                visitStack.push_back(taskId.Value());

                const int methodCount = domain.GetMethodCount(taskId);
                for (int m = 0; m < methodCount; ++m)
                {
                    if (!domain.EvalMethodPrecondition(taskId, m, ctx))
                        continue;

                    Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 32> subtasks;
                    domain.GetMethodSubtasks(taskId, m, subtasks);

                    // Try to decompose the full sub-task list into a branch.
                    std::vector<HTNTask> branch;
                    bool branchOk = true;
                    for (unsigned int s = 0; s < subtasks.Size(); ++s)
                    {
                        if (!DecomposeTask(subtasks[s], domain, ctx, branch, visitStack))
                        {
                            branchOk = false;
                            break;
                        }
                    }

                    if (branchOk)
                    {
                        for (HTNTask& t : branch)
                            outTasks.push_back(std::move(t));
                        visitStack.pop_back();
                        return true;
                    }
                    // Branch failed — try next method
                }

                visitStack.pop_back();
                return false; // no method succeeded
            }

            return false; // unknown task
        }

        // -----------------------------------------------------------------------
        // HTNPlanner::Plan
        // -----------------------------------------------------------------------
        HTNPlan HTNPlanner::Plan(Dia::Core::StringCRC rootTask,
                                 const HTNDomain& domain,
                                 Dia::Condition::IConditionContext& ctx) const
        {
            DIA_TRACE_ZONE  ("htn.plan", ::Dia::Observation::Trace::Category::kNone);
            DIA_PROFILE_SCOPE("htn.plan", ::Dia::Observation::Profile::Category::kNone);

            HTNPlan plan;

            if (!domain.IsValid())
            {
                DIA_LOG_WARNING("HTN", "htn.plan: called with invalid domain, root=%u", rootTask.Value());
                return plan;
            }

            RecordingContext recording(ctx);
            std::vector<HTNTask> tasks;
            std::vector<unsigned int> visitStack;

            if (!DecomposeTask(rootTask, domain, recording, tasks, visitStack))
            {
                DIA_LOG_DEBUG("HTN", "htn.plan.failed: no decomposition for root=%u", rootTask.Value());
                return plan; // empty plan = failure
            }

            for (HTNTask& t : tasks)
                HTNPlanAddTask(plan, std::move(t));

            for (const auto& f : recording.GetFloats())
                HTNPlanAddSnapFloat(plan, f.slot, f.field, f.value);
            for (const auto& b : recording.GetBools())
                HTNPlanAddSnapBool(plan, b.slot, b.field, b.value);

            DIA_LOG_DEBUG("HTN", "htn.plan.built: root=%u tasks=%d", rootTask.Value(), plan.GetTaskCount());
            return plan;
        }

        // -----------------------------------------------------------------------
        // Async work item
        //
        // Heap-allocated on submission, self-deletes after the callback fires.
        // No static storage — SD-009 (no hidden global state).
        // -----------------------------------------------------------------------
        struct HTNPlanWorkItem : public Dia::AIBudget::IAIBudgetedSystem
        {
            Dia::Core::StringCRC GetSystemId() const override
            {
                return Dia::Core::StringCRC("HTNPlanWorkItem");
            }

            Dia::Core::StringCRC rootTask;
            const HTNDomain*     domain           = nullptr;
            Dia::Condition::IConditionContext* ctx = nullptr;
            PlanResultCallback   callback          = nullptr;
            void*                callbackUserData  = nullptr;
            Dia::AIBudget::AIBudgetScheduler* scheduler = nullptr;

            void UpdateBudgeted(float /*budgetMs*/) override
            {
                HTNPlanner planner;
                HTNPlan plan = planner.Plan(rootTask, *domain, *ctx);

                if (scheduler)
                    scheduler->Unregister(this);

                if (callback)
                    callback(std::move(plan), callbackUserData);

                // Self-delete: no global list owns us.
                delete this;
            }
        };

        // -----------------------------------------------------------------------
        // HTNPlanner::PlanAsync
        // -----------------------------------------------------------------------
        bool HTNPlanner::PlanAsync(Dia::Core::StringCRC rootTask,
                                   const HTNDomain& domain,
                                   Dia::Condition::IConditionContext& ctx,
                                   Dia::AIBudget::AIBudgetScheduler& scheduler,
                                   PlanResultCallback callback,
                                   void* callbackUserData)
        {
            HTNPlanWorkItem* item = new HTNPlanWorkItem();
            item->rootTask         = rootTask;
            item->domain           = &domain;
            item->ctx              = &ctx;
            item->callback         = callback;
            item->callbackUserData = callbackUserData;
            item->scheduler        = &scheduler;

            const bool submitted = scheduler.Register(item);
            if (!submitted)
            {
                DIA_LOG_WARNING("HTN", "htn.plan_async.submit_failed: scheduler at capacity, root=%u", rootTask.Value());
                delete item;
            }
            else
            {
                DIA_LOG_DEBUG("HTN", "htn.plan_async.submitted: root=%u", rootTask.Value());
            }
            return submitted;
        }

    } // namespace HTN
} // namespace Dia
