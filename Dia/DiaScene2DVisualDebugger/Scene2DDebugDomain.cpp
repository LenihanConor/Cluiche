#include "Scene2DDebugDomain.h"

#ifdef DIA_DEBUG

#include "SceneOverviewDrawer.h"

#include <DiaVisualDebugger/DebugLayerManager.h>
#include <DiaVisualDebugger/Domain/DebugGroupAccents.h>

namespace Dia::Scene2DVisualDebugger
{

namespace
{
    const char* const kDrawerLabels[Scene2DDebugDomain::kDrawerCount] =
    {
        "Overview",
    };

    const int kDrawerPriorities[Scene2DDebugDomain::kDrawerCount] =
    {
        5
    };

    const Dia::Core::StringCRC kStageTag("Scene2D");
    const Dia::Core::StringCRC kCmdToggle("toggle");
    const Dia::Core::StringCRC kCmdSetScale("setScale");
    const Dia::Core::StringCRC kScaleKeyDebugScale("debugScale");
}

Scene2DDebugDomain::Scene2DDebugDomain(
    const Dia::Camera2D::CameraRegistry2D&  cameraRegistry,
    const Dia::Lighting2D::LightRegistry2D& lightRegistry,
    const Dia::Scene2D::LayerTable&         layerTable)
    : mCameraRegistry(cameraRegistry)
    , mLightRegistry(lightRegistry)
    , mLayerTable(layerTable)
{}

Scene2DDebugDomain::~Scene2DDebugDomain() = default;

// Identity
Dia::Core::StringCRC Scene2DDebugDomain::GetDomainId() const  { return Dia::Core::StringCRC("Scene2D"); }
const char* Scene2DDebugDomain::GetDisplayName() const        { return "Scene2D"; }
const char* Scene2DDebugDomain::GetDescription() const        { return "Scene overview — cameras, lights, layers, world bounds"; }
Dia::Core::StringCRC Scene2DDebugDomain::GetGroup() const     { return Dia::Core::StringCRC("Rendering"); }
Dia::Core::RGBA Scene2DDebugDomain::GetAccentColour() const   { return Dia::VisualDebugger::DebugGroupAccents::kRendering; }

// Lifecycle
void Scene2DDebugDomain::Register(Dia::Debug::DebugLayerManager& mgr)
{
    if (mLayerManager != nullptr) return;
    mOverviewDrawer = std::make_unique<SceneOverviewDrawer>(mCameraRegistry, mLightRegistry, mLayerTable, mgr);
    for (int i = 0; i < kDrawerCount; ++i)
        mgr.Register(GetDrawer(i), kDrawerPriorities[i], kStageTag);
    mLayerManager = &mgr;
}

void Scene2DDebugDomain::Unregister(Dia::Debug::DebugLayerManager& mgr)
{
    for (int i = 0; i < kDrawerCount; ++i)
        if (Dia::Debug::IVisualDebugger* d = GetDrawer(i)) mgr.Unregister(d->GetLayerName());
    mOverviewDrawer.reset();
    mLayerManager = nullptr;
}

// Panel bridge
void Scene2DDebugDomain::GetJSONState(Json::Value& out)
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

void Scene2DDebugDomain::OnCommand(Dia::Core::StringCRC cmd, const Json::Value& args)
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
int Scene2DDebugDomain::GetDrawerCount() const { return mOverviewDrawer ? kDrawerCount : 0; }

Dia::Debug::IVisualDebugger* Scene2DDebugDomain::GetDrawer(int index)
{
    switch (index)
    {
    case 0: return mOverviewDrawer.get();
    default: return nullptr;
    }
}

Dia::Core::StringCRC Scene2DDebugDomain::ResolveLayerName(const char* drawerName) const
{
    if (drawerName == nullptr || drawerName[0] == '\0') return Dia::Core::StringCRC::kZero;
    Scene2DDebugDomain* self = const_cast<Scene2DDebugDomain*>(this);
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

} // namespace Dia::Scene2DVisualDebugger

#endif // DIA_DEBUG
