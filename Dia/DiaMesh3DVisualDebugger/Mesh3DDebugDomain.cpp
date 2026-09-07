#include "Mesh3DDebugDomain.h"

#ifdef DIA_DEBUG

#include "MeshBoundsDrawer.h"
#include "MeshOriginDrawer.h"
#include "MeshStatsDrawer.h"

#include <DiaDebugDraw/Domain/IDebugLayerRegistry.h>
#include <DiaDebugDraw/Domain/DebugGroupAccents.h>

namespace Dia { namespace Mesh3D {

namespace
{
    const char* const kDrawerLabels[Mesh3DDebugDomain::kDrawerCount] =
    {
        "Bounds",
        "Origins",
        "Stats",
    };

    const int kDrawerPriorities[Mesh3DDebugDomain::kDrawerCount] =
    {
        10, 11, 12
    };

    const Dia::Core::StringCRC kStageTag("Mesh3D");
    const Dia::Core::StringCRC kCmdToggle("toggle");
    const Dia::Core::StringCRC kCmdSetScale("setScale");
    const Dia::Core::StringCRC kScaleKeyDebugScale("debugScale");
}

Mesh3DDebugDomain::Mesh3DDebugDomain(const Dia::Graphics3D::Mesh3DFrameData& frameData,
                                     const Dia::Mesh3D::Mesh3DAssetHandler&  assetHandler)
    : mFrameData(frameData)
    , mAssetHandler(assetHandler)
{}

Mesh3DDebugDomain::~Mesh3DDebugDomain() = default;

// Identity
Dia::Core::StringCRC Mesh3DDebugDomain::GetDomainId() const  { return Dia::Core::StringCRC("Mesh3D"); }
const char* Mesh3DDebugDomain::GetDisplayName() const        { return "Mesh3D"; }
const char* Mesh3DDebugDomain::GetDescription() const        { return "Mesh rendering — bounds, origins, load statistics"; }
Dia::Core::StringCRC Mesh3DDebugDomain::GetGroup() const     { return Dia::Core::StringCRC("Rendering"); }
Dia::Core::RGBA Mesh3DDebugDomain::GetAccentColour() const   { return Dia::VisualDebugger::DebugGroupAccents::kRendering; }

// Lifecycle
void Mesh3DDebugDomain::Register(Dia::Debug::IDebugLayerRegistry& mgr)
{
    if (mLayerManager != nullptr) return;
    mBoundsDrawer  = std::make_unique<MeshBoundsDrawer>(mFrameData, mAssetHandler);
    mOriginsDrawer = std::make_unique<MeshOriginDrawer>(mFrameData);
    mStatsDrawer   = std::make_unique<MeshStatsDrawer>(mFrameData, mAssetHandler, mgr);
    for (int i = 0; i < kDrawerCount; ++i)
        mgr.Register(GetDrawer(i), kDrawerPriorities[i], kStageTag);
    mLayerManager = &mgr;
}

void Mesh3DDebugDomain::Unregister(Dia::Debug::IDebugLayerRegistry& mgr)
{
    for (int i = 0; i < kDrawerCount; ++i)
        if (Dia::Debug::IVisualDebugger* d = GetDrawer(i)) mgr.Unregister(d->GetLayerName());
    mBoundsDrawer.reset(); mOriginsDrawer.reset(); mStatsDrawer.reset();
    mLayerManager = nullptr;
}

// Panel bridge
void Mesh3DDebugDomain::GetJSONState(Json::Value& out)
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

    Json::Value stats(Json::objectValue);
    if (mStatsDrawer)
    {
        stats["draws"]    = mStatsDrawer->GetCachedDrawCount();
        stats["dropped"]  = mStatsDrawer->GetCachedDroppedCount();
        stats["loaded"]   = mStatsDrawer->GetCachedLoadedCount();
        stats["skinned"]  = mStatsDrawer->GetCachedSkinnedCount();
        stats["static"]   = mStatsDrawer->GetCachedStaticCount();
        stats["ready"]    = mStatsDrawer->GetCachedStateReady();
        stats["pending"]  = mStatsDrawer->GetCachedStatePending();
        stats["failed"]   = mStatsDrawer->GetCachedStateFailed();
        stats["notFound"] = mStatsDrawer->GetCachedStateNotFound();

        Json::Value layers(Json::arrayValue);
        const int layerCount = mStatsDrawer->GetCachedLayerCount();
        for (int i = 0; i < layerCount; ++i)
        {
            const Dia::Mesh3D::MeshStatsDrawer::LayerBucket bucket = mStatsDrawer->GetCachedLayer(i);
            Json::Value entry(Json::objectValue);
            entry["index"]     = static_cast<int>(bucket.layer);
            entry["drawCount"] = bucket.count;
            layers.append(entry);
        }
        stats["layers"] = layers;
    }
    out["stats"] = stats;
}

void Mesh3DDebugDomain::OnCommand(Dia::Core::StringCRC cmd, const Json::Value& args)
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
int Mesh3DDebugDomain::GetDrawerCount() const { return mBoundsDrawer ? kDrawerCount : 0; }

Dia::Debug::IVisualDebugger* Mesh3DDebugDomain::GetDrawer(int index)
{
    switch (index)
    {
    case 0: return mBoundsDrawer.get();
    case 1: return mOriginsDrawer.get();
    case 2: return mStatsDrawer.get();
    default: return nullptr;
    }
}

Dia::Core::StringCRC Mesh3DDebugDomain::ResolveLayerName(const char* drawerName) const
{
    if (drawerName == nullptr || drawerName[0] == '\0') return Dia::Core::StringCRC::kZero;
    Mesh3DDebugDomain* self = const_cast<Mesh3DDebugDomain*>(this);
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

} } // namespace Dia::Mesh3D

#endif // DIA_DEBUG
