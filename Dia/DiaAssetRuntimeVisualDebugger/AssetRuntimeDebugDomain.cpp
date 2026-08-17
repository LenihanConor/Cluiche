#include "AssetRuntimeDebugDomain.h"

#ifdef DIA_DEBUG

#include "DiaAssetRuntimeVisualDebugger.h"

#include <DiaAssetRuntime/AssetRuntime.h>
#include <DiaVisualDebugger/DebugLayerManager.h>
#include <DiaVisualDebugger/Domain/DebugGroupAccents.h>

namespace Dia::AssetRuntime
{

namespace
{
    const char* const kDrawerLabels[AssetRuntimeDebugDomain::kDrawerCount] =
    {
        "AssetRuntime",
    };

    const int kDrawerPriorities[AssetRuntimeDebugDomain::kDrawerCount] =
    {
        50
    };

    const Dia::Core::StringCRC kStageTag("AssetRuntime");
    const Dia::Core::StringCRC kCmdToggle("toggle");
    const Dia::Core::StringCRC kCmdSetScale("setScale");
    const Dia::Core::StringCRC kScaleKeyDebugScale("debugScale");
}

AssetRuntimeDebugDomain::AssetRuntimeDebugDomain() = default;
AssetRuntimeDebugDomain::~AssetRuntimeDebugDomain() = default;

void AssetRuntimeDebugDomain::SetRuntime(const Dia::AssetRuntime::AssetRuntime* runtime)
{
    mRuntime = runtime;
}

// Identity
Dia::Core::StringCRC AssetRuntimeDebugDomain::GetDomainId() const  { return Dia::Core::StringCRC("AssetRuntime"); }
const char* AssetRuntimeDebugDomain::GetDisplayName() const        { return "AssetRuntime"; }
const char* AssetRuntimeDebugDomain::GetDescription() const        { return "Asset loading — runtime status and load statistics"; }
Dia::Core::StringCRC AssetRuntimeDebugDomain::GetGroup() const     { return Dia::Core::StringCRC("CoreDebug"); }
Dia::Core::RGBA AssetRuntimeDebugDomain::GetAccentColour() const   { return Dia::VisualDebugger::DebugGroupAccents::kCoreDebug; }

// Lifecycle
void AssetRuntimeDebugDomain::Register(Dia::Debug::DebugLayerManager& mgr)
{
    if (mLayerManager != nullptr) return;
    mDebugger = std::make_unique<DiaAssetRuntimeVisualDebugger>();
    for (int i = 0; i < kDrawerCount; ++i)
        mgr.Register(GetDrawer(i), kDrawerPriorities[i], kStageTag);
    mLayerManager = &mgr;
}

void AssetRuntimeDebugDomain::Unregister(Dia::Debug::DebugLayerManager& mgr)
{
    for (int i = 0; i < kDrawerCount; ++i)
        if (Dia::Debug::IVisualDebugger* d = GetDrawer(i)) mgr.Unregister(d->GetLayerName());
    mDebugger.reset();
    mLayerManager = nullptr;
}

// Panel bridge
void AssetRuntimeDebugDomain::GetJSONState(Json::Value& out)
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

    Json::Value stats(Json::objectValue);
    if (mRuntime != nullptr)
    {
        Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 128> allBuf;
        Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 128> loadedBuf;
        Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 128> pendingBuf;
        const unsigned int assetCount   = mRuntime->GetAllAssets(allBuf);
        const unsigned int loadedCount  = mRuntime->GetLoadedAssets(loadedBuf);
        const unsigned int pendingCount = mRuntime->GetStagedAssets(pendingBuf);

        stats["assetCount"]   = assetCount;
        stats["loadedCount"]  = loadedCount;
        stats["pendingCount"] = pendingCount;
    }
    out["stats"] = stats;
}

void AssetRuntimeDebugDomain::OnCommand(Dia::Core::StringCRC cmd, const Json::Value& args)
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
int AssetRuntimeDebugDomain::GetDrawerCount() const { return mDebugger ? kDrawerCount : 0; }

Dia::Debug::IVisualDebugger* AssetRuntimeDebugDomain::GetDrawer(int index)
{
    switch (index)
    {
    case 0: return mDebugger.get();
    default: return nullptr;
    }
}

Dia::Core::StringCRC AssetRuntimeDebugDomain::ResolveLayerName(const char* drawerName) const
{
    if (drawerName == nullptr || drawerName[0] == '\0') return Dia::Core::StringCRC::kZero;
    AssetRuntimeDebugDomain* self = const_cast<AssetRuntimeDebugDomain*>(this);
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

} // namespace Dia::AssetRuntime

#endif // DIA_DEBUG
