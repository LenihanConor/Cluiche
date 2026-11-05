// S-1 Terrain Generator: TerrainRules table, loader and serialiser.
#include <Shared/WorldGeneration/TerrainRules.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <set>
#include <string>

namespace CoW {
namespace WorldGeneration {

namespace {

const RuleDef kRuleTable[kRuleCount] =
{
#define TERRAIN_RULE_ROW(name, def, lo, hi) { #name, def, lo, hi },
    TERRAIN_RULES_TABLE(TERRAIN_RULE_ROW)
#undef TERRAIN_RULE_ROW
};

constexpr const char* kRulesVersionKey = "rulesVersion";

void AddWarning(RuleWarnings& warnings, const char* prefix, const char* key)
{
    if (warnings.IsFull())
    {
        return;
    }
    if (warnings.Size() == warnings.Capacity() - 1)
    {
        warnings.Add(Dia::Core::StringCRC("warnings:truncated"));
        return;
    }
    char text[Dia::Core::StringCRC::kStringLength];
    std::snprintf(text, sizeof(text), "%s:%s", prefix, key);
    warnings.Add(Dia::Core::StringCRC(text));
}

} // namespace

const RuleDef& GetRuleDef(uint32_t index) { return kRuleTable[index]; }
const RuleDef& GetRuleDef(Rule r) { return kRuleTable[static_cast<uint32_t>(r)]; }

TerrainRules TerrainRules::Defaults()
{
    TerrainRules r;
    for (uint32_t i = 0; i < kRuleCount; ++i)
    {
        r.values[i] = kRuleTable[i].defaultValue;
    }
    return r;
}

bool TerrainRules::LoadFromJson(const Json::Value& v, RuleWarnings& warnings)
{
    *this = Defaults();
    if (!v.isObject())
    {
        degraded = true;
        AddWarning(warnings, "invalid", "root");
        return false;
    }

    if (v.isMember(kRulesVersionKey) && v[kRulesVersionKey].isUInt())
    {
        rulesVersion = v[kRulesVersionKey].asUInt();
    }
    else
    {
        AddWarning(warnings, v.isMember(kRulesVersionKey) ? "invalid" : "missing", kRulesVersionKey);
    }

    for (uint32_t i = 0; i < kRuleCount; ++i)
    {
        const RuleDef& def = kRuleTable[i];
        if (!v.isMember(def.key))
        {
            AddWarning(warnings, "missing", def.key);
            continue;
        }
        const Json::Value& jv = v[def.key];
        if (!jv.isNumeric())
        {
            AddWarning(warnings, "invalid", def.key);
            degraded = true;
            continue;
        }
        const double d = jv.asDouble();
        if (!std::isfinite(d))
        {
            AddWarning(warnings, "range", def.key);
            degraded = true;
            continue;
        }
        float f = static_cast<float>(d);
        if (f < def.minValue || f > def.maxValue)
        {
            f = f < def.minValue ? def.minValue : def.maxValue;
            AddWarning(warnings, "range", def.key);
            degraded = true;
        }
        values[i] = f;
    }

    std::set<std::string> known;
    known.insert(kRulesVersionKey);
    for (uint32_t i = 0; i < kRuleCount; ++i)
    {
        known.insert(kRuleTable[i].key);
    }
    for (const std::string& name : v.getMemberNames())
    {
        if (known.find(name) == known.end())
        {
            AddWarning(warnings, "unknown", name.c_str());
        }
    }
    return true;
}

Json::Value TerrainRules::ToJson() const
{
    Json::Value root(Json::objectValue);
    root[kRulesVersionKey] = Json::Value(rulesVersion);
    for (uint32_t i = 0; i < kRuleCount; ++i)
    {
        // Short decimal so 0.02f is written as 0.02, not 0.019999999552965164.
        char text[32];
        std::snprintf(text, sizeof(text), "%.7g", static_cast<double>(values[i]));
        root[kRuleTable[i].key] = Json::Value(std::strtod(text, nullptr));
    }
    return root;
}

} // namespace WorldGeneration
} // namespace CoW
