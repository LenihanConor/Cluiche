////////////////////////////////////////////////////////////////////////////////
// Filename: PathfindingVisualDebugger.cpp
// System spec: docs/specs/applications/dia/systems/diapathfindingvisualdebugger/
////////////////////////////////////////////////////////////////////////////////
#include "PathfindingVisualDebugger.h"
#ifdef DIA_DEBUG

#include "PathPolylineDrawer.h"
#include "GridPassabilityDrawer.h"
#include <DiaVisualDebugger/DebugLayerManager.h>
#include <DiaVisualDebugger/Domain/DebugGroupAccents.h>
#include <DiaPathfinding/SquarePathGrid.h>
#include <DiaPathfinding/PathResult.h>
#include <memory>

namespace
{
    const Dia::Core::StringCRC kCmdToggle("toggle");
    const Dia::Core::StringCRC kCmdSetScale("setScale");
    const Dia::Core::StringCRC kDrawerPathPolyline("PathPolyline");
    const Dia::Core::StringCRC kDrawerGridPassability("GridPassability");
    const Dia::Core::StringCRC kScaleKeyMarkerRadius("markerRadius");
}

namespace Dia { namespace Pathfinding {

PathfindingVisualDebugger::PathfindingVisualDebugger(const SquarePathGrid& grid,
                                                      const PathResult& result,
                                                      float cellSize)
    : mGrid(grid), mResult(result), mCellSize(cellSize) {}

PathfindingVisualDebugger::~PathfindingVisualDebugger() = default;

Dia::Core::StringCRC PathfindingVisualDebugger::GetDomainId()    const { return Dia::Core::StringCRC("pathfinding"); }
const char* PathfindingVisualDebugger::GetDisplayName()           const { return "Pathfinding"; }
const char* PathfindingVisualDebugger::GetDescription()           const { return "Pathfinding - path polyline, start/goal markers, grid passability"; }
Dia::Core::StringCRC PathfindingVisualDebugger::GetGroup()        const { return Dia::Core::StringCRC("Navigation"); }
Dia::Core::RGBA      PathfindingVisualDebugger::GetAccentColour() const { return Dia::VisualDebugger::DebugGroupAccents::kNavigation; }

void PathfindingVisualDebugger::Register(Dia::Debug::DebugLayerManager& mgr)
{
    if (mLayerManager != nullptr) return;
    mPathPolyline    = std::make_unique<PathPolylineDrawer>(mGrid, mResult, mgr, mCellSize);
    mGridPassability = std::make_unique<GridPassabilityDrawer>(mGrid, mgr, mCellSize);
    mPathPolyline->SetMarkerRadius(mMarkerRadius);
    mPathPolyline->SetEnabled(mPathPolylineEnabled.load());
    mGridPassability->SetEnabled(mGridPassabilityEnabled.load());
    for (int i = 0; i < 2; ++i)
        mgr.Register(GetDrawer(i), 30 + i, Dia::Core::StringCRC("Pathfinding"));
    mLayerManager = &mgr;
}

void PathfindingVisualDebugger::Unregister(Dia::Debug::DebugLayerManager& mgr)
{
    for (int i = 0; i < 2; ++i)
        if (auto* d = GetDrawer(i)) mgr.Unregister(d->GetLayerName());
    mPathPolyline.reset();
    mGridPassability.reset();
    mLayerManager = nullptr;
}

Dia::Debug::IVisualDebugger* PathfindingVisualDebugger::GetDrawer(int index)
{
    switch (index) {
        case 0: return mPathPolyline.get();
        case 1: return mGridPassability.get();
        default: return nullptr;
    }
}

void PathfindingVisualDebugger::GetJSONState(Json::Value& out)
{
    Json::Value drawers(Json::arrayValue);
    {
        Json::Value e(Json::objectValue);
        e["name"]    = "PathPolyline";
        e["enabled"] = mPathPolylineEnabled.load();
        drawers.append(e);
    }
    {
        Json::Value e(Json::objectValue);
        e["name"]    = "GridPassability";
        e["enabled"] = mGridPassabilityEnabled.load();
        drawers.append(e);
    }
    out["drawers"] = drawers;

    Json::Value stats(Json::objectValue);
    stats["pathActive"]    = mResult.success;
    stats["waypointCount"] = static_cast<int>(mResult.cells.Size());
    stats["totalCost"]     = mResult.totalCost;
    out["stats"] = stats;
}

void PathfindingVisualDebugger::OnCommand(Dia::Core::StringCRC cmd, const Json::Value& args)
{
    if (cmd == kCmdToggle)
    {
        if (!args.isMember("drawer") || !args["drawer"].isString()) return;
        const Dia::Core::StringCRC name(args["drawer"].asCString());

        if (name == kDrawerPathPolyline)
        {
            const bool newVal = !mPathPolylineEnabled.load();
            mPathPolylineEnabled.store(newVal);
            if (mPathPolyline) mPathPolyline->SetEnabled(newVal);
        }
        else if (name == kDrawerGridPassability)
        {
            const bool newVal = !mGridPassabilityEnabled.load();
            mGridPassabilityEnabled.store(newVal);
            if (mGridPassability) mGridPassability->SetEnabled(newVal);
        }
        return;
    }

    if (cmd == kCmdSetScale)
    {
        if (!args.isMember("key")   || !args["key"].isString())   return;
        if (!args.isMember("value") || !args["value"].isNumeric()) return;
        const Dia::Core::StringCRC key(args["key"].asCString());
        if (key == kScaleKeyMarkerRadius)
        {
            mMarkerRadius = static_cast<float>(args["value"].asDouble());
            if (mPathPolyline) mPathPolyline->SetMarkerRadius(mMarkerRadius);
        }
        return;
    }
    // unknown commands — no-op, no crash
}

} }
#endif // DIA_DEBUG
