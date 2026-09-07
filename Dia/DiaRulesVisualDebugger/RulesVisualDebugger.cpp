#include "RulesVisualDebugger.h"

#ifdef DIA_DEBUG

#include <DiaDebugDraw/Domain/DebugGroupAccents.h>
#include <DiaRules/RuleSetComponent.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

namespace Dia::Rules
{

namespace
{
    const Dia::Core::StringCRC kCmdToggle("toggle");
    const Dia::Core::StringCRC kDrawerFireLog("FireLog");
}

RulesVisualDebugger::RulesVisualDebugger(const RuleSetComponent& component)
    : mComponent(component)
{}

Dia::Core::StringCRC RulesVisualDebugger::GetDomainId()     const { return Dia::Core::StringCRC("rules"); }
const char* RulesVisualDebugger::GetDisplayName()           const { return "Rules"; }
const char* RulesVisualDebugger::GetDescription()           const { return "Rule evaluation \xe2\x80\x94 fired rules, guard results, dispatched actions"; }
Dia::Core::StringCRC RulesVisualDebugger::GetGroup()        const { return Dia::Core::StringCRC("AIBehavior"); }
Dia::Core::RGBA RulesVisualDebugger::GetAccentColour()      const { return Dia::VisualDebugger::DebugGroupAccents::kAIBehavior; }

void RulesVisualDebugger::GetJSONState(Json::Value& out)
{
    const bool enabled = mFireLogEnabled.load();

    // drawers[]
    Json::Value drawers(Json::arrayValue);
    {
        Json::Value entry(Json::objectValue);
        entry["name"]    = "FireLog";
        entry["enabled"] = enabled;
        drawers.append(entry);
    }
    out["drawers"] = drawers;

    const RuleSet* ruleSet   = mComponent.GetRuleSet();
    const int      ruleCount = (ruleSet != nullptr) ? ruleSet->GetRuleCount() : 0;

    Dia::Core::Containers::DynamicArrayC<RuleSet::RuleFireEntry, 16> fireReport;
    const int fired = (ruleSet != nullptr) ? ruleSet->GetLastFireReport(fireReport) : 0;

    // stats{}
    Json::Value stats(Json::objectValue);
    stats["ruleCount"] = ruleCount;
    stats["fired"]     = fired;
    out["stats"] = stats;

    if (!enabled)
        return;

    // rules[] — full inventory cross-referenced against fire report.
    // Fire report entries are in definition order (same as rule order).
    // Use a "claimed" array so unnamed (kZero) rules are matched positionally.
    bool claimed[16] = {};

    Json::Value rules(Json::arrayValue);
    for (int i = 0; i < ruleCount; ++i)
    {
        const RuleDef* rule = ruleSet->GetRuleAt(i);
        if (rule == nullptr) continue;

        // Find first unclaimed fire report entry matching this rule's ID.
        int matchIdx = -1;
        for (int j = 0; j < fired && j < 16; ++j)
        {
            if (!claimed[j] && fireReport[j].ruleId.Value() == rule->id.Value())
            {
                matchIdx = j;
                break;
            }
        }
        const bool ruleFired = (matchIdx >= 0);
        if (ruleFired) claimed[matchIdx] = true;

        Json::Value ruleEntry(Json::objectValue);
        const bool  isUnnamed = (rule->id.Value() == Dia::Core::StringCRC::kZero.Value());
        ruleEntry["id"]    = isUnnamed ? "(unnamed)" : rule->id.AsChar();
        ruleEntry["fired"] = ruleFired;

        Json::Value actions(Json::arrayValue);
        if (ruleFired)
        {
            for (unsigned int k = 0; k < fireReport[matchIdx].actions.Size(); ++k)
                actions.append(fireReport[matchIdx].actions[k].AsChar());
        }
        else
        {
            for (unsigned int k = 0; k < rule->actions.Size(); ++k)
                actions.append(rule->actions[k].AsChar());
        }
        ruleEntry["actions"] = actions;
        rules.append(ruleEntry);
    }
    out["rules"] = rules;
}

void RulesVisualDebugger::OnCommand(Dia::Core::StringCRC cmd, const Json::Value& args)
{
    if (cmd == kCmdToggle)
    {
        if (!args.isMember("drawer") || !args["drawer"].isString()) return;
        if (Dia::Core::StringCRC(args["drawer"].asCString()) == kDrawerFireLog)
            mFireLogEnabled = !mFireLogEnabled.load();
    }
    // "setScale" — no-op for panel-only domain
}

} // namespace Dia::Rules

#endif // DIA_DEBUG
