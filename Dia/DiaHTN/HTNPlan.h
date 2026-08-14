#pragma once

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCondition/IConditionContext.h>

namespace Dia
{
    namespace HTN
    {
        //-------------------------------------------------------------------------------------------
        // HTNTask
        //-------------------------------------------------------------------------------------------
        struct HTNTask
        {
            Dia::Core::StringCRC operatorId;
            Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 8> params;
        };

        //-------------------------------------------------------------------------------------------
        // HTNPlan
        //
        // SD-004: Re-planning always caller-triggered.
        // SD-005: HasDiverged() re-evaluates snapshot taken at plan-build time.
        // PD-004: No STL in public API.
        // AD-003: Dia::HTN:: namespace.
        //-------------------------------------------------------------------------------------------
        class HTNPlan
        {
        public:
            HTNPlan();
            ~HTNPlan();

            HTNPlan(HTNPlan&&) noexcept;
            HTNPlan& operator=(HTNPlan&&) noexcept;

            HTNPlan(const HTNPlan&) = delete;
            HTNPlan& operator=(const HTNPlan&) = delete;

            bool IsEmpty() const;
            bool IsComplete() const;

            const HTNTask& CurrentTask() const;
            void Advance();

            bool HasDiverged(Dia::Condition::IConditionContext& ctx) const;

            int GetTaskCount() const;
            int GetCurrentTaskIndex() const;

        private:
            // Internal mutation helpers called by HTNPlanner (defined in HTNPlan.cpp,
            // granted access via the friend declarations below).
            friend void HTNPlanAddTask(HTNPlan& plan, HTNTask&& task);
            friend void HTNPlanAddSnapFloat(HTNPlan& plan,
                                            Dia::Core::StringCRC slot,
                                            Dia::Core::StringCRC field,
                                            float value);
            friend void HTNPlanAddSnapBool(HTNPlan& plan,
                                           Dia::Core::StringCRC slot,
                                           Dia::Core::StringCRC field,
                                           bool value);

            struct Impl;
            Impl* mImpl;
            int   mCursor;
        };

        // Package-private: used by HTNPlanner to populate a plan.
        void HTNPlanAddTask(HTNPlan& plan, HTNTask&& task);
        void HTNPlanAddSnapFloat(HTNPlan& plan, Dia::Core::StringCRC slot,
                                 Dia::Core::StringCRC field, float value);
        void HTNPlanAddSnapBool(HTNPlan& plan, Dia::Core::StringCRC slot,
                                Dia::Core::StringCRC field, bool value);

    } // namespace HTN
} // namespace Dia
