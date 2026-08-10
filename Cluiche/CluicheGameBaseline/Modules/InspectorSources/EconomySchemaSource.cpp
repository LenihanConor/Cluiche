#include "Modules/InspectorSources/EconomySchemaSource.h"

#include <DiaDebugServer/InspectorDataSourceBase.h>
#include <DiaEconomy/EconomySchema.h>
#include <DiaEconomy/EconomySystem.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>

namespace Cluiche { namespace AppFlow {

EconomySchemaSource::EconomySchemaSource(
    const Dia::Economy::EconomySchema& schema,
    const Dia::Economy::EconomySystem& system)
    : mSchema(schema)
    , mSystem(system)
{
}

Dia::Core::StringCRC EconomySchemaSource::GetTopic() const
{
    static const Dia::Core::StringCRC kTopic("economy.schema");
    return kTopic;
}

Dia::DebugServer::SourcePolicy EconomySchemaSource::GetPolicy() const
{
    return { Dia::DebugServer::SourceStrategy::kChangeDetected, 0.0f, 0, 0.0f };
}

unsigned int EconomySchemaSource::CollectAndHash(Json::Value& payload)
{
    // --- resources -----------------------------------------------------------
    Json::Value resourcesArray(Json::arrayValue);
    const unsigned int resourceCount = mSchema.GetResourceCount();

    for (unsigned int r = 0; r < resourceCount; ++r)
    {
        const Dia::Economy::ResourceDefinition& def = mSchema.GetResourceByIndex(r);

        const bool isDerived = mSystem.IsDerivedResource(def.resource_name);

        // Find first income rule whose resource_name matches this resource.
        const char* incomeRuleStr = "";
        const unsigned int ruleCount = mSchema.GetIncomeRuleCount();
        for (unsigned int i = 0; i < ruleCount; ++i)
        {
            const Dia::Economy::IncomeRule& rule = mSchema.GetIncomeRuleByIndex(i);
            if (rule.resource_name.Value() == def.resource_name.Value())
            {
                incomeRuleStr = rule.rule_name.AsChar();
                break;
            }
        }

        Json::Value resJson;
        resJson["name"]         = def.resource_name.AsChar();
        resJson["type"]         = isDerived ? "derived" : "base";
        resJson["base_cap"]     = isDerived ? -1.0f : def.maximum_value;
        resJson["income_rule"]  = incomeRuleStr;

        resourcesArray.append(resJson);
    }

    payload["resources"] = resourcesArray;

    // --- cost_table ----------------------------------------------------------
    Json::Value costTableArray(Json::arrayValue);
    const unsigned int tableCount = mSchema.GetCostTableCount();

    for (unsigned int t = 0; t < tableCount; ++t)
    {
        const Dia::Economy::CostTableDef& table = mSchema.GetCostTableByIndex(t);
        const unsigned int entryCount = table.entries.Size();

        for (unsigned int e = 0; e < entryCount; ++e)
        {
            const Dia::Economy::CostEntry& entry = table.entries[e];

            Json::Value rowJson;
            rowJson["action"] = entry.resource_name.AsChar();

            // Add a cost column for each entry in this table row.
            // The spec cost_table format has one row per action with all
            // cost resources as columns.  Since entries are flat (one action
            // per CostEntry), each entry becomes its own row keyed by
            // resource_name, and the cost value is its single cost column.
            rowJson[entry.resource_name.AsChar()] = entry.cost;

            costTableArray.append(rowJson);
        }
    }

    payload["cost_table"] = costTableArray;

    // --- hash ----------------------------------------------------------------
    // The schema is immutable at runtime (v1), so any non-zero stable hash
    // is sufficient.  Combine resource count, table count, and schema name CRC.
    unsigned int hash = 0;
    hash = HashCombine(hash, resourceCount);
    hash = HashCombine(hash, tableCount);
    hash = HashCombine(hash, mSchema.GetSchemaName().Value());

    return hash;
}

}} // namespace Cluiche::AppFlow
