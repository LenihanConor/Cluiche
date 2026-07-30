#include <DiaUtilityAI/UtilitySet.h>
#include <DiaUtilityAI/ResponseCurve.h>
#include <DiaCondition/ConditionExpr.h>
#include <DiaRules/RuleActionRegistry.h>
#include <DiaCore/CRC/StringCRC.h>
#include <vector>
#include <algorithm>

namespace Dia
{
    namespace UtilityAI
    {
        struct UtilitySet::Impl
        {
            std::vector<ActionDef> actions;

#ifdef DIA_DEBUG
            mutable std::vector<Dia::Core::StringCRC> lastScoreIds;
            mutable std::vector<float>                lastScores;
#endif
        };

        UtilitySet::UtilitySet()
            : mImpl(new Impl())
        {
        }

        UtilitySet::~UtilitySet()
        {
            delete mImpl;
        }

        UtilitySet::UtilitySet(UtilitySet&& other) noexcept
            : mImpl(other.mImpl)
        {
            other.mImpl = nullptr;
        }

        UtilitySet& UtilitySet::operator=(UtilitySet&& other) noexcept
        {
            if (this != &other)
            {
                delete mImpl;
                mImpl = other.mImpl;
                other.mImpl = nullptr;
            }
            return *this;
        }

        UtilitySelection UtilitySet::Evaluate(
            Dia::Condition::IConditionContext& ctx,
            const Dia::Rules::RuleActionRegistry& registry,
            void* actionContext,
            GroupConsiderationContext* group) const
        {
            UtilitySelection winner;

#ifdef DIA_DEBUG
            mImpl->lastScoreIds.clear();
            mImpl->lastScores.clear();
#endif

            for (const ActionDef& def : mImpl->actions)
            {
                // 1. Check prerequisite
                if (def.prerequisite.IsValid())
                {
                    if (!def.prerequisite.Evaluate(ctx))
                    {
#ifdef DIA_DEBUG
                        mImpl->lastScoreIds.push_back(def.actionId);
                        mImpl->lastScores.push_back(0.0f);
#endif
                        continue;
                    }
                }
                // If prerequisite is not valid (default-constructed), treat as always pass

                // 2. Check maxConcurrent
                if (group != nullptr && def.maxConcurrent > 0)
                {
                    if (group->GetCount(def.actionId) >= def.maxConcurrent)
                    {
#ifdef DIA_DEBUG
                        mImpl->lastScoreIds.push_back(def.actionId);
                        mImpl->lastScores.push_back(0.0f);
#endif
                        continue;
                    }
                }

                // 3. Compute score = product of all scorer outputs
                float score = 1.0f;

                if (def.scorers.Size() == 0)
                {
                    // No scorers: default to fully eligible (score = 1.0f)
                    score = 1.0f;
                }
                else
                {
                    for (unsigned int i = 0; i < def.scorers.Size(); ++i)
                    {
                        const ScorerDef& scorer = def.scorers[i];

                        float rawValue = ctx.GetFloat(scorer.slot, scorer.field);

                        // Normalise to [0,1]
                        float normInput = 0.0f;
                        float range = scorer.inputMax - scorer.inputMin;
                        if (range > 1e-7f)
                        {
                            normInput = (rawValue - scorer.inputMin) / range;
                        }

                        // Clamp to [0,1]
                        if (normInput < 0.0f) normInput = 0.0f;
                        if (normInput > 1.0f) normInput = 1.0f;

                        // Evaluate curve
                        float scorerOutput = scorer.curve.Evaluate(normInput);

                        score *= scorerOutput;

                        // Early out: product already at zero
                        if (score <= 0.0f)
                        {
                            score = 0.0f;
                            break;
                        }
                    }
                }

#ifdef DIA_DEBUG
                mImpl->lastScoreIds.push_back(def.actionId);
                mImpl->lastScores.push_back(score);
#endif

                // 5. Track winner
                if (score > winner.score)
                {
                    winner.score    = score;
                    winner.actionId = def.actionId;
                }
            }

            // Dispatch winner if eligible
            if (winner.score > 0.0f)
            {
                Dia::Rules::RuleActionFn fn = registry.Find(winner.actionId);
                if (fn != nullptr)
                {
                    fn(actionContext);
                }
            }

            return winner;
        }

