#pragma once

#include <DiaRules/RuleActionRegistry.h>
#include <DiaCondition/IConditionContext.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Json/external/json/json.h>

namespace Dia
{
    namespace Rules
    {
        //-------------------------------------------------------------------------------------------
        // RuleSet
        //
        // JSON-loadable, immutable collection of RuleDef entries.  Evaluates all rules whose guard
        // passes and fires their registered actions (all-matching; no first-match stop).
        //
        // SD-001: All-matching evaluation — every rule whose guard passes fires.
        // SD-002: No priority ordering in v1 — rules fire in definition order.
        // SD-005: Immutable after LoadFromJson().
        // SD-006: Evaluate() takes IConditionContext&, not BlackboardInstance& directly.
        // SD-007: Evaluate() returns count of rules fired (0 = no match).
        // SD-008: Caller owns JSON parsing; LoadFromJson takes Json::Value&.
        // PD-004: No STL in public APIs.
        // AD-003: Dia::Rules:: namespace.
        //-------------------------------------------------------------------------------------------
        class RuleSet
        {
        public:
            RuleSet();
            ~RuleSet();

            // Move-only (internal RuleDef storage holds move-only ConditionExpr)
            RuleSet(RuleSet&&) noexcept;
            RuleSet& operator=(RuleSet&&) noexcept;

            RuleSet(const RuleSet&) = delete;
            RuleSet& operator=(const RuleSet&) = delete;

            // Load from JSON. Caller owns JSON parsing.
            // JSON format:
            // { "rules": [ { "id": "...", "guard": <ConditionExpr>, "actions": ["ActionA", ...] } ] }
            static RuleSet LoadFromJson(const Json::Value& root);

            // Validate: non-empty action lists, no duplicate named rule IDs (unnamed rules OK).
            // Returns false + populates outErrors on failure.
            bool Validate(Dia::Core::Containers::DynamicArrayC<const char*, 32>& outErrors) const;

            // Evaluate all rules. Fires every action for every rule whose guard passes.
            // actionContext is passed opaquely to each RuleActionFn.
            // Returns count of rules that fired (0 = no match).
            int Evaluate(Dia::Condition::IConditionContext& context,
                         const RuleActionRegistry& registry,
                         void* actionContext) const;

            int GetRuleCount() const;

        private:
            // Pimpl to keep STL (std::vector<RuleDef>) out of the public header.
            struct Impl;
            Impl* mImpl;
        };

    } // namespace Rules
} // namespace Dia
