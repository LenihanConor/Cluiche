////////////////////////////////////////////////////////////////////////////////
// Filename: ScalarFieldVisualDebugger.cpp
// System spec: docs/specs/applications/dia/systems/diadebugdomain/diadebugdomain.md
////////////////////////////////////////////////////////////////////////////////
#include "ScalarFieldVisualDebugger.h"
#ifdef DIA_DEBUG

#include <DiaDebugDraw/Domain/DebugGroupAccents.h>

namespace
{
    const Dia::Core::StringCRC kCmdToggle      ("toggle");
    const Dia::Core::StringCRC kCmdSetScale    ("setScale");
    const Dia::Core::StringCRC kDrawerHeatmap  ("Heatmap");
    const Dia::Core::StringCRC kDrawerGradient ("Gradient");
    const Dia::Core::StringCRC kScaleArrowScale("arrowScale");
}

namespace Dia { namespace ScalarField {

ScalarFieldVisualDebugger::ScalarFieldVisualDebugger() = default;

Dia::Core::StringCRC ScalarFieldVisualDebugger::GetDomainId()     const { return Dia::Core::StringCRC("scalarfield"); }
const char*          ScalarFieldVisualDebugger::GetDisplayName()  const { return "Scalar Field"; }
const char*          ScalarFieldVisualDebugger::GetDescription()  const { return "Scalar field \xe2\x80\x94 heatmap values, gradient arrows"; }
Dia::Core::StringCRC ScalarFieldVisualDebugger::GetGroup()        const { return Dia::Core::StringCRC("Spatial"); }
Dia::Core::RGBA      ScalarFieldVisualDebugger::GetAccentColour() const { return Dia::VisualDebugger::DebugGroupAccents::kSpatial; }

void ScalarFieldVisualDebugger::GetJSONState(Json::Value& out)
{
    Json::Value drawers(Json::arrayValue);
    {
        Json::Value e(Json::objectValue);
        e["name"] = "Heatmap"; e["enabled"] = mHeatmapEnabled.load();
        drawers.append(e);
    }
    {
        Json::Value e(Json::objectValue);
        e["name"] = "Gradient"; e["enabled"] = mGradientEnabled.load();
        drawers.append(e);
    }
    out["drawers"] = drawers;

    Json::Value stats(Json::objectValue);
    stats["arrowScale"] = mArrowScale;
    out["stats"] = stats;
}

void ScalarFieldVisualDebugger::OnCommand(Dia::Core::StringCRC cmd, const Json::Value& args)
{
    if (cmd == kCmdToggle)
    {
        if (!args.isMember("drawer") || !args["drawer"].isString()) return;
        const Dia::Core::StringCRC name(args["drawer"].asCString());

        if      (name == kDrawerHeatmap)  mHeatmapEnabled  = !mHeatmapEnabled.load();
        else if (name == kDrawerGradient) mGradientEnabled = !mGradientEnabled.load();
        return;
    }

    if (cmd == kCmdSetScale)
    {
        if (!args.isMember("key")   || !args["key"].isString())    return;
        if (!args.isMember("value") || !args["value"].isNumeric()) return;
        const Dia::Core::StringCRC key(args["key"].asCString());
        if (key == kScaleArrowScale)
            mArrowScale = static_cast<float>(args["value"].asDouble());
        // other setScale keys are no-op
        return;
    }
    // unknown command — no-op, no crash
}

} } // namespace Dia::ScalarField

#endif // DIA_DEBUG
