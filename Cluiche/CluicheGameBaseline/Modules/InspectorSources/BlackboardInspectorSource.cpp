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

            slotsArray.append(slotJson);
            ++slotCount;
        });

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

        // --- Build board entry ------------------------------------------------
        Json::Value boardJson;
        boardJson["id"]        = entry.id.AsChar();
        boardJson["label"]     = entry.label ? entry.label : "";
        boardJson["slots"]     = slotsArray;
        boardJson["observers"] = observersArray;

        boardsArray.append(boardJson);
    }

    payload["boards"] = boardsArray;
    return hash;
}

}} // namespace Cluiche::AppFlow
