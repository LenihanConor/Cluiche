////////////////////////////////////////////////////////////////////////////////
// Filename: SteeringVisualDebugger.cpp
// System spec: docs/specs/applications/dia/systems/diasteeringvisualdebugger/diasteeringvisualdebugger.md
////////////////////////////////////////////////////////////////////////////////
#include "SteeringVisualDebugger.h"
#ifdef DIA_DEBUG

#include "VelocityArrowsDrawer.h"
#include "SeparationRadiusDrawer.h"
#include "DetectionBoxDrawer.h"
#include <DiaVisualDebugger/DebugLayerManager.h>
#include <DiaDebugDraw/Domain/DebugGroupAccents.h>
#include <DiaSteering/SteeringSystem.h>
#include <memory>

namespace
{
    const Dia::Core::StringCRC kCmdToggle("toggle");
    const Dia::Core::StringCRC kCmdSetScale("setScale");
    const Dia::Core::StringCRC kDrawerVelocityArrows("VelocityArrows");
    const Dia::Core::StringCRC kDrawerSeparationRadius("SeparationRadius");
    const Dia::Core::StringCRC kDrawerDetectionBoxes("DetectionBoxes");
    const Dia::Core::StringCRC kScaleKeyArrowScale("arrowScale");
}

namespace Dia { namespace Steering {

SteeringVisualDebugger::SteeringVisualDebugger(const SteeringSystem& system)
    : mSystem(system) {}

SteeringVisualDebugger::~SteeringVisualDebugger() = default;

Dia::Core::StringCRC SteeringVisualDebugger::GetDomainId()    const { return Dia::Core::StringCRC("steering"); }
const char* SteeringVisualDebugger::GetDisplayName()           const { return "Steering"; }
const char* SteeringVisualDebugger::GetDescription()           const { return "Steering agents — velocity arrows, separation radii, detection boxes"; }
Dia::Core::StringCRC SteeringVisualDebugger::GetGroup()        const { return Dia::Core::StringCRC("Navigation"); }
Dia::Core::RGBA      SteeringVisualDebugger::GetAccentColour() const { return Dia::VisualDebugger::DebugGroupAccents::kNavigation; }

void SteeringVisualDebugger::Register(Dia::Debug::DebugLayerManager& mgr)
{
    if (mLayerManager != nullptr) return;
    mVelocityArrows   = std::make_unique<VelocityArrowsDrawer>(mSystem, mgr, mArrowScale);
    mSeparationRadius = std::make_unique<SeparationRadiusDrawer>(mSystem, mgr);
    mDetectionBoxes   = std::make_unique<DetectionBoxDrawer>(mSystem, mgr);
    // Sync initial enabled state from atomics
    mVelocityArrows->SetEnabled(mVelocityArrowsEnabled.load());
    mSeparationRadius->SetEnabled(mSeparationRadiusEnabled.load());
    mDetectionBoxes->SetEnabled(mDetectionBoxesEnabled.load());
    for (int i = 0; i < 3; ++i)
        mgr.Register(GetDrawer(i), 20 + i, Dia::Core::StringCRC("Steering"));
    mLayerManager = &mgr;
}

void SteeringVisualDebugger::Unregister(Dia::Debug::DebugLayerManager& mgr)
{
    for (int i = 0; i < 3; ++i)
        if (auto* d = GetDrawer(i)) mgr.Unregister(d->GetLayerName());
    mVelocityArrows.reset();
    mSeparationRadius.reset();
    mDetectionBoxes.reset();
    mLayerManager = nullptr;
}

Dia::Debug::IVisualDebugger* SteeringVisualDebugger::GetDrawer(int index)
{
    switch (index) {
        case 0: return mVelocityArrows.get();
        case 1: return mSeparationRadius.get();
        case 2: return mDetectionBoxes.get();
        default: return nullptr;
    }
}

void SteeringVisualDebugger::GetJSONState(Json::Value& out)
{
    // drawers
    Json::Value drawers(Json::arrayValue);
    {
        Json::Value e(Json::objectValue);
        e["name"]    = "VelocityArrows";
        e["enabled"] = mVelocityArrowsEnabled.load();
        drawers.append(e);
    }
    {
        Json::Value e(Json::objectValue);
        e["name"]    = "SeparationRadius";
        e["enabled"] = mSeparationRadiusEnabled.load();
        drawers.append(e);
    }
    {
        Json::Value e(Json::objectValue);
        e["name"]    = "DetectionBoxes";
        e["enabled"] = mDetectionBoxesEnabled.load();
        drawers.append(e);
    }
    out["drawers"] = drawers;

    // stats
    Json::Value stats(Json::objectValue);
    stats["agentCount"] = mSystem.GetAgentCount();
    out["stats"] = stats;
}

void SteeringVisualDebugger::OnCommand(Dia::Core::StringCRC cmd, const Json::Value& args)
{
    if (cmd == kCmdToggle)
    {
        if (!args.isMember("drawer") || !args["drawer"].isString()) return;
        const Dia::Core::StringCRC name(args["drawer"].asCString());

        if (name == kDrawerVelocityArrows)
        {
            const bool newVal = !mVelocityArrowsEnabled.load();
            mVelocityArrowsEnabled.store(newVal);
            if (mVelocityArrows) mVelocityArrows->SetEnabled(newVal);
        }
        else if (name == kDrawerSeparationRadius)
        {
            const bool newVal = !mSeparationRadiusEnabled.load();
            mSeparationRadiusEnabled.store(newVal);
            if (mSeparationRadius) mSeparationRadius->SetEnabled(newVal);
        }
        else if (name == kDrawerDetectionBoxes)
        {
            const bool newVal = !mDetectionBoxesEnabled.load();
            mDetectionBoxesEnabled.store(newVal);
            if (mDetectionBoxes) mDetectionBoxes->SetEnabled(newVal);
        }
        return;
    }

    if (cmd == kCmdSetScale)
    {
        if (!args.isMember("key") || !args["key"].isString()) return;
        if (!args.isMember("value") || !args["value"].isNumeric()) return;
        const Dia::Core::StringCRC key(args["key"].asCString());
        if (key == kScaleKeyArrowScale)
        {
            mArrowScale = static_cast<float>(args["value"].asDouble());
            if (mVelocityArrows) mVelocityArrows->SetArrowScale(mArrowScale);
        }
        // other setScale keys are no-op (no crash)
        return;
    }
    // unknown commands — no-op, no crash
}

} }
#endif // DIA_DEBUG
