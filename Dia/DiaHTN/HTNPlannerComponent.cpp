#include <DiaHTN/HTNPlannerComponent.h>
#include <DiaEntity/ComponentMacros.h>
#include <DiaCore/Reflect/ReflectMacros.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Trace/DiaTrace.h>
#include <DiaObservation/Profile/DiaProfile.h>

DIA_SERIALIZE(Dia::HTN::HTNPlannerComponent, Dia::HTN::HTNPlannerComponent::kVersion)
DIA_SERIALIZE_END

namespace Dia
{
    namespace HTN
    {

DIA_COMPONENT_REGISTER(HTNPlannerComponent, "htn-planner-component", false, true,
    nullptr, 0,
    nullptr, 0,
    nullptr, 0)

HTNPlannerComponent::~HTNPlannerComponent()
{
    // If a work item is still pending, cancel it so the callback becomes a no-op.
    // The work item owns the AsyncContext heap allocation and will delete it on fire.
    if (mPendingContext)
    {
        mPendingContext->cancelled  = true;
        mPendingContext->component  = nullptr;
        mPendingContext             = nullptr;
    }
}

void HTNPlannerComponent::SetDomain(const HTNDomain* domain)
{
    mDomain = domain;
}

void HTNPlannerComponent::SetRegistry(const OperatorRegistry* registry)
{
    mRegistry = registry;
}

void HTNPlannerComponent::SetRootTask(Dia::Core::StringCRC taskId)
{
    mRootTask = taskId;
}

void HTNPlannerComponent::Replan(Dia::Condition::IConditionContext& ctx)
{
    DIA_TRACE_ZONE  ("htn.replan", ::Dia::Observation::Trace::Category::kNone);
    DIA_PROFILE_SCOPE("htn.replan", ::Dia::Observation::Profile::Category::kNone);

    if (!mDomain)
    {
        DIA_LOG_WARNING("HTN", "htn.component.replan: no domain set");
        return;
    }

    mActivePlan = mPlanner.Plan(mRootTask, *mDomain, ctx);
    mHasPlan    = !mActivePlan.IsEmpty();

    if (mHasPlan)
        DIA_LOG_DEBUG("HTN", "htn.component.replan: root=%u tasks=%d", mRootTask.Value(), mActivePlan.GetTaskCount());
    else
        DIA_LOG_WARNING("HTN", "htn.component.replan_failed: root=%u", mRootTask.Value());
}

void HTNPlannerComponent::ReplanAsync(Dia::Condition::IConditionContext& ctx,
                                       Dia::SimTime::SimTimeBudget& budget)
{
    if (!mDomain)
        return;

    // Allocate the new cancel token before submitting.
    // Only cancel the previous token if submission succeeds — if submission is ever
    // rejected (e.g. duplicate ID already queued), leave the old item alive.
    AsyncContext* newCtx = new AsyncContext();
    newCtx->component   = this;
    newCtx->cancelled   = false;

    const bool submitted = mPlanner.PlanAsync(mRootTask, *mDomain, ctx, budget, &OnAsyncPlanReady, newCtx);
    if (!submitted)
    {
        delete newCtx;
        return;
    }

    // Submission succeeded — cancel the previous pending token if one exists.
    if (mPendingContext)
    {
        mPendingContext->cancelled = true;
        mPendingContext->component = nullptr;
        // Work item still owns mPendingContext; it will delete it when the callback fires.
    }
    mPendingContext = newCtx;
}

TaskResult HTNPlannerComponent::Tick(void* operatorContext)
{
    DIA_TRACE_ZONE  ("htn.tick", ::Dia::Observation::Trace::Category::kNone);
    DIA_PROFILE_SCOPE("htn.tick", ::Dia::Observation::Profile::Category::kNone);

    if (!mHasPlan)
        return TaskResult::kSucceeded;

    if (!mRegistry)
    {
        DIA_LOG_ERROR("HTN", "htn.component.tick: no registry set");
        return TaskResult::kFailed;
    }

    if (mActivePlan.IsComplete())
        return TaskResult::kSucceeded;

    const HTNTask& task = mActivePlan.CurrentTask();
    OperatorBinding fn = mRegistry->Find(task.operatorId);

    if (!fn)
    {
        DIA_LOG_ERROR("HTN", "htn.component.tick: operator not found crc=%u", task.operatorId.Value());
        return TaskResult::kFailed;
    }

    const TaskResult result = fn(operatorContext, task.params);

    if (result == TaskResult::kSucceeded)
        mActivePlan.Advance();
    else if (result == TaskResult::kFailed)
        DIA_LOG_WARNING("HTN", "htn.component.tick: operator failed crc=%u", task.operatorId.Value());

    return result;
}

bool HTNPlannerComponent::HasDiverged(Dia::Condition::IConditionContext& ctx) const
{
    if (!mHasPlan)
        return false;
    return mActivePlan.HasDiverged(ctx);
}

const HTNPlan* HTNPlannerComponent::GetActivePlan() const
{
    return mHasPlan ? &mActivePlan : nullptr;
}

bool HTNPlannerComponent::HasActivePlan() const
{
    return mHasPlan;
}

// static
void HTNPlannerComponent::OnAsyncPlanReady(HTNPlan plan, void* userData)
{
    AsyncContext* ctx = static_cast<AsyncContext*>(userData);

    if (!ctx->cancelled && ctx->component)
    {
        HTNPlannerComponent* self = ctx->component;
        self->mActivePlan   = std::move(plan);
        self->mHasPlan      = !self->mActivePlan.IsEmpty();
        self->mPendingContext = nullptr; // work item consumed; clear the pointer

        if (self->mHasPlan)
            DIA_LOG_DEBUG("HTN", "htn.component.async_plan_ready: tasks=%d", self->mActivePlan.GetTaskCount());
        else
            DIA_LOG_WARNING("HTN", "htn.component.async_plan_ready: empty plan (planning failed)");
    }

    // Work item owns the context — delete it now that the callback has fired.
    delete ctx;
}

    } // namespace HTN
} // namespace Dia
