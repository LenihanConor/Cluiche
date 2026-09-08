#pragma once

// Internal header — NOT part of DiaHTN's public API.
// Exposes raw domain data structures for use by HTNPlanner.cpp and HTNDomain.cpp only.
// Do not include from outside DiaHTN.

#include <DiaHTN/HTNDomain.h>
#include <DiaCondition/ConditionExpr.h>
#include <DiaCore/CRC/StringCRC.h>

#include <vector>
#include <unordered_map>

namespace Dia
{
    namespace HTN
    {
        struct MethodDef
        {
            Dia::Core::StringCRC id;
            Dia::Condition::ConditionExpr precondition;
            std::vector<Dia::Core::StringCRC> subtasks;

            MethodDef() = default;
            MethodDef(MethodDef&&) = default;
            MethodDef& operator=(MethodDef&&) = default;
            MethodDef(const MethodDef&) = delete;
            MethodDef& operator=(const MethodDef&) = delete;
        };

        struct CompoundTaskDef
        {
            Dia::Core::StringCRC id;
            std::vector<MethodDef> methods;

            CompoundTaskDef() = default;
            CompoundTaskDef(CompoundTaskDef&&) = default;
            CompoundTaskDef& operator=(CompoundTaskDef&&) = default;
            CompoundTaskDef(const CompoundTaskDef&) = delete;
            CompoundTaskDef& operator=(const CompoundTaskDef&) = delete;
        };

        struct PrimitiveDef
        {
            Dia::Core::StringCRC id;
            Dia::Core::StringCRC operatorId;
            std::vector<Dia::Core::StringCRC> params;
        };

        // The Impl definition — must match HTNDomain::Impl exactly.
        // Defined here so HTNPlanner.cpp can access it.
        struct HTNDomain::Impl
        {
            std::unordered_map<unsigned int, CompoundTaskDef> compounds;
            std::unordered_map<unsigned int, PrimitiveDef>    primitives;

            bool HasTask(Dia::Core::StringCRC id) const
            {
                return compounds.count(id.Value()) || primitives.count(id.Value());
            }
        };

        // Accessor: casts the opaque void* back to Impl*.
        inline const HTNDomain::Impl* GetDomainImpl(const HTNDomain& domain)
        {
            return static_cast<const HTNDomain::Impl*>(domain.GetImplOpaque());
        }

    } // namespace HTN
} // namespace Dia
