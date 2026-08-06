#include <DiaObjective/ObjectiveSet.h>

#include <DiaCore/Core/Assert.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Trace/DiaTrace.h>
#include <DiaObservation/Profile/DiaProfile.h>
#include <DiaObservation/Metric/MetricRegistry.h>
#include <DiaObservation/Metric/Counter.h>

#include <algorithm>
#include <vector>

namespace Dia
{
    namespace Objective
    {
        //-------------------------------------------------------------------------------------------
        // ObjectiveSet::Impl
        //-------------------------------------------------------------------------------------------
        struct ObjectiveSet::Impl
        {
            std::vector<ObjectiveDef>        defs;
            std::vector<ObjectiveState>      states;   // parallel to defs
            std::vector<IObjectiveObserver*> observers;

            Dia::Observation::Metric::Counter* activationsCounter = nullptr;
            Dia::Observation::Metric::Counter* completionsCounter = nullptr;
            Dia::Observation::Metric::Counter* failuresCounter    = nullptr;
        };

        //-------------------------------------------------------------------------------------------
        // ObjectiveSet
        //-------------------------------------------------------------------------------------------
        ObjectiveSet::ObjectiveSet()
            : mImpl(new Impl())
        {
        }

        ObjectiveSet::~ObjectiveSet()
        {
            delete mImpl;
            mImpl = nullptr;
        }

        ObjectiveSet::ObjectiveSet(ObjectiveSet&& other) noexcept
            : mImpl(other.mImpl)
        {
            other.mImpl = nullptr;
        }

        ObjectiveSet& ObjectiveSet::operator=(ObjectiveSet&& other) noexcept
        {
            if (this != &other)
            {
                delete mImpl;
                mImpl = other.mImpl;
                other.mImpl = nullptr;
            }
            return *this;
        }

        //-------------------------------------------------------------------------------------------
        // InitMetrics
        //-------------------------------------------------------------------------------------------
        void ObjectiveSet::InitMetrics()
        {
            if (!mImpl) return;
            auto& reg = Dia::Observation::Metric::MetricRegistry::Instance();
            mImpl->activationsCounter = reg.RegisterCounter(Dia::Core::StringCRC("objective.activations"));
            mImpl->completionsCounter = reg.RegisterCounter(Dia::Core::StringCRC("objective.completions"));
            mImpl->failuresCounter    = reg.RegisterCounter(Dia::Core::StringCRC("objective.failures"));
        }

