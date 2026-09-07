#include "MessageBusDebugDomain.h"
#ifdef DIA_DEBUG

#include <algorithm>
#include <vector>
#include <cstring>
#include <DiaDebugDraw/Domain/DebugGroupAccents.h>

namespace Dia::MessageBus {

namespace {
    const Dia::Core::StringCRC kCmdSelectTab("selectTab");
    const Dia::Core::StringCRC kCmdSetHistoryWindow("setHistoryWindow");

    const char* RouterLabel(bool sawBroadcast, bool sawEntity) {
        if (sawBroadcast && sawEntity) return "both";
        if (sawBroadcast)              return "broadcast";
        if (sawEntity)                 return "entity";
        return "none";
    }

    const char* PassLabel(Pass pass) {
        return (pass == Pass::Reaction) ? "Reaction" : "Primary";
    }
}

MessageBusDebugDomain::MessageBusDebugDomain(const MessageBusModule& module)
    : mModule(module)
{}

Dia::Core::StringCRC MessageBusDebugDomain::GetDomainId()     const { return Dia::Core::StringCRC("MessageBus"); }
const char*          MessageBusDebugDomain::GetDisplayName()  const { return "Message Bus"; }
const char*          MessageBusDebugDomain::GetDescription()  const { return "Message bus \xe2\x80\x94 routing schema, live tick flow, tick history"; }
Dia::Core::StringCRC MessageBusDebugDomain::GetGroup()         const { return Dia::Core::StringCRC("Systems"); }

Dia::Core::RGBA MessageBusDebugDomain::GetAccentColour() const
{
    // No canonical "Systems" entry exists in DebugGroupAccents (see that
    // file's 8-group list) — MessageBus is core plumbing rather than
    // Physics/Animation/etc., so kCoreDebug ("core / utility domains") is
    // the closest documented fit. Still a DebugGroupAccents constant, never
    // an inline literal, per IDebugDomain::GetAccentColour()'s contract.
    return Dia::VisualDebugger::DebugGroupAccents::kCoreDebug;
}

const char* MessageBusDebugDomain::TabName(Tab tab)
{
    switch (tab)
    {
        case Tab::Schema:  return "schema";
        case Tab::Live:    return "live";
        case Tab::History: return "history";
        default:           return "live";
    }
}

void MessageBusDebugDomain::GetJSONState(Json::Value& out)
{
    // Panel-only domain — no world-space drawers/toggles to report, but
    // still write the minimum keys IDebugDomain::GetJSONState() documents.
    out["drawers"] = Json::Value(Json::arrayValue);
    out["stats"]   = Json::Value(Json::objectValue);

    out["activeTab"]          = TabName(mActiveTab);
    out["historyWindowTicks"] = mHistoryWindowTicks;

    WriteSchemaTab(out);
    WriteLiveTab(out);
    WriteHistoryTab(out);
}

void MessageBusDebugDomain::WriteSchemaTab(Json::Value& out) const
{
    struct Row {
        Dia::Core::StringCRC typeId;
        bool                 sawBroadcastRouter = false;
        bool                 sawEntityRouter    = false;
        std::vector<Dia::Core::StringCRC> producerIds;
        std::vector<Dia::Core::StringCRC> subscriberIds;
    };

    const Bus& bus = mModule.GetBus();

    std::vector<Row> rows;
    bus.ForEachRegisteredType([&](const Bus::RegisteredTypeInfo& info) {
        Row row;
        row.typeId             = info.typeId;
        row.sawBroadcastRouter = info.sawBroadcastRouter;
        row.sawEntityRouter    = info.sawEntityRouter;
        bus.ForEachProducerForType(info.typeId, [&](Dia::Core::StringCRC producerId) {
            row.producerIds.push_back(producerId);
        });
        bus.ForEachSubscriberForType(info.typeId, [&](Dia::Core::StringCRC subscriberId) {
            row.subscriberIds.push_back(subscriberId);
        });
        rows.push_back(row);
    });

    std::sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) {
        return std::strcmp(a.typeId.AsChar(), b.typeId.AsChar()) < 0;
    });

    Json::Value schema(Json::arrayValue);
    for (const Row& row : rows)
    {
        Json::Value entry(Json::objectValue);
        entry["typeId"]   = row.typeId.AsChar();
        entry["routerId"] = RouterLabel(row.sawBroadcastRouter, row.sawEntityRouter);

        Json::Value producerIds(Json::arrayValue);
        for (Dia::Core::StringCRC id : row.producerIds) producerIds.append(id.AsChar());
        entry["producerIds"] = producerIds;

        Json::Value subscriberIds(Json::arrayValue);
        for (Dia::Core::StringCRC id : row.subscriberIds) subscriberIds.append(id.AsChar());
        entry["subscriberIds"] = subscriberIds;

        schema.append(entry);
    }
    out["schema"] = schema;
}

