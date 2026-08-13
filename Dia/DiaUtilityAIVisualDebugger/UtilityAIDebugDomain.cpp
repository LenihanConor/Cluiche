#include "UtilityAIDebugDomain.h"

#ifdef DIA_DEBUG

#include "UtilityScoreDrawer.h"

#include <DiaVisualDebugger/DebugLayerManager.h>
#include <DiaVisualDebugger/Domain/DebugGroupAccents.h>

namespace Dia::UtilityAI
{

namespace
{
    const char* const kDrawerLabels[UtilityAIDebugDomain::kDrawerCount] =
    {
        "ScoreTable",
    };

    const int kDrawerPriorities[UtilityAIDebugDomain::kDrawerCount] =
    {
        45
    };

    const Dia::Core::StringCRC kStageTag("UtilityAI");
    const Dia::Core::StringCRC kCmdToggle("toggle");
    const Dia::Core::StringCRC kCmdSetScale("setScale");
    const Dia::Core::StringCRC kScaleKeyDebugScale("debugScale");
}

UtilityAIDebugDomain::UtilityAIDebugDomain(const UtilitySet& utilitySet)
    : mUtilitySet(utilitySet)
{}

UtilityAIDebugDomain::~UtilityAIDebugDomain() = default;

// Identity
Dia::Core::StringCRC UtilityAIDebugDomain::GetDomainId() const  { return Dia::Core::StringCRC("UtilityAI"); }
const char* UtilityAIDebugDomain::GetDisplayName() const        { return "UtilityAI"; }
const char* UtilityAIDebugDomain::GetDescription() const        { return "Utility AI — per-action score table"; }
Dia::Core::StringCRC UtilityAIDebugDomain::GetGroup() const     { return Dia::Core::StringCRC("AIBehavior"); }
Dia::Core::RGBA UtilityAIDebugDomain::GetAccentColour() const   { return Dia::VisualDebugger::DebugGroupAccents::kAIBehavior; }

// Lifecycle
void UtilityAIDebugDomain::Register(Dia::Debug::DebugLayerManager& mgr)
{
    if (mLayerManager != nullptr) return;
    mScoreDrawer = std::make_unique<UtilityScoreDrawer>(mUtilitySet);
    for (int i = 0; i < kDrawerCount; ++i)
        mgr.Register(GetDrawer(i), kDrawerPriorities[i], kStageTag);
    mLayerManager = &mgr;
}

void UtilityAIDebugDomain::Unregister(Dia::Debug::DebugLayerManager& mgr)
{
    for (int i = 0; i < kDrawerCount; ++i)
        if (Dia::Debug::IVisualDebugger* d = GetDrawer(i)) mgr.Unregister(d->GetLayerName());
    mScoreDrawer.reset();
    mLayerManager = nullptr;
}

// Panel bridge
void UtilityAIDebugDomain::GetJSONState(Json::Value& out)
{
    Json::Value drawers(Json::arrayValue);
    for (int i = 0; i < kDrawerCount; ++i)
    {
        const Dia::Debug::IVisualDebugger* drawer = GetDrawer(i);
        Json::Value entry(Json::objectValue);
        entry["name"]    = kDrawerLabels[i];
        entry["enabled"] = (mLayerManager != nullptr && drawer != nullptr)
                         ? mLayerManager->IsLayerEnabled(drawer->GetLayerName()) : false;
        drawers.append(entry);
    }
    out["drawers"] = drawers;
    out["stats"]   = Json::Value(Json::objectValue);
}

void UtilityAIDebugDomain::OnCommand(Dia::Core::StringCRC cmd, const Json::Value& args)
{
    if (mLayerManager == nullptr) return;
    if (cmd == kCmdToggle)
    {
        if (!args.isMember("drawer") || !args["drawer"].isString()) return;
        const Dia::Core::StringCRC layerName = ResolveLayerName(args["drawer"].asCString());
        if (layerName == Dia::Core::StringCRC::kZero) return;
        if (mLayerManager->IsLayerEnabled(layerName)) mLayerManager->DisableLayer(layerName);
        else mLayerManager->EnableLayer(layerName);
        return;
    }
    if (cmd == kCmdSetScale)
    {
        if (!args.isMember("value") || !args["value"].isNumeric()) return;
        const Dia::Core::StringCRC key = args.isMember("key") && args["key"].isString()
                                       ? Dia::Core::StringCRC(args["key"].asCString())
                                       : kScaleKeyDebugScale;
        if (key == kScaleKeyDebugScale)
            mLayerManager->SetDebugScale(static_cast<float>(args["value"].asDouble()));
    }
}

// Drawer access
int UtilityAIDebugDomain::GetDrawerCount() const { return mScoreDrawer ? kDrawerCount : 0; }

Dia::Debug::IVisualDebugger* UtilityAIDebugDomain::GetDrawer(int index)
{
    switch (index)
    {
    case 0: return mScoreDrawer.get();
    default: return nullptr;
    }
}

Dia::Core::StringCRC UtilityAIDebugDomain::ResolveLayerName(const char* drawerName) const
{
    if (drawerName == nullptr || drawerName[0] == '\0') return Dia::Core::StringCRC::kZero;
    UtilityAIDebugDomain* self = const_cast<UtilityAIDebugDomain*>(this);
    const Dia::Core::StringCRC requested(drawerName);
    for (int i = 0; i < kDrawerCount; ++i)
    {
        const Dia::Debug::IVisualDebugger* drawer = self->GetDrawer(i);
        if (drawer == nullptr) continue;
        if (requested == Dia::Core::StringCRC(kDrawerLabels[i]) ||
            requested == drawer->GetLayerName())
            return drawer->GetLayerName();
    }
    return Dia::Core::StringCRC::kZero;
}

} // namespace Dia::UtilityAI

#endif // DIA_DEBUG