        UtilitySet UtilitySet::LoadFromJson(const Json::Value& root)
        {
            UtilitySet result;

            if (!root.isMember("actions") || !root["actions"].isArray())
            {
                return result;
            }

            const Json::Value& actionsArray = root["actions"];
            for (Json::ArrayIndex i = 0; i < actionsArray.size(); ++i)
            {
                const Json::Value& actionNode = actionsArray[i];

                ActionDef def;

                // Parse id
                if (actionNode.isMember("id") && actionNode["id"].isString())
                {
                    def.actionId = Dia::Core::StringCRC(actionNode["id"].asCString());
                }

                // Parse prerequisite
                if (actionNode.isMember("prerequisite"))
                {
                    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
                    def.prerequisite = Dia::Condition::ConditionExpr::LoadFromJson(
                        actionNode["prerequisite"], errors);
                }
                // Otherwise, leave def.prerequisite default-constructed (invalid = always pass)

                // Parse scorers
                if (actionNode.isMember("scorers") && actionNode["scorers"].isArray())
                {
                    const Json::Value& scorersArray = actionNode["scorers"];
                    for (Json::ArrayIndex s = 0; s < scorersArray.size(); ++s)
                    {
                        const Json::Value& scorerNode = scorersArray[s];

                        ScorerDef scorer;

                        if (scorerNode.isMember("slot") && scorerNode["slot"].isString())
                        {
                            scorer.slot = Dia::Core::StringCRC(scorerNode["slot"].asCString());
                        }

                        if (scorerNode.isMember("field") && scorerNode["field"].isString())
                        {
                            scorer.field = Dia::Core::StringCRC(scorerNode["field"].asCString());
                        }

                        if (scorerNode.isMember("input_min") && scorerNode["input_min"].isNumeric())
                        {
                            scorer.inputMin = scorerNode["input_min"].asFloat();
                        }

                        if (scorerNode.isMember("input_max") && scorerNode["input_max"].isNumeric())
                        {
                            scorer.inputMax = scorerNode["input_max"].asFloat();
                        }

                        if (scorerNode.isMember("curve"))
                        {
                            scorer.curve = ResponseCurve::LoadFromJson(scorerNode["curve"]);
                        }

                        def.scorers.Add(scorer);
                    }
                }

                // Parse cooldown
                if (actionNode.isMember("cooldown") && actionNode["cooldown"].isNumeric())
                {
                    def.cooldownSeconds = actionNode["cooldown"].asFloat();
                }

                // Parse max_concurrent
                if (actionNode.isMember("max_concurrent") && actionNode["max_concurrent"].isNumeric())
                {
                    def.maxConcurrent = actionNode["max_concurrent"].asInt();
                }

                result.mImpl->actions.push_back(std::move(def));
            }

            return result;
        }

        bool UtilitySet::Validate(
            const Dia::Condition::ConditionRegistry& registry,
            Dia::Core::Containers::DynamicArrayC<const char*, 32>& outErrors) const
        {
            bool valid = true;

            for (const ActionDef& def : mImpl->actions)
            {
                // Validate action ID is non-zero
                if (def.actionId.Value() == 0)
                {
                    if (!outErrors.IsFull())
                        outErrors.Add("UtilitySet: ActionDef has empty/zero action ID");
                    valid = false;
                }

                // Validate prerequisite if valid
                if (def.prerequisite.IsValid())
                {
                    if (!def.prerequisite.Validate(registry, outErrors))
                        valid = false;
                }

                // Validate scorers
                for (unsigned int s = 0; s < def.scorers.Size(); ++s)
                {
                    const ScorerDef& scorer = def.scorers[s];

                    if (scorer.slot.Value() == 0)
                    {
                        if (!outErrors.IsFull())
                            outErrors.Add("UtilitySet: ScorerDef has empty slot");
                        valid = false;
                    }

                    if (scorer.field.Value() == 0)
                    {
                        if (!outErrors.IsFull())
                            outErrors.Add("UtilitySet: ScorerDef has empty field");
                        valid = false;
                    }
                }
            }

            return valid;
        }

        int UtilitySet::GetActionCount() const
        {
            return static_cast<int>(mImpl->actions.size());
        }

        void UtilitySet::GetLastFrameScores(
            Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 32>& outIds,
            Dia::Core::Containers::DynamicArrayC<float, 32>& outScores) const
        {
#ifdef DIA_DEBUG
            const int count = static_cast<int>(mImpl->lastScoreIds.size());
            for (int i = 0; i < count; ++i)
            {
                if (outIds.IsFull() || outScores.IsFull())
                    break;
                outIds.Add(mImpl->lastScoreIds[static_cast<size_t>(i)]);
                outScores.Add(mImpl->lastScores[static_cast<size_t>(i)]);
            }
#else
            (void)outIds;
            (void)outScores;
#endif
        }

    } // namespace UtilityAI
} // namespace Dia
