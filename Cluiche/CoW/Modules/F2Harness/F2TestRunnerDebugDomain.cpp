#include "Modules/F2TestRunnerDebugDomain.h"
#ifdef DIA_DEBUG

#include "Modules/F2TestRunnerModule.h"
#include "Modules/F2PanelModel.h"
#include <DiaVisualDebugger/Domain/DebugGroupAccents.h>
#include <DiaObservation/Log/DiaLog.h>
#include <cmath>

namespace CoW {

namespace {
    // The panel page keys commands by the display name, and the host looks the domain up by the
    // CRC of that string, so the domain id MUST be derived from it (single source of truth).
    const char* const kDisplayName = "F-2 Core Runtime";
    const Dia::Core::StringCRC kCmdRunScenario("run_scenario");
    const Dia::Core::StringCRC kCmdSetSpeed("set_speed");
    const Dia::Core::StringCRC kCmdStep("step");
    const Dia::Core::StringCRC kCmdModalOpen("modal_open");
    const Dia::Core::StringCRC kCmdModalClose("modal_close");
}

F2TestRunnerDebugDomain::F2TestRunnerDebugDomain(F2TestRunnerModule& runner) : mRunner(runner) {}

Dia::Core::StringCRC F2TestRunnerDebugDomain::GetDomainId()    const { return Dia::Core::StringCRC(kDisplayName); }
const char*          F2TestRunnerDebugDomain::GetDisplayName() const { return kDisplayName; }
const char*          F2TestRunnerDebugDomain::GetDescription() const { return "F-2 Save, Reload, Replay scenario runner"; }
Dia::Core::StringCRC F2TestRunnerDebugDomain::GetGroup()       const { return Dia::Core::StringCRC("Systems"); }
Dia::Core::RGBA      F2TestRunnerDebugDomain::GetAccentColour() const { return Dia::VisualDebugger::DebugGroupAccents::kCoreDebug; }

void F2TestRunnerDebugDomain::GetJSONState(Json::Value& out)
{
    const F2PanelModel model = F2PanelModel::Build(F2PanelModel::CaptureInput(mRunner));
    const Json::Value json = model.ToJson();
    for (const std::string& key : json.getMemberNames())
        out[key] = json[key];

    // Marker the panel page uses to render the dedicated TestRunnerPanel card instead of the generic one.
    out["panel"] = "f2_test_runner";
    out["drawers"] = Json::Value(Json::arrayValue);
    Json::Value& stats = out["stats"] = Json::Value(Json::objectValue);
    stats["scenario"] = model.tickText;
    stats["pass"] = model.passCount;
    stats["fail"] = model.failCount;
}

void F2TestRunnerDebugDomain::OnCommand(Dia::Core::StringCRC cmd, const Json::Value& args)
{
    bool ok = false;
    if (cmd == kCmdRunScenario)
        ok = mRunner.RunScenario();
    else if (cmd == kCmdSetSpeed)
    {
        if (args.isObject() && args.isMember("tier") && args["tier"].isNumeric())
        {
            const long tier = std::lround(args["tier"].asDouble());
            if (tier >= 0 && tier <= static_cast<long>(SpeedTier::Fastest))
                ok = mRunner.RequestSetSpeed(static_cast<SpeedTier>(tier));
        }
    }
    else if (cmd == kCmdStep)
        ok = mRunner.RequestManualStep();
    else if (cmd == kCmdModalOpen)
        ok = mRunner.RequestModal(true);
    else if (cmd == kCmdModalClose)
        ok = mRunner.RequestModal(false);

    if (ok)
        ++mAccepted;
    else
    {
        ++mRefused;
        DIA_LOG_INFO("CoW", "F2TestRunnerDebugDomain: command refused (unknown, malformed or scenario running)");
    }
}

} // namespace CoW

#endif // DIA_DEBUG
