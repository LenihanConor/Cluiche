#include "Modules/InspectorSources/EconomyModifiersSource.h"
#include <DiaEconomy/EconomySystem.h>
#include <DiaEconomy/EconomyInstance.h>
#include <DiaEconomy/EconomySchema.h>
#include <DiaCore/Json/external/json/json.h>

namespace Cluiche { namespace AppFlow {

EconomyModifiersSource::EconomyModifiersSource(Dia::Economy::EconomySystem& system)
    : mSystem(system)
{
}

Dia::Core::StringCRC EconomyModifiersSource::GetTopic() const
{
    static const Dia::Core::StringCRC kTopic("economy.modifiers");
    return kTopic;
}

Dia::DebugServer::SourcePolicy EconomyModifiersSource::GetPolicy() const
{
    return { Dia::DebugServer::SourceStrategy::kChangeDetected, 0.0f, 0, 0.0f };
}

unsigned int EconomyModifiersSource::CollectAndHash(Json::Value& payload)
{
    ++mFrame;
    payload["frame"] = mFrame;

    unsigned int hash = 0;
    hash = HashCombine(hash, mFrame);

    const unsigned int instanceCount = mSystem.GetInstanceCount();
    hash = HashCombine(hash, instanceCount);

    Json::Value instancesArray(Json::arrayValue);

    for (unsigned int i = 0; i < instanceCount; ++i)
    {
        const Dia::Economy::EconomyInstance& inst = mSystem.GetInstanceByIndex(i);
        const Dia::Economy::EconomySchema*   schema = inst.GetSchema();

        Json::Value instanceJson;
        instanceJson["id"] = inst.GetInstanceName().AsChar();

        Json::Value resourcesArray(Json::arrayValue);

        const unsigned int resourceCount = schema->GetResourceCount();
        for (unsigned int r = 0; r < resourceCount; ++r)
        {
            const Dia::Economy::ResourceDefinition& resDef = schema->GetResourceByIndex(r);

            Dia::Economy::ModifierStackEntry entries[32];
            const unsigned int count = mSystem.GetModifierStack(
                inst,
                resDef.resource_name,
                entries,
                32u);

            if (count == 0)
                continue;

            Json::Value resourceJson;
            resourceJson["name"] = resDef.resource_name.AsChar();

            Json::Value modifiersArray(Json::arrayValue);

            for (unsigned int m = 0; m < count; ++m)
            {
                const Dia::Economy::ModifierStackEntry& entry = entries[m];
                const Dia::Economy::ModifierDef* mod = entry.modifier;

                Json::Value modJson;
                modJson["type"]   = mod->operation.AsChar();
                modJson["value"]  = mod->value;
                modJson["source"] = mod->modifier_name.AsChar();

                if (mod->when_condition[0] == '\0')
                    modJson["condition"] = Json::Value(Json::nullValue);
                else
                    modJson["condition"] = mod->when_condition;

                modJson["active"] = entry.active;

                // Accumulate hash: modifier name + active flag
                hash = HashCombine(hash, mod->modifier_name.Value());
                hash = HashCombine(hash, entry.active ? 1u : 0u);

                modifiersArray.append(modJson);
            }

            resourceJson["modifiers"] = modifiersArray;
            resourcesArray.append(resourceJson);
        }

        instanceJson["resources"] = resourcesArray;
        instancesArray.append(instanceJson);
    }

    payload["instances"] = instancesArray;
    return hash;
}

}} // namespace Cluiche::AppFlow
