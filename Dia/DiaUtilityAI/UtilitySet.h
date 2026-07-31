#pragma once

#include <DiaUtilityAI/ActionDef.h>
#include <DiaUtilityAI/GroupConsiderationContext.h>
#include <DiaCondition/IConditionContext.h>
#include <DiaCondition/ConditionRegistry.h>
#include <DiaRules/RuleActionRegistry.h>
#include <DiaAIBudget/AIBudgetScheduler.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Json/external/json/json.h>

namespace Dia
{
    namespace UtilityAI
    {
        struct UtilitySelection
        {
            Dia::Core::StringCRC actionId;  // kInvalidCRC (zero) if nothing eligible
            float score;                    // 0.0f if nothing eligible

            UtilitySelection() : score(0.0f) {}
        };

        // Callback type for EvaluateAsync. Fired on the thread that calls
        // AIBudgetScheduler::Update() when the work item is drained.
        // result — the winning selection (actionId == zero and score == 0.0f if nothing eligible).
        // userData — the pointer passed as callbackUserData to EvaluateAsync().
        using UtilityResultCallback = void(*)(UtilitySelection result, void* userData);

        class UtilitySet
        {
        public:
            UtilitySet();
            ~UtilitySet();

            // Move-only (holds vector of move-only ActionDefs)
            UtilitySet(UtilitySet&&) noexcept;
            UtilitySet& operator=(UtilitySet&&) noexcept;
            UtilitySet(const UtilitySet&) = delete;
            UtilitySet& operator=(const UtilitySet&) = delete;

            // Sync evaluation: score all eligible actions, dispatch the winner,
            // return the selection. Returns kInvalidCRC + 0.0f if nothing eligible.
            UtilitySelection Evaluate(
                Dia::Condition::IConditionContext& ctx,
                const Dia::Rules::RuleActionRegistry& registry,
                void* actionContext,
                GroupConsiderationContext* group = nullptr) const;

            // Async evaluation: submits a one-shot work item to scheduler.
            // All parameters are captured by pointer/value at submission time.
            // Caller is responsible for keeping ctx, registry, and any pointed-to
            // objects alive until the callback fires.
            // When the scheduler drains the work item, Evaluate() is called (which
            // dispatches the winner) and then callback is invoked with the selection.
            // The work item is one-shot — callback fires exactly once.
            void EvaluateAsync(
                Dia::Condition::IConditionContext& ctx,
                const Dia::Rules::RuleActionRegistry& registry,
                void* actionContext,
                Dia::AIBudget::AIBudgetScheduler& scheduler,
                UtilityResultCallback callback,
                void* callbackUserData,
                GroupConsiderationContext* group = nullptr);

            // JSON loading. See class doc for schema.
            static UtilitySet LoadFromJson(const Json::Value& root);

            // Validate: non-empty scorers lists, valid action IDs.
            // Returns false + populates outErrors on failure.
            bool Validate(
                const Dia::Condition::ConditionRegistry& registry,
                Dia::Core::Containers::DynamicArrayC<const char*, 32>& outErrors) const;

            int GetActionCount() const;

            // Returns the cooldown duration in seconds for a given action ID.
            // Returns 0.0f if the action is not found.
            float GetCooldownForAction(Dia::Core::StringCRC actionId) const;

            // Score-only: selects winner without dispatching. Used by UtilitySetComponent
            // to check cooldown before committing to dispatch.
            UtilitySelection SelectWinner(
                Dia::Condition::IConditionContext& ctx,
                GroupConsiderationContext* group = nullptr) const;

            // Last-frame scores — only populated after at least one Evaluate() call.
            // Only stores scores when DIA_DEBUG is defined; otherwise always empty.
            void GetLastFrameScores(
                Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 32>& outIds,
                Dia::Core::Containers::DynamicArrayC<float, 32>& outScores) const;

        private:
            struct Impl;
            Impl* mImpl;
        };

    } // namespace UtilityAI
} // namespace Dia