        //-------------------------------------------------------------------------------------------
        // LoadFromJson
        //-------------------------------------------------------------------------------------------
        ObjectiveSet ObjectiveSet::LoadFromJson(
            const Json::Value& root,
            Dia::Core::Containers::DynamicArrayC<const char*, 32>& outErrors)
        {
            ObjectiveSet set;

            const Json::Value& objectives = root["objectives"];
            if (!objectives.isArray())
            {
                outErrors.Add("ObjectiveSet: missing or non-array 'objectives' field");
                return set;
            }

            for (Json::ArrayIndex i = 0; i < objectives.size(); ++i)
            {
                const Json::Value& entry = objectives[i];
                if (!entry.isObject())
                {
                    outErrors.Add("ObjectiveSet: objectives entry is not an object");
                    continue;
                }

                ObjectiveDef def;

                // id
                if (entry.isMember("id") && entry["id"].isString())
                {
                    def.id = Dia::Core::StringCRC(entry["id"].asCString());
                }

                // classification
                def.classification = ObjectiveClassification::kPrimary;
                if (entry.isMember("classification") && entry["classification"].isString())
                {
                    const char* cls = entry["classification"].asCString();
                    if (std::strcmp(cls, "secondary") == 0)
                    {
                        def.classification = ObjectiveClassification::kSecondary;
                    }
                    else if (std::strcmp(cls, "optional") == 0)
                    {
                        def.classification = ObjectiveClassification::kOptional;
                    }
                    // "primary" and unknown both default to kPrimary
                }

                // completion (required)
                if (!entry.isMember("completion"))
                {
                    outErrors.Add("ObjectiveSet: objective missing required 'completion' field");
                    continue;
                }
                def.completion = Dia::Condition::ConditionExpr::LoadFromJson(entry["completion"], outErrors);
                if (!def.completion.IsValid())
                {
                    outErrors.Add("ObjectiveSet: objective 'completion' expression failed to parse");
                    continue;
                }

                // failure (optional)
                if (entry.isMember("failure"))
                {
                    def.failure = Dia::Condition::ConditionExpr::LoadFromJson(entry["failure"], outErrors);
                }

                // reward (optional)
                if (entry.isMember("reward") && entry["reward"].isArray())
                {
                    const Json::Value& reward = entry["reward"];
                    for (Json::ArrayIndex r = 0; r < reward.size(); ++r)
                    {
                        const Json::Value& rewardEntry = reward[r];
                        if (!rewardEntry.isObject())
                        {
                            continue;
                        }

                        RewardEntry re;
                        re.type   = Dia::Core::StringCRC(
                            rewardEntry.isMember("type") ? rewardEntry["type"].asCString() : "");
                        re.amount = rewardEntry.isMember("amount")
                                        ? rewardEntry["amount"].asFloat()
                                        : 0.0f;

                        if (!def.reward.IsFull())
                        {
                            def.reward.Add(re);
                        }
                    }
                }

                // prerequisites (optional, up to 4)
                if (entry.isMember("prerequisites") && entry["prerequisites"].isArray())
                {
                    const Json::Value& prereqs = entry["prerequisites"];
                    for (Json::ArrayIndex p = 0; p < prereqs.size(); ++p)
                    {
                        if (!prereqs[p].isString())
                        {
                            continue;
                        }
                        if (!def.prerequisites.IsFull())
                        {
                            def.prerequisites.Add(Dia::Core::StringCRC(prereqs[p].asCString()));
                        }
                    }
                }

                // Initial state: kActive if no prerequisites, kInactive otherwise
                ObjectiveState initialState =
                    (def.prerequisites.Size() == 0)
                        ? ObjectiveState::kActive
                        : ObjectiveState::kInactive;

                set.mImpl->defs.push_back(std::move(def));
                set.mImpl->states.push_back(initialState);
            }

            return set;
        }

        //-------------------------------------------------------------------------------------------
        // Validate
        //-------------------------------------------------------------------------------------------
        bool ObjectiveSet::Validate(
            const Dia::Condition::ConditionRegistry& registry,
            Dia::Core::Containers::DynamicArrayC<const char*, 32>& outErrors) const
        {
            bool valid = true;
            for (const ObjectiveDef& def : mImpl->defs)
            {
                if (!def.completion.Validate(registry, outErrors))
                {
                    valid = false;
                }
                if (def.failure.IsValid())
                {
                    if (!def.failure.Validate(registry, outErrors))
                    {
                        valid = false;
                    }
                }
            }
            return valid;
        }

