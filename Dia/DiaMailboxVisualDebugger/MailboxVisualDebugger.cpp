#include "MailboxVisualDebugger.h"

#ifdef DIA_DEBUG

#include <DiaDebugDraw/Domain/DebugGroupAccents.h>
#include <DiaMailbox/Mailbox.h>
#include <stdio.h>

namespace
{
    const Dia::Core::StringCRC kCmdToggle("toggle");
    const Dia::Core::StringCRC kDrawerQueueTable("QueueTable");
    const Dia::Core::StringCRC kDrawerDropAlerts("DropAlerts");
}

namespace Dia::Mailbox
{

MailboxVisualDebugger::MailboxVisualDebugger(const Mailbox& mailbox)
    : mMailbox(mailbox)
{}

Dia::Core::StringCRC MailboxVisualDebugger::GetDomainId()    const { return Dia::Core::StringCRC("mailbox"); }
const char* MailboxVisualDebugger::GetDisplayName()          const { return "Mailbox"; }
const char* MailboxVisualDebugger::GetDescription()          const { return "Mailbox \xe2\x80\x94 per-type queue fill, send/drop/drain counters"; }
Dia::Core::StringCRC MailboxVisualDebugger::GetGroup()       const { return Dia::Core::StringCRC("AIBehavior"); }
Dia::Core::RGBA MailboxVisualDebugger::GetAccentColour()     const { return Dia::VisualDebugger::DebugGroupAccents::kAIBehavior; }

void MailboxVisualDebugger::GetJSONState(Json::Value& out)
{
    const bool queueTableEnabled = mQueueTableEnabled.load();
    const bool dropAlertsEnabled = mDropAlertsEnabled.load();

    // drawers
    Json::Value drawers(Json::arrayValue);
    {
        Json::Value entry(Json::objectValue);
        entry["name"]    = "QueueTable";
        entry["enabled"] = queueTableEnabled;
        drawers.append(entry);
    }
    {
        Json::Value entry(Json::objectValue);
        entry["name"]    = "DropAlerts";
        entry["enabled"] = dropAlertsEnabled;
        drawers.append(entry);
    }
    out["drawers"] = drawers;

    const uint32_t typeCount = mMailbox.GetRegisteredTypeCount();

    // stats — requires a full pass to sum totalDropped
    uint64_t totalDropped = 0;
    for (uint32_t i = 0; i < typeCount; ++i)
    {
        totalDropped += mMailbox.GetTypeStatsByIndex(static_cast<int>(i)).totalDropped;
    }

    Json::Value stats(Json::objectValue);
    stats["typeCount"]    = typeCount;
    stats["totalDropped"] = static_cast<Json::UInt64>(totalDropped);
    out["stats"] = stats;

    // types array — emitted only when QueueTable drawer is enabled
    if (queueTableEnabled)
    {
        Json::Value types(Json::arrayValue);
        for (uint32_t i = 0; i < typeCount; ++i)
        {
            const Mailbox::TypeStats s = mMailbox.GetTypeStatsByIndex(static_cast<int>(i));

            uint32_t fillPct = 0;
            if (s.capacity > 0)
            {
                const uint32_t pct = static_cast<uint32_t>(
                    (static_cast<float>(s.currentCount) / static_cast<float>(s.capacity)) * 100.0f + 0.5f);
                fillPct = pct > 100u ? 100u : pct;
            }

            char typeKeyBuf[12];
            sprintf_s(typeKeyBuf, sizeof(typeKeyBuf), "0x%08X", s.typeKey);

            Json::Value entry(Json::objectValue);
            entry["typeKey"]      = typeKeyBuf;
            entry["capacity"]     = s.capacity;
            entry["currentCount"] = s.currentCount;
            entry["fillPct"]      = fillPct;
            entry["totalSent"]    = static_cast<Json::UInt64>(s.totalSent);
            entry["totalDropped"] = static_cast<Json::UInt64>(s.totalDropped);
            entry["totalDrained"] = static_cast<Json::UInt64>(s.totalDrained);
            entry["hasDrops"]     = (s.totalDropped > 0);
            types.append(entry);
        }
        out["types"] = types;
    }

    // dropAlerts — emitted only when DropAlerts drawer is enabled
    if (dropAlertsEnabled)
    {
        Json::Value alerts(Json::arrayValue);
        for (uint32_t i = 0; i < typeCount; ++i)
        {
            const Mailbox::TypeStats s = mMailbox.GetTypeStatsByIndex(static_cast<int>(i));
            if (s.totalDropped > 0)
            {
                char typeKeyBuf[12];
                sprintf_s(typeKeyBuf, sizeof(typeKeyBuf), "0x%08X", s.typeKey);

                Json::Value entry(Json::objectValue);
                entry["typeKey"]      = typeKeyBuf;
                entry["totalDropped"] = static_cast<Json::UInt64>(s.totalDropped);
                alerts.append(entry);
            }
        }
        out["dropAlerts"] = alerts;
    }
}

void MailboxVisualDebugger::OnCommand(Dia::Core::StringCRC cmd, const Json::Value& args)
{
    if (cmd == kCmdToggle)
    {
        if (!args.isMember("drawer") || !args["drawer"].isString()) return;
        const Dia::Core::StringCRC drawer(args["drawer"].asCString());
        if (drawer == kDrawerQueueTable)
            mQueueTableEnabled = !mQueueTableEnabled.load();
        else if (drawer == kDrawerDropAlerts)
            mDropAlertsEnabled = !mDropAlertsEnabled.load();
    }
    // "setScale" — no-op for panel-only domain
}

} // namespace Dia::Mailbox

#endif // DIA_DEBUG
