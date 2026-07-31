#include "HTNPlan.h"

#include <DiaCore/Core/Assert.h>
#include <DiaObservation/Trace/DiaTrace.h>
#include <DiaObservation/Profile/DiaProfile.h>

#include <vector>

namespace Dia
{
    namespace HTN
    {
        struct HTNPlan::Impl
        {
            std::vector<HTNTask> tasks;

            // Minimal divergence snapshot: slot/field/value pairs tested by the plan's
            // method preconditions, captured at plan-build time (SD-005).
            struct SnapEntry
            {
                Dia::Core::StringCRC slot;
                Dia::Core::StringCRC field;
                bool  isBool;
                float floatValue;
                bool  boolValue;
                // The comparison operator and threshold that the precondition tested.
                // We store the original expected value so HasDiverged can re-test
                // "would this precondition still pass?"
                // For floats: diverged when |live - snapshot| crosses the threshold.
                // For bools:  diverged when live != snapshot.
                // Simple conservative policy: store the live value at plan time and
                // flag diverged if it has changed at all (bools) or changed sign (floats).
                //
                // This is intentionally minimal — callers can ignore HasDiverged() if
                // they prefer "plan until complete" semantics.
            };

            std::vector<SnapEntry> snapshot;
        };

        // -----------------------------------------------------------------------
        // Constructor / destructor / move
        // -----------------------------------------------------------------------
        HTNPlan::HTNPlan()
            : mImpl(new Impl())
            , mCursor(0)
        {
        }

        HTNPlan::~HTNPlan()
        {
            delete mImpl;
        }

        HTNPlan::HTNPlan(HTNPlan&& other) noexcept
            : mImpl(other.mImpl)
            , mCursor(other.mCursor)
        {
            other.mImpl   = nullptr;
            other.mCursor = 0;
        }

        HTNPlan& HTNPlan::operator=(HTNPlan&& other) noexcept
        {
            if (this != &other)
            {
                delete mImpl;
                mImpl         = other.mImpl;
                mCursor       = other.mCursor;
                other.mImpl   = nullptr;
                other.mCursor = 0;
            }
            return *this;
        }

        // -----------------------------------------------------------------------
        // Query
        // -----------------------------------------------------------------------
        bool HTNPlan::IsEmpty() const
        {
            if (!mImpl) return true;
            return mImpl->tasks.empty();
        }

        bool HTNPlan::IsComplete() const
        {
            if (!mImpl) return true;
            return mCursor >= static_cast<int>(mImpl->tasks.size());
        }

        const HTNTask& HTNPlan::CurrentTask() const
        {
            DIA_ASSERT(!IsComplete(), "HTNPlan::CurrentTask called on a completed or empty plan");
            return mImpl->tasks[static_cast<std::size_t>(mCursor)];
        }

        void HTNPlan::Advance()
        {
            if (!IsComplete())
                ++mCursor;
        }

        int HTNPlan::GetTaskCount() const
        {
            if (!mImpl) return 0;
            return static_cast<int>(mImpl->tasks.size());
        }

        // -----------------------------------------------------------------------
        // HasDiverged
        //
        // Re-evaluates the world-state snapshot taken at plan-build time.
        // For bools: diverged if the live value differs from the snapshot.
        // For floats: diverged if the live value differs from the snapshot
        //             (conservative: any change flags divergence).
        // -----------------------------------------------------------------------
        bool HTNPlan::HasDiverged(Dia::Condition::IConditionContext& ctx) const
        {
            DIA_TRACE_ZONE  ("htn.plan.has_diverged", ::Dia::Observation::Trace::Category::kNone);
            DIA_PROFILE_SCOPE("htn.plan.has_diverged", ::Dia::Observation::Profile::Category::kNone);

            if (!mImpl) return false;

            for (const Impl::SnapEntry& entry : mImpl->snapshot)
            {
                if (entry.isBool)
                {
                    if (ctx.GetBool(entry.slot, entry.field) != entry.boolValue)
                        return true;
                }
                else
                {
                    if (ctx.GetFloat(entry.slot, entry.field) != entry.floatValue)
                        return true;
                }
            }

            return false;
        }

        // -----------------------------------------------------------------------
        // Package-private helpers used by HTNPlanner to build the plan.
        // Declared as free functions in HTNPlanInternal.h.
        // -----------------------------------------------------------------------
        void HTNPlanAddTask(HTNPlan& plan, HTNTask&& task)
        {
            plan.mImpl->tasks.push_back(std::move(task));
        }

        void HTNPlanAddSnapFloat(HTNPlan& plan,
                                 Dia::Core::StringCRC slot,
                                 Dia::Core::StringCRC field,
                                 float value)
        {
            HTNPlan::Impl::SnapEntry entry;
            entry.slot       = slot;
            entry.field      = field;
            entry.isBool     = false;
            entry.floatValue = value;
            entry.boolValue  = false;
            plan.mImpl->snapshot.push_back(entry);
        }

        void HTNPlanAddSnapBool(HTNPlan& plan,
                                Dia::Core::StringCRC slot,
                                Dia::Core::StringCRC field,
                                bool value)
        {
            HTNPlan::Impl::SnapEntry entry;
            entry.slot       = slot;
            entry.field      = field;
            entry.isBool     = true;
            entry.floatValue = 0.0f;
            entry.boolValue  = value;
            plan.mImpl->snapshot.push_back(entry);
        }

    } // namespace HTN
} // namespace Dia
