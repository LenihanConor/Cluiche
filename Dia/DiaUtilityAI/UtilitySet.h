#pragma once

#include <DiaUtilityAI/ActionDef.h>
#include <DiaUtilityAI/GroupConsiderationContext.h>
#include <DiaCondition/IConditionContext.h>
#include <DiaCondition/ConditionRegistry.h>
#include <DiaRules/RuleActionRegistry.h>
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

            // JSON loading. See class doc for schema.
            static UtilitySet LoadFromJson(const Json::Value& root);

            // Validate: non-empty scorers lists, valid action IDs.
            // Returns false + populates outErrors on failure.
            bool Validate(
                const Dia::Condition::ConditionRegistry& registry,
                Dia::Core::Containers::DynamicArrayC<const char*, 32>& outErrors) const;

            int GetActionCount() const;

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
