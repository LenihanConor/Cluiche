#include <DiaRules/RuleSet.h>
#include <DiaRules/RuleDef.h>

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Json/external/json/json.h>

#include <vector>
#include <unordered_set>

namespace Dia
{
    namespace Rules
    {
        // -----------------------------------------------------------------------
        // Pimpl struct
        // -----------------------------------------------------------------------
        struct RuleSet::Impl
        {
            std::vector<RuleDef> rules;

#ifdef DIA_DEBUG
            mutable std::vector<RuleFireEntry> lastFireReport;
#endif // DIA_DEBUG
        };

        // -----------------------------------------------------------------------
        // Constructor / Destructor
        // -----------------------------------------------------------------------
        RuleSet::RuleSet()
            : mImpl(new Impl())
        {
        }

        RuleSet::~RuleSet()
        {
            delete mImpl;
        }

        RuleSet::RuleSet(RuleSet&& other) noexcept
            : mImpl(other.mImpl)
        {
            other.mImpl = nullptr;
        }

        RuleSet& RuleSet::operator=(RuleSet&& other) noexcept
        {
            if (this != &other)
            {
                delete mImpl;
                mImpl = other.mImpl;
                other.mImpl = nullptr;
            }
            return *this;
        }

        // -----------------------------------------------------------------------
        // LoadFromJson
        // -----------------------------------------------------------------------
        RuleSet RuleSet::LoadFromJson(const Json::Value& root)
        {
            RuleSet rs;

            const Json::Value& rulesArray = root["rules"];
            if (!rulesArray.isArray())
                return rs;

            for (Json::ArrayIndex i = 0; i < rulesArray.size(); ++i)
            {
                const Json::Value& ruleNode = rulesArray[i];

                RuleDef def;

                // Non-object entries (e.g. numbers, strings) produce an empty RuleDef
                if (!ruleNode.isObject())
                {
                    rs.mImpl->rules.push_back(std::move(def));
                    continue;
                }

                // Optional id
                if (ruleNode.isMember("id") && ruleNode["id"].isString())
                {
                    const std::string& idStr = ruleNode["id"].asString();
                    if (!idStr.empty())
                        def.id = Dia::Core::StringCRC(idStr.c_str());
                }

                // Guard — use ConditionExpr::LoadFromJson
                if (ruleNode.isMember("guard"))
                {
                    Dia::Core::Containers::DynamicArrayC<const char*, 32> loadErrors;
                    def.guard = Dia::Condition::ConditionExpr::LoadFromJson(ruleNode["guard"], loadErrors);
                }

                // Actions array
                if (ruleNode.isMember("actions") && ruleNode["actions"].isArray())
                {
                    const Json::Value& actionsArray = ruleNode["actions"];
                    for (Json::ArrayIndex j = 0; j < actionsArray.size() && !def.actions.IsFull(); ++j)
                    {
                        if (actionsArray[j].isString())
                        {
                            const std::string& actionStr = actionsArray[j].asString();
                            if (!actionStr.empty())
                                def.actions.Add(Dia::Core::StringCRC(actionStr.c_str()));
                        }
                    }
                }

                rs.mImpl->rules.push_back(std::move(def));
            }

            return rs;
        }

        // -----------------------------------------------------------------------
        // Validate
        // -----------------------------------------------------------------------
        bool RuleSet::Validate(Dia::Core::Containers::DynamicArrayC<const char*, 32>& outErrors) const
        {
            if (!mImpl) return true;  // moved-from / empty — treat as valid

            bool valid = true;

            // Check for empty action lists
            for (const RuleDef& rule : mImpl->rules)
            {
                if (rule.actions.Size() == 0)
                {
                    if (!outErrors.IsFull())
                        outErrors.Add("RuleDef has empty actions list");
                    valid = false;
                }
            }

            // Check for duplicate named rule IDs (unnamed rules use kZero — allowed to repeat)
            std::unordered_set<unsigned int> seen;
            for (const RuleDef& rule : mImpl->rules)
            {
                // kZero means unnamed — skip duplicate check
                if (rule.id.Value() == Dia::Core::StringCRC::kZero.Value())
                    continue;

                if (!seen.insert(rule.id.Value()).second)
                {
                    if (!outErrors.IsFull())
                        outErrors.Add("RuleSet has duplicate rule ID");
                    valid = false;
                }
            }

            return valid;
        }

        // -----------------------------------------------------------------------
        // Evaluate
        // -----------------------------------------------------------------------
        int RuleSet::Evaluate(Dia::Condition::IConditionContext& context,
                              const RuleActionRegistry& registry,
                              void* actionContext) const
        {
            if (!mImpl) return 0;

            int fired = 0;

#ifdef DIA_DEBUG
            mImpl->lastFireReport.clear();
#endif // DIA_DEBUG

            for (const RuleDef& rule : mImpl->rules)
            {
                if (rule.guard.Evaluate(context))
                {
                    // Fire all actions in this rule
                    for (unsigned int i = 0; i < rule.actions.Size(); ++i)
                    {
                        RuleActionFn fn = registry.Find(rule.actions[i]);
                        if (fn != nullptr)
                            fn(actionContext);
                    }

#ifdef DIA_DEBUG
                    RuleFireEntry entry;
                    entry.ruleId = rule.id;
                    for (unsigned int i = 0; i < rule.actions.Size() && !entry.actions.IsFull(); ++i)
                        entry.actions.Add(rule.actions[i]);
                    mImpl->lastFireReport.push_back(std::move(entry));
#endif // DIA_DEBUG

                    ++fired;
                }
            }

            return fired;
        }

        // -----------------------------------------------------------------------
        // GetRuleCount
        // -----------------------------------------------------------------------
        int RuleSet::GetRuleCount() const
        {
            if (!mImpl) return 0;
            return static_cast<int>(mImpl->rules.size());
        }

        // -----------------------------------------------------------------------
        // GetRuleAt
        // -----------------------------------------------------------------------
        const RuleDef* RuleSet::GetRuleAt(int index) const
        {
            if (!mImpl || index < 0 || index >= static_cast<int>(mImpl->rules.size()))
                return nullptr;
            return &mImpl->rules[static_cast<std::size_t>(index)];
        }

#ifdef DIA_DEBUG
        // -----------------------------------------------------------------------
        // GetLastFireReport
        // -----------------------------------------------------------------------
        int RuleSet::GetLastFireReport(
            Dia::Core::Containers::DynamicArrayC<RuleFireEntry, 16>& outEntries) const
        {
            if (!mImpl) return 0;

            int count = 0;
            for (const RuleFireEntry& entry : mImpl->lastFireReport)
            {
                if (!outEntries.IsFull())
                    outEntries.Add(entry);
                ++count;
            }
            return count;
        }
#endif // DIA_DEBUG

    } // namespace Rules
} // namespace Dia
