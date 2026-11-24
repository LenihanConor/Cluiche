#include "Modules/InspectorSources/BlackboardInspectorSource.h"

namespace Cluiche { namespace AppFlow {

BlackboardInspectorSource::BlackboardInspectorSource(
    const Dia::Blackboard::BlackboardRegistry& registry)
    : mRegistry(registry)
{
}

Dia::Core::StringCRC BlackboardInspectorSource::GetTopic() const
{
    static const Dia::Core::StringCRC kTopic("blackboard.state");
    return kTopic;
}

Dia::DebugServer::SourcePolicy BlackboardInspectorSource::GetPolicy() const
{
    return { Dia::DebugServer::SourceStrategy::kChangeDetected, 0.0f, 0, 0.0f };
}

Json::Value BlackboardInspectorSource::MakeSlotJson(Dia::Core::StringCRC key,
                                                    const void*          typeTag,
                                                    const void*          data) const
{
    Json::Value slotJson;
    slotJson["key"]  = key.AsChar();
    slotJson["type"] = "unknown";  // no runtime string for type tag in v1

    Json::Value valueJson;
    if (mRegistry.HasSerializer(typeTag))
    {
        mRegistry.Serialize(typeTag, data, valueJson);
    }
    else
    {
        valueJson = "[no serializer]";
    }
    slotJson["value"] = valueJson;
    return slotJson;
}

unsigned int BlackboardInspectorSource::CollectAndHash(Json::Value& payload)
{
    const auto& entries = mRegistry.GetAll();

    unsigned int hash = 0;
    Json::Value boardsArray(Json::arrayValue);

    for (unsigned int e = 0; e < entries.Size(); ++e)
    {
        const Dia::Blackboard::BlackboardEntry& entry = entries[e];
        const Dia::Blackboard::Blackboard*      board = entry.board;

        // --- Build slots array ------------------------------------------------
        Json::Value slotsArray(Json::arrayValue);
        unsigned int slotCount = 0;

        board->VisitSlots([&](Dia::Core::StringCRC key,
                               const void*          typeTag,
                               const void*          data)
        {
            slotsArray.append(MakeSlotJson(key, typeTag, data));
            ++slotCount;
        });

        // --- Build inherited array (nearest ancestor first) -------------------
        Json::Value inheritedArray(Json::arrayValue);
        unsigned int inheritedSlotCount = 0;
        unsigned int ancestorCount = 0;

        for (const Dia::Blackboard::Blackboard* anc = board->GetParent(); anc != nullptr; anc = anc->GetParent())
        {
            Json::Value ancSlots(Json::arrayValue);
            anc->VisitSlots([&](Dia::Core::StringCRC key,
                                 const void*          typeTag,
                                 const void*          data)
            {
                Json::Value slotJson = MakeSlotJson(key, typeTag, data);
                bool shadowed = false;
                for (const Dia::Blackboard::Blackboard* near = board; near != anc; near = near->GetParent())
                {
                    near->VisitSlots([&](Dia::Core::StringCRC k, const void*, const void*)
                    {
                        if (k == key) shadowed = true;
                    });
                    if (shadowed) break;
                }
                slotJson["shadowed"] = shadowed;
                ancSlots.append(slotJson);
                ++inheritedSlotCount;
            });

            Json::Value ancJson;
            ancJson["from"]  = anc->GetName().AsChar();
            ancJson["slots"] = ancSlots;
            inheritedArray.append(ancJson);
            ++ancestorCount;
        }

        const Dia::Blackboard::Blackboard* parent = board->GetParent();

        // --- Build observers array --------------------------------------------
        Json::Value observersArray(Json::arrayValue);
        unsigned int observerCount = 0;

        board->VisitObservers([&](const Dia::Blackboard::IBlackboardObserver* obs)
        {
            observersArray.append(obs->GetId().AsChar());
            ++observerCount;
        });

        // --- Accumulate hash --------------------------------------------------
        hash = HashCombine(hash, static_cast<unsigned int>(entry.id.Value()));
        hash = HashCombine(hash, slotCount);
        hash = HashCombine(hash, observerCount);
        hash = HashCombine(hash, parent ? static_cast<unsigned int>(parent->GetName().Value()) : 0u);
        hash = HashCombine(hash, ancestorCount);
        hash = HashCombine(hash, inheritedSlotCount);

        // --- Build board entry ------------------------------------------------
        Json::Value boardJson;
        boardJson["id"]        = entry.id.AsChar();
        boardJson["label"]     = entry.label ? entry.label : "";
        boardJson["name"]      = board->GetName().AsChar();
        boardJson["parent"]    = parent ? parent->GetName().AsChar() : "";
        boardJson["slots"]     = slotsArray;
        boardJson["inherited"] = inheritedArray;
        boardJson["observers"] = observersArray;

        boardsArray.append(boardJson);
    }

    payload["boards"] = boardsArray;
    return hash;
}

}} // namespace Cluiche::AppFlow
