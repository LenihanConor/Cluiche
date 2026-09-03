#include <DiaUtilityAI/UtilitySet.h>
#include <DiaUtilityAI/PersonalityProfile.h>
#include <DiaUtilityAI/ResponseCurve.h>
#include <DiaCondition/ConditionExpr.h>
#include <DiaRules/RuleActionRegistry.h>
#include <DiaSimTime/IOneShotWork.h>
#include <DiaSimTime/SimTimeBudget.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Core/Assert.h>
#include <vector>
#include <list>
#include <algorithm>

namespace Dia
{
    namespace UtilityAI
    {
        // ---------------------------------------------------------------------------
        // UtilityEvalWorkItem — one-shot IOneShotWork submitted by EvaluateAsync.
        // Stored in a std::list (stable addresses) inside Impl.
        // ---------------------------------------------------------------------------
        struct UtilityEvalWorkItem : public Dia::SimTime::IOneShotWork
        {
            const UtilitySet*                     set;
            Dia::Condition::IConditionContext*    ctx;
            const Dia::Rules::RuleActionRegistry* registry;
            void*                                 actionContext;
            GroupConsiderationContext*            group;
            const PersonalityProfile*             personality;
            UtilityResultCallback                 callback;
            void*                                 callbackUserData;
            Dia::SimTime::SimTimeBudget*          budget;

            // Completion flag. NOT a re-entrancy guard for Step() — SimTimeBudget::
            // RunOneShots() removes an item from its own queue once Step() returns
            // true, so Step() is guaranteed to run at most once per submission.
            // This flag exists purely so EvaluateAsync can prune completed entries
            // from mImpl->pendingWorkItems (this list has no relationship to
            // SimTimeBudget's queue and needs its own bookkeeping).
            bool                                  mFired;

            UtilityEvalWorkItem()
                : set(nullptr), ctx(nullptr), registry(nullptr)
                , actionContext(nullptr), group(nullptr), personality(nullptr)
                , callback(nullptr), callbackUserData(nullptr)
                , budget(nullptr), mFired(false)
            {}

            bool Step(float /*budgetMs*/) override
            {
                // Run sync evaluation (dispatches winner via registry).
                UtilitySelection result = set->Evaluate(*ctx, *registry, actionContext, group, personality);

                mFired = true;

                if (callback)
                    callback(result, callbackUserData);

                return true; // always fully synchronous — completes in one Step() call.
            }
        };

        struct UtilitySet::Impl
        {
            std::vector<ActionDef> actions;
            std::list<UtilityEvalWorkItem> pendingWorkItems;

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
            if (mImpl)
            {
                // NOTE: Unlike the old AIBudgetScheduler-backed path, SimTimeBudget's
                // one-shot queue (ST-012) has no per-item Unregister/cancel API — a
                // pending (unfired) work item cannot be pulled out of the queue here.
                // Destroying a UtilitySet while an EvaluateAsync call is still pending
                // is therefore a caller error (Step() would later dereference a
                // dangling `set` pointer); assert in debug to catch misuse instead of
                // silently leaving a dangling pointer in the queue.
                for (const UtilityEvalWorkItem& item : mImpl->pendingWorkItems)
                {
                    DIA_ASSERT(item.mFired,
                        "UtilitySet destroyed with a pending EvaluateAsync work item still queued in SimTimeBudget");
                }
                delete mImpl;
            }
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

        UtilitySelection UtilitySet::SelectWinner(
            Dia::Condition::IConditionContext& ctx,
            GroupConsiderationContext* group,
            const PersonalityProfile* personality) const
        {
            UtilitySelection winner;

#ifdef DIA_DEBUG
            mImpl->lastScoreIds.clear();
            mImpl->lastScores.clear();
#endif

            for (const ActionDef& def : mImpl->actions)
            {
                // 1. Check prerequisite (invalid/default = always pass)
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

                for (unsigned int i = 0; i < def.scorers.Size(); ++i)
                {
                    const ScorerDef& scorer = def.scorers[i];
                    float rawValue  = ctx.GetFloat(scorer.slot, scorer.field);
                    float range     = scorer.inputMax - scorer.inputMin;
                    float normInput = (range > 1e-7f) ? (rawValue - scorer.inputMin) / range : 0.0f;
                    if (normInput < 0.0f) normInput = 0.0f;
                    if (normInput > 1.0f) normInput = 1.0f;
                    score *= scorer.curve.Evaluate(normInput);
                    if (score <= 0.0f) { score = 0.0f; break; }
                }

                // Apply personality bias multiplier if present
                if (personality != nullptr)
                    score *= personality->GetScoreMultiplier(def.actionId);

#ifdef DIA_DEBUG
                mImpl->lastScoreIds.push_back(def.actionId);
                mImpl->lastScores.push_back(score);
#endif

                if (score > winner.score)
                {
                    winner.score    = score;
                    winner.actionId = def.actionId;
                }
            }

            return winner;
        }

        UtilitySelection UtilitySet::Evaluate(
            Dia::Condition::IConditionContext& ctx,
            const Dia::Rules::RuleActionRegistry& registry,
            void* actionContext,
            GroupConsiderationContext* group,
            const PersonalityProfile* personality) const
        {
            UtilitySelection winner = SelectWinner(ctx, group, personality);

            if (winner.score > 0.0f)
            {
                Dia::Rules::RuleActionFn fn = registry.Find(winner.actionId);
                if (fn != nullptr)
                    fn(actionContext);
            }

            return winner;
        }

        void UtilitySet::EvaluateAsync(
            Dia::Condition::IConditionContext& ctx,
            const Dia::Rules::RuleActionRegistry& registry,
            void* actionContext,
            Dia::SimTime::SimTimeBudget& budget,
            UtilityResultCallback callback,
            void* callbackUserData,
            GroupConsiderationContext* group,
            const PersonalityProfile* personality)
        {
            // Prune fired items before adding new ones to keep the list bounded.
            for (auto it = mImpl->pendingWorkItems.begin(); it != mImpl->pendingWorkItems.end(); )
            {
                if (it->mFired)
                    it = mImpl->pendingWorkItems.erase(it);
                else
                    ++it;
            }

            // Allocate a new work item in the list (stable address while in list).
            mImpl->pendingWorkItems.emplace_back();
            UtilityEvalWorkItem& item = mImpl->pendingWorkItems.back();

            item.set              = this;
            item.ctx              = &ctx;
            item.registry         = &registry;
            item.actionContext    = actionContext;
            item.group            = group;
            item.personality      = personality;
            item.callback         = callback;
            item.callbackUserData = callbackUserData;
            item.budget           = &budget;
            item.mFired           = false;

            budget.SubmitOneShot(&item);
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

        float UtilitySet::GetCooldownForAction(Dia::Core::StringCRC actionId) const
        {
            for (const ActionDef& def : mImpl->actions)
            {
                if (def.actionId == actionId)
                    return def.cooldownSeconds;
            }
            return 0.0f;
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
