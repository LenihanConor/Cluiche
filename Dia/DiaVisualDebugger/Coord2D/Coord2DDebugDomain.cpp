#include "Coord2DDebugDomain.h"

#ifdef DIA_DEBUG

#include "Coord2DOriginDrawer.h"
#include "Coord2DAxesDrawer.h"
#include "Coord2DGridDrawer.h"
#include "Coord2DBoundsDrawer.h"
#include "Coord2DCursorDrawer.h"

#include <DiaVisualDebugger/DebugLayerManager.h>
#include <DiaDebugDraw/Domain/DebugGroupAccents.h>
#include <DiaCore/DebugDraw/DebugLayerNames.h>

namespace Dia::Debug
{

namespace
{
    const char* const kDrawerLabels[Coord2DDebugDomain::kDrawerCount] =
    {
        "Origin",
        "Axes",
        "Grid",
        "Bounds",
        "Cursor",
    };

    // Priorities preserved from pre-migration VisualDebuggerModule wiring.
    const int kDrawerPriorities[Coord2DDebugDomain::kDrawerCount] =
    {
        50, 51, 52, 53, 54
    };

    const Dia::Core::StringCRC kCmdToggle("toggle");
    const Dia::Core::StringCRC kCmdSetScale("setScale");
    const Dia::Core::StringCRC kScaleKeyDebugScale("debugScale");
}

Coord2DDebugDomain::Coord2DDebugDomain() = default;
Coord2DDebugDomain::~Coord2DDebugDomain() = default;

// Identity
Dia::Core::StringCRC Coord2DDebugDomain::GetDomainId() const  { return Dia::Core::StringCRC("Coord2D"); }
const char* Coord2DDebugDomain::GetDisplayName() const        { return "Coord2D"; }
const char* Coord2DDebugDomain::GetDescription() const        { return "2D coordinate system — origin, axes, grid"; }
Dia::Core::StringCRC Coord2DDebugDomain::GetGroup() const     { return Dia::Core::StringCRC("CoreDebug"); }
Dia::Core::RGBA Coord2DDebugDomain::GetAccentColour() const   { return Dia::VisualDebugger::DebugGroupAccents::kCoreDebug; }

// Lifecycle
void Coord2DDebugDomain::Register(DebugLayerManager& mgr)
{
    if (mLayerManager != nullptr) return;

    mOriginDrawer = std::make_unique<Coord2DOriginDrawer>(mgr);
    mAxesDrawer   = std::make_unique<Coord2DAxesDrawer>(mgr);
    mGridDrawer   = std::make_unique<Coord2DGridDrawer>(mgr);
    mBoundsDrawer = std::make_unique<Coord2DBoundsDrawer>(mgr);
    mCursorDrawer = std::make_unique<Coord2DCursorDrawer>(mgr);

    for (int i = 0; i < kDrawerCount; ++i)
        mgr.Register(GetDrawer(i), kDrawerPriorities[i], Dia::Debug::LayerNames::kCoord2DStageTag);

    // Coord2D overlays are opt-in — disable all layers after registration.
    mgr.DisableLayer(Dia::Debug::LayerNames::kCoord2DOrigin);
    mgr.DisableLayer(Dia::Debug::LayerNames::kCoord2DAxes);
    mgr.DisableLayer(Dia::Debug::LayerNames::kCoord2DGrid);
    mgr.DisableLayer(Dia::Debug::LayerNames::kCoord2DBounds);
    mgr.DisableLayer(Dia::Debug::LayerNames::kCoord2DCursor);

    mLayerManager = &mgr;
}

void Coord2DDebugDomain::Unregister(DebugLayerManager& mgr)
{
    for (int i = 0; i < kDrawerCount; ++i)
        if (IVisualDebugger* d = GetDrawer(i)) mgr.Unregister(d->GetLayerName());
    mOriginDrawer.reset(); mAxesDrawer.reset(); mGridDrawer.reset();
    mBoundsDrawer.reset(); mCursorDrawer.reset();
    mLayerManager = nullptr;
}

// Panel bridge
void Coord2DDebugDomain::GetJSONState(Json::Value& out)
{
    Json::Value drawers(Json::arrayValue);
    for (int i = 0; i < kDrawerCount; ++i)
    {
        const IVisualDebugger* drawer = GetDrawer(i);
        Json::Value entry(Json::objectValue);
        entry["name"]    = kDrawerLabels[i];
        entry["enabled"] = (mLayerManager != nullptr && drawer != nullptr)
                         ? mLayerManager->IsLayerEnabled(drawer->GetLayerName())
                         : false;
        drawers.append(entry);
    }
    out["drawers"] = drawers;

    Json::Value stats(Json::objectValue);
    if (mLayerManager != nullptr)
    {
        const Dia::Maths::Vector2D cursor = mLayerManager->GetCursorWorld();
        Json::Value cur(Json::objectValue);
        cur["x"] = cursor.x;
        cur["y"] = cursor.y;
        stats["cursor"] = cur;
    }
    out["stats"] = stats;
}

void Coord2DDebugDomain::OnCommand(Dia::Core::StringCRC cmd, const Json::Value& args)
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
int Coord2DDebugDomain::GetDrawerCount() const { return mOriginDrawer ? kDrawerCount : 0; }

IVisualDebugger* Coord2DDebugDomain::GetDrawer(int index)
{
    switch (index)
    {
    case 0: return mOriginDrawer.get();
    case 1: return mAxesDrawer.get();
    case 2: return mGridDrawer.get();
    case 3: return mBoundsDrawer.get();
    case 4: return mCursorDrawer.get();
    default: return nullptr;
    }
}

Dia::Core::StringCRC Coord2DDebugDomain::ResolveLayerName(const char* drawerName) const
{
    if (drawerName == nullptr || drawerName[0] == '\0') return Dia::Core::StringCRC::kZero;
    Coord2DDebugDomain* self = const_cast<Coord2DDebugDomain*>(this);
    const Dia::Core::StringCRC requested(drawerName);
    for (int i = 0; i < kDrawerCount; ++i)
    {
        const IVisualDebugger* drawer = self->GetDrawer(i);
        if (drawer == nullptr) continue;
        if (requested == Dia::Core::StringCRC(kDrawerLabels[i]) ||
            requested == drawer->GetLayerName())
            return drawer->GetLayerName();
    }
    return Dia::Core::StringCRC::kZero;
}

} // namespace Dia::Debug

#endif // DIA_DEBUG
