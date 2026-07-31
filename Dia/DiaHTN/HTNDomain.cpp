#include "HTNDomain.h"

#include <DiaCondition/ConditionExpr.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Trace/DiaTrace.h>
#include <DiaObservation/Profile/DiaProfile.h>

#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Dia
{
    namespace HTN
    {
        // -----------------------------------------------------------------------
        // Internal domain data structures — confined entirely to this .cpp.
        // -----------------------------------------------------------------------

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

        struct PrimitiveTaskDef
        {
            Dia::Core::StringCRC id;
            Dia::Core::StringCRC operatorId;
            std::vector<Dia::Core::StringCRC> params;
        };

        struct HTNDomain::Impl
        {
            std::unordered_map<unsigned int, CompoundTaskDef> compounds;
            std::unordered_map<unsigned int, PrimitiveTaskDef> primitives;

            bool HasTask(Dia::Core::StringCRC id) const
            {
                return compounds.count(id.Value()) || primitives.count(id.Value());
            }
        };

        // -----------------------------------------------------------------------
        // Constructor / destructor / move
        // -----------------------------------------------------------------------
        HTNDomain::HTNDomain()
            : mImpl(new Impl())
            , mValid(false)
        {
        }

        HTNDomain::~HTNDomain()
        {
            delete mImpl;
        }

        HTNDomain::HTNDomain(HTNDomain&& other) noexcept
            : mImpl(other.mImpl)
            , mValid(other.mValid)
        {
            other.mImpl  = nullptr;
            other.mValid = false;
        }

        HTNDomain& HTNDomain::operator=(HTNDomain&& other) noexcept
        {
            if (this != &other)
            {
                delete mImpl;
                mImpl        = other.mImpl;
                mValid       = other.mValid;
                other.mImpl  = nullptr;
                other.mValid = false;
            }
            return *this;
        }

        // -----------------------------------------------------------------------
        // LoadFromJson
        // -----------------------------------------------------------------------
        HTNDomain HTNDomain::LoadFromJson(
            const Json::Value& root,
            Dia::Core::Containers::DynamicArrayC<const char*, 32>& outErrors)
        {
            DIA_TRACE_ZONE  ("htn.domain.load", ::Dia::Observation::Trace::Category::kNone);
            DIA_PROFILE_SCOPE("htn.domain.load", ::Dia::Observation::Profile::Category::kNone);

            HTNDomain domain;

            const Json::Value& tasks = root["tasks"];
            if (!tasks.isObject())
            {
                if (!outErrors.IsFull())
                    outErrors.Add("HTNDomain: missing or non-object 'tasks' key");
                DIA_LOG_ERROR("HTN", "htn.domain.load_failed: missing 'tasks' key");
                return domain;
            }

            for (const std::string& taskName : tasks.getMemberNames())
            {
                const Json::Value& taskNode = tasks[taskName];
                if (!taskNode.isObject())
                    continue;

                const Dia::Core::StringCRC taskId(taskName.c_str());

                if (!taskNode.isMember("type"))
                    continue;

                const std::string typeStr = taskNode["type"].asString();

                if (typeStr == "compound")
                {
                    CompoundTaskDef compound;
                    compound.id = taskId;

                    const Json::Value& methods = taskNode["methods"];
                    if (methods.isArray())
                    {
                        for (Json::ArrayIndex i = 0; i < methods.size(); ++i)
                        {
                            const Json::Value& mNode = methods[i];
                            if (!mNode.isObject())
                                continue;

                            MethodDef method;

                            if (mNode.isMember("id") && mNode["id"].isString())
                                method.id = Dia::Core::StringCRC(mNode["id"].asString().c_str());

                            if (mNode.isMember("precondition"))
                            {
                                Dia::Core::Containers::DynamicArrayC<const char*, 32> loadErrors;
                                method.precondition = Dia::Condition::ConditionExpr::LoadFromJson(
                                    mNode["precondition"], loadErrors);
                            }

                            if (mNode.isMember("subtasks") && mNode["subtasks"].isArray())
                            {
                                const Json::Value& subs = mNode["subtasks"];
                                for (Json::ArrayIndex j = 0; j < subs.size(); ++j)
                                {
                                    if (subs[j].isString())
                                        method.subtasks.push_back(
                                            Dia::Core::StringCRC(subs[j].asString().c_str()));
                                }
                            }

                            compound.methods.push_back(std::move(method));
                        }
                    }

                    domain.mImpl->compounds.emplace(taskId.Value(), std::move(compound));
                }
                else if (typeStr == "primitive")
                {
                    PrimitiveTaskDef prim;
                    prim.id = taskId;

                    if (taskNode.isMember("operator") && taskNode["operator"].isString())
                        prim.operatorId = Dia::Core::StringCRC(taskNode["operator"].asString().c_str());

                    if (taskNode.isMember("params") && taskNode["params"].isArray())
                    {
                        const Json::Value& paramsNode = taskNode["params"];
                        for (Json::ArrayIndex i = 0; i < paramsNode.size(); ++i)
                        {
                            if (paramsNode[i].isString())
                                prim.params.push_back(
                                    Dia::Core::StringCRC(paramsNode[i].asString().c_str()));
                        }
                    }

                    domain.mImpl->primitives.emplace(taskId.Value(), std::move(prim));
                }
            }

            domain.mValid = true;
            DIA_LOG_DEBUG("HTN", "htn.domain.loaded: %d compound, %d primitive",
                static_cast<int>(domain.mImpl->compounds.size()),
                static_cast<int>(domain.mImpl->primitives.size()));
            return domain;
        }

        // -----------------------------------------------------------------------
        // Validate
        // -----------------------------------------------------------------------
        bool HTNDomain::Validate(Dia::Core::Containers::DynamicArrayC<const char*, 32>& outErrors) const
        {
            DIA_TRACE_ZONE  ("htn.domain.validate", ::Dia::Observation::Trace::Category::kNone);
            DIA_PROFILE_SCOPE("htn.domain.validate", ::Dia::Observation::Profile::Category::kNone);

            if (!mImpl)
                return true;

            bool valid = true;

            for (auto& [key, compound] : mImpl->compounds)
            {
                for (const MethodDef& method : compound.methods)
                {
                    for (const Dia::Core::StringCRC& sub : method.subtasks)
                    {
                        if (!mImpl->HasTask(sub))
                        {
                            if (!outErrors.IsFull())
                                outErrors.Add("HTNDomain: method references unknown sub-task");
                            DIA_LOG_WARNING("HTN", "htn.domain.validate: unknown sub-task crc=%u", sub.Value());
                            valid = false;
                        }
                    }
                }
            }

            std::unordered_set<unsigned int> grey;
            std::unordered_set<unsigned int> black;

            std::function<bool(Dia::Core::StringCRC)> dfs = [&](Dia::Core::StringCRC id) -> bool
            {
                const unsigned int key = id.Value();
                if (black.count(key)) return false;
                if (grey.count(key))
                {
                    if (!outErrors.IsFull())
                        outErrors.Add("HTNDomain: cycle detected in compound task graph");
                    DIA_LOG_WARNING("HTN", "htn.domain.validate: cycle detected at task crc=%u", key);
                    return true;
                }
                grey.insert(key);

                auto it = mImpl->compounds.find(key);
                if (it != mImpl->compounds.end())
                {
                    for (const MethodDef& method : it->second.methods)
                        for (const Dia::Core::StringCRC& sub : method.subtasks)
                            if (dfs(sub)) return true;
                }

                grey.erase(key);
                black.insert(key);
                return false;
            };

            for (auto& [key, compound] : mImpl->compounds)
            {
                if (dfs(compound.id))
                {
                    valid = false;
                    break;
                }
            }

            return valid;
        }

        // -----------------------------------------------------------------------
        // Accessors
        // -----------------------------------------------------------------------
        bool HTNDomain::IsValid() const { return mValid; }

        bool HTNDomain::IsCompound(Dia::Core::StringCRC taskId) const
        {
            if (!mImpl) return false;
            return mImpl->compounds.count(taskId.Value()) != 0;
        }

        bool HTNDomain::IsPrimitive(Dia::Core::StringCRC taskId) const
        {
            if (!mImpl) return false;
            return mImpl->primitives.count(taskId.Value()) != 0;
        }

        // -----------------------------------------------------------------------
        // Method iteration API
        // -----------------------------------------------------------------------
        int HTNDomain::GetMethodCount(Dia::Core::StringCRC compoundTaskId) const
        {
            if (!mImpl) return 0;
            auto it = mImpl->compounds.find(compoundTaskId.Value());
            if (it == mImpl->compounds.end()) return 0;
            return static_cast<int>(it->second.methods.size());
        }

        bool HTNDomain::EvalMethodPrecondition(Dia::Core::StringCRC compoundTaskId,
                                                int methodIndex,
                                                Dia::Condition::IConditionContext& ctx) const
        {
            if (!mImpl) return false;
            auto it = mImpl->compounds.find(compoundTaskId.Value());
            if (it == mImpl->compounds.end()) return false;
            const auto& methods = it->second.methods;
            if (methodIndex < 0 || methodIndex >= static_cast<int>(methods.size())) return false;

            const MethodDef& method = methods[static_cast<std::size_t>(methodIndex)];
            // No precondition → always passes
            if (!method.precondition.IsValid()) return true;
            return method.precondition.Evaluate(ctx);
        }

        bool HTNDomain::GetMethodSubtasks(
            Dia::Core::StringCRC compoundTaskId,
            int methodIndex,
            Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 32>& outSubtasks) const
        {
            if (!mImpl) return false;
            auto it = mImpl->compounds.find(compoundTaskId.Value());
            if (it == mImpl->compounds.end()) return false;
            const auto& methods = it->second.methods;
            if (methodIndex < 0 || methodIndex >= static_cast<int>(methods.size())) return false;

            for (const Dia::Core::StringCRC& sub : methods[static_cast<std::size_t>(methodIndex)].subtasks)
                if (!outSubtasks.IsFull()) outSubtasks.Add(sub);

            return true;
        }

        // -----------------------------------------------------------------------
        // GetPrimitiveInfo
        // -----------------------------------------------------------------------
        PrimitiveInfo HTNDomain::GetPrimitiveInfo(Dia::Core::StringCRC primitiveTaskId) const
        {
            PrimitiveInfo info;
            if (!mImpl) return info;

            auto it = mImpl->primitives.find(primitiveTaskId.Value());
            if (it == mImpl->primitives.end()) return info;

            const PrimitiveTaskDef& prim = it->second;
            info.valid      = true;
            info.operatorId = prim.operatorId;
            for (const Dia::Core::StringCRC& p : prim.params)
                if (!info.params.IsFull()) info.params.Add(p);

            return info;
        }

    } // namespace HTN
} // namespace Dia
