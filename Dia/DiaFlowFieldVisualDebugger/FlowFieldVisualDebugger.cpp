////////////////////////////////////////////////////////////////////////////////
// Filename: FlowFieldVisualDebugger.cpp
// System spec: docs/specs/applications/dia/systems/diaflowfieldvisualdebugger/
////////////////////////////////////////////////////////////////////////////////
#include "FlowFieldVisualDebugger.h"
#ifdef DIA_DEBUG

#include "DirectionArrowsDrawer.h"
#include "ReachabilityOverlayDrawer.h"
#include <DiaVisualDebugger/DebugLayerManager.h>
#include <DiaVisualDebugger/Domain/DebugGroupAccents.h>
#include <DiaFlowField/FlowField.h>
#include <DiaPathfinding/CellCoord.h>
#include <memory>

namespace
{
    const Dia::Core::StringCRC kCmdToggle("toggle");
    const Dia::Core::StringCRC kCmdSetScale("setScale");
    const Dia::Core::StringCRC kDrawerDirectionArrows("DirectionArrows");
    const Dia::Core::StringCRC kDrawerReachabilityOverlay("ReachabilityOverlay");
    const Dia::Core::StringCRC kScaleKeyArrowLength("arrowLength");
}

namespace Dia { namespace FlowField {

FlowFieldVisualDebugger::FlowFieldVisualDebugger(const FlowField& field, float cellSize)
    : mField(field), mCellSize(cellSize) {}

FlowFieldVisualDebugger::~FlowFieldVisualDebugger() = default;

Dia::Core::StringCRC FlowFieldVisualDebugger::GetDomainId()    const { return Dia::Core::StringCRC("flowfield"); }
const char* FlowFieldVisualDebugger::GetDisplayName()           const { return "Flow Field"; }
const char* FlowFieldVisualDebugger::GetDescription()           const { return "Flow field — per-cell direction arrows and reachability overlay"; }
Dia::Core::StringCRC FlowFieldVisualDebugger::GetGroup()        const { return Dia::Core::StringCRC("Navigation"); }
Dia::Core::RGBA      FlowFieldVisualDebugger::GetAccentColour() const { return Dia::VisualDebugger::DebugGroupAccents::kNavigation; }

void FlowFieldVisualDebugger::Register(Dia::Debug::DebugLayerManager& mgr)
{
    if (mLayerManager != nullptr) return;
    mDirectionArrows     = std::make_unique<DirectionArrowsDrawer>(mField, mgr, mCellSize);
    mReachabilityOverlay = std::make_unique<ReachabilityOverlayDrawer>(mField, mgr, mCellSize);
    mDirectionArrows->SetEnabled(mDirectionArrowsEnabled.load());
    mDirectionArrows->SetArrowLengthScale(mArrowLengthScale);
    mReachabilityOverlay->SetEnabled(mReachabilityOverlayEnabled.load());
    for (int i = 0; i < 2; ++i)
        mgr.Register(GetDrawer(i), 40 + i, Dia::Core::StringCRC("FlowField"));
    mLayerManager = &mgr;
}

void FlowFieldVisualDebugger::Unregister(Dia::Debug::DebugLayerManager& mgr)
{
    for (int i = 0; i < 2; ++i)
        if (auto* d = GetDrawer(i)) mgr.Unregister(d->GetLayerName());
    mDirectionArrows.reset();
    mReachabilityOverlay.reset();
    mLayerManager = nullptr;
}

Dia::Debug::IVisualDebugger* FlowFieldVisualDebugger::GetDrawer(int index)
{
    switch (index) {
        case 0: return mDirectionArrows.get();
        case 1: return mReachabilityOverlay.get();
        default: return nullptr;
    }
}

void FlowFieldVisualDebugger::GetJSONState(Json::Value& out)
{
    Json::Value drawers(Json::arrayValue);
    {
        Json::Value e(Json::objectValue);
        e["name"]    = "DirectionArrows";
        e["enabled"] = mDirectionArrowsEnabled.load();
        drawers.append(e);
    }
    {
        Json::Value e(Json::objectValue);
        e["name"]    = "ReachabilityOverlay";
        e["enabled"] = mReachabilityOverlayEnabled.load();
        drawers.append(e);
    }
    out["drawers"] = drawers;

    int reachableCount = 0;
    for (int row = 0; row < mField.GetHeight(); ++row)
        for (int col = 0; col < mField.GetWidth(); ++col)
            if (mField.Sample(Dia::Pathfinding::CellCoord{col, row}).reachable)
                ++reachableCount;

    Json::Value stats(Json::objectValue);
    stats["cellCount"]      = mField.GetCellCount();
    stats["reachableCount"] = reachableCount;
    stats["isComplete"]     = mField.IsComplete();
    out["stats"] = stats;
}

void FlowFieldVisualDebugger::OnCommand(Dia::Core::StringCRC cmd, const Json::Value& args)
{
    if (cmd == kCmdToggle)
    {
        if (!args.isMember("drawer") || !args["drawer"].isString()) return;
        const Dia::Core::StringCRC name(args["drawer"].asCString());

        if (name == kDrawerDirectionArrows)
        {
            const bool newVal = !mDirectionArrowsEnabled.load();
            mDirectionArrowsEnabled.store(newVal);
            if (mDirectionArrows) mDirectionArrows->SetEnabled(newVal);
        }
        else if (name == kDrawerReachabilityOverlay)
        {
            const bool newVal = !mReachabilityOverlayEnabled.load();
            mReachabilityOverlayEnabled.store(newVal);
            if (mReachabilityOverlay) mReachabilityOverlay->SetEnabled(newVal);
        }
        return;
    }

    if (cmd == kCmdSetScale)
    {
        if (!args.isMember("key")   || !args["key"].isString())    return;
        if (!args.isMember("value") || !args["value"].isNumeric()) return;
        const Dia::Core::StringCRC key(args["key"].asCString());
        if (key == kScaleKeyArrowLength)
        {
            mArrowLengthScale = static_cast<float>(args["value"].asDouble());
            if (mDirectionArrows) mDirectionArrows->SetArrowLengthScale(mArrowLengthScale);
        }
        return;
    }
    // unknown commands — no-op, no crash
}

} }
#endif // DIA_DEBUG
