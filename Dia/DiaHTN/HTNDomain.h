#pragma once

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Json/external/json/json.h>
#include <DiaCondition/IConditionContext.h>

namespace Dia
{
    namespace HTN
    {
        //-------------------------------------------------------------------------------------------
        // HTNDomain
        //
        // Immutable after LoadFromJson. Shared safely across entities running the same domain.
        //
        // SD-006: Immutable after LoadFromJson.
        // SD-007: Ordered method selection — first passing precondition wins.
        // SD-011: Caller owns JSON parsing; LoadFromJson takes Json::Value&.
        // PD-001: All task/method/operator IDs use StringCRC.
        // PD-004: No STL in public API — DynamicArrayC for error output and sub-task lists.
        // AD-003: Dia::HTN:: namespace.
        //-------------------------------------------------------------------------------------------

        // Info returned for a primitive task
        struct PrimitiveInfo
        {
            Dia::Core::StringCRC operatorId;
            Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 8> params;
            bool valid = false;
        };

        // HTN planner uses GetMethodCount / GetMethodSubtasks to iterate compound task methods.
        // This allows the planner to try each method in order and backtrack on branch failure.

        class HTNDomain
        {
        public:
            HTNDomain();
            ~HTNDomain();

            HTNDomain(HTNDomain&&) noexcept;
            HTNDomain& operator=(HTNDomain&&) noexcept;

            HTNDomain(const HTNDomain&) = delete;
            HTNDomain& operator=(const HTNDomain&) = delete;

            // Load from JSON. Returns an invalid domain and populates errors on failure.
            static HTNDomain LoadFromJson(
                const Json::Value& root,
                Dia::Core::Containers::DynamicArrayC<const char*, 32>& outErrors);

            // Validate: all sub-tasks resolvable, no cycles.
            bool Validate(Dia::Core::Containers::DynamicArrayC<const char*, 32>& outErrors) const;

            bool IsValid() const;

            bool IsCompound(Dia::Core::StringCRC taskId) const;
            bool IsPrimitive(Dia::Core::StringCRC taskId) const;

            // Returns number of methods on a compound task (0 if not compound).
            int GetMethodCount(Dia::Core::StringCRC compoundTaskId) const;

            // Evaluates precondition for method[methodIndex] on the given compound task.
            // Returns true if the precondition passes (or there is no precondition).
            // Returns false if methodIndex is out of range.
            bool EvalMethodPrecondition(Dia::Core::StringCRC compoundTaskId,
                                         int methodIndex,
                                         Dia::Condition::IConditionContext& ctx) const;

            // Fills outSubtasks with the sub-task IDs of method[methodIndex].
            // Returns false if methodIndex is out of range.
            bool GetMethodSubtasks(
                Dia::Core::StringCRC compoundTaskId,
                int methodIndex,
                Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 32>& outSubtasks) const;

            // Returns info for a primitive task. out.valid is false if taskId is not found.
            PrimitiveInfo GetPrimitiveInfo(Dia::Core::StringCRC primitiveTaskId) const;

        private:
            struct Impl;
            Impl* mImpl;
            bool  mValid;
        };

    } // namespace HTN
} // namespace Dia
