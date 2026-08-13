#include "Lighting3DDebugDomain.h"

#ifdef DIA_DEBUG

#include "LightWidgetsDrawer.h"
#include "LightPathArcDrawer.h"

#include <DiaVisualDebugger/DebugLayerManager.h>
#include <DiaVisualDebugger/Domain/DebugGroupAccents.h>

namespace Dia { namespace Lighting3D {

namespace
{
    const char* const kDrawerLabels[Lighting3DDebugDomain::kDrawerCount] =
    {
        "Widgets",
        "PathArc",
    };

    const int kDrawerPriorities[Lighting3DDebugDomain::kDrawerCount] =
    {
        10, 11
    };

    const Dia::Core::StringCRC kStageTag("Lighting3D");
    const Dia::Core::StringCRC kCmdToggle("toggle");
    const Dia::Core::StringCRC kCmdSetScale("setScale");
    const Dia::Core::StringCRC kScaleKeyDebugScale("debugScale");
}

Lighting3DDebugDomain::Lighting3DDebugDomain(const LightRegistry3D& registry)
    : mRegistry(registry)
{}

Lighting3DDebugDomain::~Lighting3DDebugDomain() = default;

// Identity
Dia::Core::StringCRC Lighting3DDebugDomain::GetDomainId() const  { return Dia::Core::StringCRC("Lighting3D"); }
const char* Lighting3DDebugDomain::GetDisplayName() const        { return "Lighting3D"; }
const char* Lighting3DDebugDomain::GetDescription() const        { return "3D lights — widget handles, directional arc paths"; }
Dia::Core::StringCRC Lighting3DDebugDomain::GetGroup() const     { return Dia::Core::StringCRC("Rendering"); }
Dia::Core::RGBA Lighting3DDebugDomain::GetAccentColour() const   { return Dia::VisualDebugger::DebugGroupAccents::kRendering; }

// Lifecycle
void Lighting3DDebugDomain::Register(Dia::Debug::DebugLayerManager& mgr)
{
    if (mLayerManager != nullptr) return;
    mWidgetsDrawer = std::make_unique<LightWidgetsDrawer>(mRegistry);
    mPathArcDrawer = std::make_unique<LightPathArcDrawer>(mRegistry);
    for (int i = 0; i < kDrawerCount; ++i)
        mgr.Register(GetDrawer(i), kDrawerPriorities[i], kStageTag);
    mLayerManager = &mgr;
}

void Lighting3DDebugDomain::Unregister(Dia::Debug::DebugLayerManager& mgr)
{
    for (int i = 0; i < kDrawerCount; ++i)
        if (Dia::Debug::IVisualDebugger* d = GetDrawer(i)) mgr.Unregister(d->GetLayerName());
    mWidgetsDrawer.reset(); mPathArcDrawer.reset();
    mLayerManager = nullptr;
}

// Panel bridge
void Lighting3DDebugDomain::GetJSONState(Json::Value& out)
{
    Json::Value drawers(Json::arrayValue);
    for (int i = 0; i < kDrawerCount; ++i)
    {
        const Dia::Debug::IVisualDebugger* drawer = GetDrawer(i);
        Json::Value entry(Json::objectValue);
        entry["name"]    = kDrawerLabels[i];
        entry["enabled"] = (mLayerManager != nullptr && drawer != nullptr)
                         ? mLayerManager->IsLayerEnabled(drawer->GetLayerName())
                         : false;
        drawers.append(entry);
    }
    out["drawers"] = drawers;
    out["stats"]   = Json::Value(Json::objectValue);
}

void Lighting3DDebugDomain::OnCommand(Dia::Core::StringCRC cmd, const Json::Value& args)
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
int Lighting3DDebugDomain::GetDrawerCount() const { return mWidgetsDrawer ? kDrawerCount : 0; }

Dia::Debug::IVisualDebugger* Lighting3DDebugDomain::GetDrawer(int index)
{
    switch (index)
    {
    case 0: return mWidgetsDrawer.get();
    case 1: return mPathArcDrawer.get();
    default: return nullptr;
    }
}

Dia::Core::StringCRC Lighting3DDebugDomain::ResolveLayerName(const char* drawerName) const
{
    if (drawerName == nullptr || drawerName[0] == '\0') return Dia::Core::StringCRC::kZero;
    Lighting3DDebugDomain* self = const_cast<Lighting3DDebugDomain*>(this);
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

} } // namespace Dia::Lighting3D

#endif // DIA_DEBUG