void MessageBusDebugDomain::WriteLiveTab(Json::Value& out) const
{
    const LedgerSnapshot& snapshot = mModule.GetBus().GetLastTickLedger();

    std::vector<const LedgerMessageEntry*> sorted;
    sorted.reserve(snapshot.entries.Size());
    for (uint32_t i = 0; i < snapshot.entries.Size(); ++i)
    {
        sorted.push_back(&snapshot.entries[i]);
    }
    std::sort(sorted.begin(), sorted.end(), [](const LedgerMessageEntry* a, const LedgerMessageEntry* b) {
        return a->count > b->count;
    });

    uint64_t totalMessages   = 0;
    uint64_t totalDeliveries = 0;

    Json::Value entries(Json::arrayValue);
    for (const LedgerMessageEntry* e : sorted)
    {
        totalMessages   += e->count;
        totalDeliveries += e->deliveries;

        Json::Value entry(Json::objectValue);
        entry["typeId"]     = e->typeId.AsChar();
        entry["routerId"]   = e->routerId.AsChar();
        entry["count"]      = e->count;
        entry["deliveries"] = e->deliveries;
        entry["pass"]       = PassLabel(e->pass);
        entries.append(entry);
    }

    Json::Value live(Json::objectValue);
    live["tickIndex"]       = static_cast<Json::UInt64>(snapshot.tickIndex);
    live["totalMessages"]   = static_cast<Json::UInt64>(totalMessages);
    live["totalDeliveries"] = static_cast<Json::UInt64>(totalDeliveries);
    live["entries"]         = entries;
    out["live"] = live;
}

void MessageBusDebugDomain::WriteHistoryTab(Json::Value& out) const
{
    const LedgerHistory& history = mModule.GetLedgerHistory();
    const uint32_t total  = history.Count();
    const uint32_t window = mHistoryWindowTicks;
    const uint32_t skip   = (total > window) ? (total - window) : 0;

    Json::Value historyArray(Json::arrayValue);
    uint32_t index = 0;
    history.ForEachSnapshot([&](const LedgerSnapshot& snapshot) {
        const uint32_t thisIndex = index++;
        if (thisIndex < skip) return;

        uint64_t count      = 0;
        uint64_t deliveries = 0;
        for (uint32_t i = 0; i < snapshot.entries.Size(); ++i)
        {
            count      += snapshot.entries[i].count;
            deliveries += snapshot.entries[i].deliveries;
        }

        Json::Value entry(Json::objectValue);
        entry["tickIndex"]  = static_cast<Json::UInt64>(snapshot.tickIndex);
        entry["count"]      = static_cast<Json::UInt64>(count);
        entry["deliveries"] = static_cast<Json::UInt64>(deliveries);
        entry["dropped"]    = snapshot.droppedCount;
        historyArray.append(entry);
    });
    out["history"] = historyArray;
}

void MessageBusDebugDomain::OnCommand(Dia::Core::StringCRC cmd, const Json::Value& args)
{
    if (cmd == kCmdSelectTab)
    {
        if (!args.isMember("tab") || !args["tab"].isString()) return;
        const std::string tab = args["tab"].asString();
        if (tab == "schema")       mActiveTab = Tab::Schema;
        else if (tab == "live")    mActiveTab = Tab::Live;
        else if (tab == "history") mActiveTab = Tab::History;
        return;
    }

    if (cmd == kCmdSetHistoryWindow)
    {
        if (!args.isMember("ticks") || !args["ticks"].isInt()) return;
        const int ticks = args["ticks"].asInt();
        if (ticks < 1 || ticks > 3600) return;
        mHistoryWindowTicks = static_cast<uint16_t>(ticks);
    }
}

} // namespace Dia::MessageBus

#endif // DIA_DEBUG
