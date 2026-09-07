#include "CalloutRegistryDebugger.h"
#ifdef DIA_DEBUG

#include "CalloutRadiiDrawer.h"
#include <DiaDebugDraw/Domain/DebugGroupAccents.h>
#include <DiaVisualDebugger/DebugLayerManager.h>
#include <DiaAICallout/CalloutRegistry.h>
#include <stdio.h>

namespace
{
    const Dia::Core::StringCRC kCmdToggle("toggle");
    const Dia::Core::StringCRC kDrawerRadii("CalloutRadii");
}

namespace Dia::AICalloutVisualDebugger {

CalloutRegistryDebugger::CalloutRegistryDebugger(const Dia::AICallout::CalloutRegistry& registry)
    : mRegistry(registry)
{
    mRadiiDrawer = std::make_unique<CalloutRadiiDrawer>(registry);
}

CalloutRegistryDebugger::~CalloutRegistryDebugger() = default;

Dia::Core::StringCRC CalloutRegistryDebugger::GetDomainId()    const { return Dia::Core::StringCRC("aicallout"); }
const char* CalloutRegistryDebugger::GetDisplayName()          const { return "AI Callout"; }
const char* CalloutRegistryDebugger::GetDescription()          const { return "AI callout board \xe2\x80\x94 live callouts, radii, claim state"; }
Dia::Core::StringCRC CalloutRegistryDebugger::GetGroup()       const { return Dia::Core::StringCRC("AIBehavior"); }
Dia::Core::RGBA CalloutRegistryDebugger::GetAccentColour()     const { return Dia::VisualDebugger::DebugGroupAccents::kAIBehavior; }

void CalloutRegistryDebugger::Register(Dia::Debug::DebugLayerManager& mgr)
{
    mRadiiDrawer->SetEnabled(mRadiiEnabled.load());
    mgr.Register(mRadiiDrawer.get(), 50);
}

void CalloutRegistryDebugger::Unregister(Dia::Debug::DebugLayerManager& mgr)
{
    mgr.Unregister(mRadiiDrawer->GetLayerName());
}

Dia::Debug::IVisualDebugger* CalloutRegistryDebugger::GetDrawer(int index)
{
    return (index == 0) ? mRadiiDrawer.get() : nullptr;
}

void CalloutRegistryDebugger::GetJSONState(Json::Value& out)
{
    const bool radiiEnabled = mRadiiEnabled.load();

    Json::Value drawers(Json::arrayValue);
    {
        Json::Value entry(Json::objectValue);
        entry["name"]    = "CalloutRadii";
        entry["enabled"] = radiiEnabled;
        drawers.append(entry);
    }
    out["drawers"] = drawers;

    int liveCount    = 0;
    int claimedCount = 0;
    for (uint32_t i = 0u; i < Dia::AICallout::CalloutRegistryData::kMaxCallouts; ++i)
    {
        const Dia::AICallout::CalloutSlot& slot = mRegistry.mSlots[i];
        if (!slot.live) continue;
        ++liveCount;
        if (slot.claimed) ++claimedCount;
    }

    Json::Value stats(Json::objectValue);
    stats["liveCount"]    = liveCount;
    stats["claimedCount"] = claimedCount;
    out["stats"] = stats;

    Json::Value callouts(Json::arrayValue);
    for (uint32_t i = 0u; i < Dia::AICallout::CalloutRegistryData::kMaxCallouts; ++i)
    {
        const Dia::AICallout::CalloutSlot& slot = mRegistry.mSlots[i];
        if (!slot.live) continue;

        char kindBuf[12];
        char factionBuf[12];
        sprintf_s(kindBuf,    sizeof(kindBuf),    "0x%08X", slot.callout.kind.Value());
        sprintf_s(factionBuf, sizeof(factionBuf), "0x%08X", slot.callout.faction.Value());

        Json::Value entry(Json::objectValue);
        entry["kind"]    = kindBuf;
        entry["x"]       = slot.callout.position.x;
        entry["y"]       = slot.callout.position.y;
        entry["radius"]  = slot.callout.radius;
        entry["faction"] = factionBuf;
        entry["ttl"]     = slot.callout.ttl;
        entry["claimed"] = slot.claimed;
        callouts.append(entry);
    }
    out["callouts"] = callouts;
}

void CalloutRegistryDebugger::OnCommand(Dia::Core::StringCRC cmd, const Json::Value& args)
{
    if (cmd == kCmdToggle)
    {
        if (!args.isMember("drawer") || !args["drawer"].isString()) return;
        const Dia::Core::StringCRC drawer(args["drawer"].asCString());
        if (drawer == kDrawerRadii)
        {
            mRadiiEnabled = !mRadiiEnabled.load();
            if (mRadiiDrawer) mRadiiDrawer->SetEnabled(mRadiiEnabled.load());
        }
    }
}

} // namespace Dia::AICalloutVisualDebugger

#endif // DIA_DEBUG