        //-------------------------------------------------------------------------------------------
        // Evaluate
        //-------------------------------------------------------------------------------------------
        int ObjectiveSet::Evaluate(Dia::Condition::IConditionContext& ctx)
        {
            DIA_TRACE_ZONE("ObjectiveSet::Evaluate", Dia::Observation::Trace::Category::kNone);
            DIA_PROFILE_SCOPE("ObjectiveSet::Evaluate", Dia::Observation::Profile::Category::kNone);
            int transitions = 0;
            const int count = static_cast<int>(mImpl->defs.size());

            // Step 1: Activation — promote kInactive objectives whose prerequisites are all kComplete
            for (int i = 0; i < count; ++i)
            {
                if (mImpl->states[i] != ObjectiveState::kInactive)
                {
                    continue;
                }

                const ObjectiveDef& def = mImpl->defs[i];
                bool allPrereqsMet = true;
                for (unsigned int p = 0; p < def.prerequisites.Size(); ++p)
                {
                    ObjectiveState prereqState = GetState(def.prerequisites[p]);
                    if (prereqState != ObjectiveState::kComplete)
                    {
                        allPrereqsMet = false;
                        break;
                    }
                }

                if (allPrereqsMet)
                {
                    mImpl->states[i] = ObjectiveState::kActive;
                    for (IObjectiveObserver* obs : mImpl->observers)
                    {
                        obs->OnObjectiveActivated(def.id);
                    }
                    DIA_LOG_DEBUG("Objective", "objective activated: id=%u", def.id.Value());
                    if (mImpl->activationsCounter) mImpl->activationsCounter->Inc();
                    ++transitions;
                }
            }

            // Step 2: Evaluation — check completion and failure for each kActive objective
            for (int i = 0; i < count; ++i)
            {
                if (mImpl->states[i] != ObjectiveState::kActive)
                {
                    continue;
                }

                const ObjectiveDef& def = mImpl->defs[i];

                if (def.completion.Evaluate(ctx))
                {
                    mImpl->states[i] = ObjectiveState::kComplete;
                    for (IObjectiveObserver* obs : mImpl->observers)
                    {
                        obs->OnObjectiveCompleted(def.id, def.reward);
                    }
                    DIA_LOG_INFO("Objective", "objective completed: id=%u", def.id.Value());
                    if (mImpl->completionsCounter) mImpl->completionsCounter->Inc();
                    ++transitions;
                }
                else if (def.failure.IsValid() && def.failure.Evaluate(ctx))
                {
                    mImpl->states[i] = ObjectiveState::kFailed;
                    for (IObjectiveObserver* obs : mImpl->observers)
                    {
                        obs->OnObjectiveFailed(def.id);
                    }
                    DIA_LOG_INFO("Objective", "objective failed: id=%u", def.id.Value());
                    if (mImpl->failuresCounter) mImpl->failuresCounter->Inc();
                    ++transitions;
                }
            }

            return transitions;
        }

        //-------------------------------------------------------------------------------------------
        // Observer management
        //-------------------------------------------------------------------------------------------
        void ObjectiveSet::AddObserver(IObjectiveObserver* observer)
        {
            DIA_ASSERT(observer != nullptr, "ObjectiveSet::AddObserver: observer must not be null");
            mImpl->observers.push_back(observer);
        }

        void ObjectiveSet::RemoveObserver(IObjectiveObserver* observer)
        {
            auto& obs = mImpl->observers;
            obs.erase(std::remove(obs.begin(), obs.end(), observer), obs.end());
        }

        //-------------------------------------------------------------------------------------------
        // Accessors
        //-------------------------------------------------------------------------------------------
        ObjectiveState ObjectiveSet::GetState(Dia::Core::StringCRC objectiveId) const
        {
            for (int i = 0; i < static_cast<int>(mImpl->defs.size()); ++i)
            {
                if (mImpl->defs[i].id.Value() == objectiveId.Value())
                {
                    return mImpl->states[i];
                }
            }
            return ObjectiveState::kInactive;
        }

        int ObjectiveSet::GetCount() const
        {
            return static_cast<int>(mImpl->defs.size());
        }

        const ObjectiveDef* ObjectiveSet::GetAt(int index) const
        {
            if (index < 0 || index >= static_cast<int>(mImpl->defs.size()))
            {
                return nullptr;
            }
            return &mImpl->defs[index];
        }

        bool ObjectiveSet::AllPrimaryComplete() const
        {
            for (int i = 0; i < static_cast<int>(mImpl->defs.size()); ++i)
            {
                if (mImpl->defs[i].classification == ObjectiveClassification::kPrimary)
                {
                    if (mImpl->states[i] != ObjectiveState::kComplete)
                    {
                        return false;
                    }
                }
            }
            return true;
        }

        bool ObjectiveSet::AnyPrimaryFailed() const
        {
            for (int i = 0; i < static_cast<int>(mImpl->defs.size()); ++i)
            {
                if (mImpl->defs[i].classification == ObjectiveClassification::kPrimary)
                {
                    if (mImpl->states[i] == ObjectiveState::kFailed)
                    {
                        return true;
                    }
                }
            }
            return false;
        }

    } // namespace Objective
} // namespace Dia
