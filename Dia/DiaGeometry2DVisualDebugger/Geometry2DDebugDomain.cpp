#include "Geometry2DDebugDomain.h"

#ifdef DIA_DEBUG

#include "ShapeDrawer.h"
#include "ShapeLabelsDrawer.h"
#include "AABBOverlayDrawer.h"

#include <DiaDebugDraw/Domain/IDebugLayerRegistry.h>
#include <DiaDebugDraw/Domain/DebugGroupAccents.h>

namespace Dia::Geometry2DVisualDebugger
{

namespace
{
    const char* const kDrawerLabels[Geometry2DDebugDomain::kDrawerCount] =
    {
        "Shapes",
        "Labels",
        "AABB",
    };

    // Draw priorities preserved from pre-migration wiring (lower = underneath).
    const int kDrawerPriorities[Geometry2DDebugDomain::kDrawerCount] =
    {
        10, 50, 15
    };

    const Dia::Core::StringCRC kStageTag("Geometry2D");
    const Dia::Core::StringCRC kCmdToggle("toggle");
    const Dia::Core::StringCRC kCmdSetScale("setScale");
    const Dia::Core::StringCRC kScaleKeyDebugScale("debugScale");
}

Geometry2DDebugDomain::Geometry2DDebugDomain() = default;
Geometry2DDebugDomain::~Geometry2DDebugDomain() = default;

// ---------------------------------------------------------------------------
// Identity
// ---------------------------------------------------------------------------

Dia::Core::StringCRC Geometry2DDebugDomain::GetDomainId() const
{
    return Dia::Core::StringCRC("Geometry2D");
}

const char* Geometry2DDebugDomain::GetDisplayName() const
{
    return "Geometry2D";
}

const char* Geometry2DDebugDomain::GetDescription() const
{
    // AC-5: must stay <=80 characters.
    return "2D geometry debug — shape overlay, labels, AABB bounds";
}

Dia::Core::StringCRC Geometry2DDebugDomain::GetGroup() const
{
    return Dia::Core::StringCRC("Spatial");
}

Dia::Core::RGBA Geometry2DDebugDomain::GetAccentColour() const
{
    return Dia::VisualDebugger::DebugGroupAccents::kSpatial;
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void Geometry2DDebugDomain::Register(Dia::Debug::IDebugLayerRegistry& mgr)
{
    if (mLayerManager != nullptr) return;   // idempotent

    // DebugLayerManager IS an IDebugContext, so it doubles as the drawers'
    // context (global debug scale, selection, viewport).
    mShapesDrawer = std::make_unique<ShapeDrawer>(mgr);
    mLabelsDrawer = std::make_unique<ShapeLabelsDrawer>(mgr);
    mAABBDrawer   = std::make_unique<AABBOverlayDrawer>(mgr);

    for (int i = 0; i < kDrawerCount; ++i)
        mgr.Register(GetDrawer(i), kDrawerPriorities[i], kStageTag);

    mLayerManager = &mgr;
}

void Geometry2DDebugDomain::Unregister(Dia::Debug::IDebugLayerRegistry& mgr)
{
    for (int i = 0; i < kDrawerCount; ++i)
        if (Dia::Debug::IVisualDebugger* d = GetDrawer(i)) mgr.Unregister(d->GetLayerName());

    mShapesDrawer.reset();
    mLabelsDrawer.reset();
    mAABBDrawer.reset();
    mLayerManager = nullptr;
}

// ---------------------------------------------------------------------------
// Panel bridge
// ---------------------------------------------------------------------------

void Geometry2DDebugDomain::GetJSONState(Json::Value& out)
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

void Geometry2DDebugDomain::OnCommand(Dia::Core::StringCRC cmd, const Json::Value& args)
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

// ---------------------------------------------------------------------------
// Drawer access
// ---------------------------------------------------------------------------

int Geometry2DDebugDomain::GetDrawerCount() const
{
    return mShapesDrawer ? kDrawerCount : 0;
}

Dia::Debug::IVisualDebugger* Geometry2DDebugDomain::GetDrawer(int index)
{
    switch (index)
    {
    case 0: return mShapesDrawer.get();
    case 1: return mLabelsDrawer.get();
    case 2: return mAABBDrawer.get();
    default: return nullptr;
    }
}

Dia::Core::StringCRC Geometry2DDebugDomain::ResolveLayerName(const char* drawerName) const
{
    if (drawerName == nullptr || drawerName[0] == '\0') return Dia::Core::StringCRC::kZero;
    Geometry2DDebugDomain* self = const_cast<Geometry2DDebugDomain*>(this);
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

} // namespace Dia::Geometry2DVisualDebugger

#endif // DIA_DEBUG
