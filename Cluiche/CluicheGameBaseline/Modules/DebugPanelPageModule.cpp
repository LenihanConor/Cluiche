#include "Modules/DebugPanelPageModule.h"

#ifdef DIA_DEBUG

#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/ProcessingUnit.h>
#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaCore/Json/external/json/json.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Trace/DiaTrace.h>
#include <DiaUI/IUISystem.h>
#include <DiaVisualDebugger/Domain/IDebugDomain.h>

#include <cstdio>
#include <cstring>
#include <string>

namespace Cluiche { namespace AppFlow {

namespace
{
    // Canonical group keys from the DiaDebugDomain spec. IDebugDomain::GetGroup()
    // returns a StringCRC, which cannot be reversed to text, so the panel-facing
    // group label is recovered by matching against this table.
    struct GroupEntry
    {
        const char* name;
    };

    const GroupEntry kGroups[] =
    {
        { "CoreDebug"  },
        { "Physics"    },
        { "Animation"  },
        { "Navigation" },
        { "Rendering"  },
        { "Spatial"    },
        { "Entity"     },
        { "AIBehavior" },
    };

    const char* ResolveGroupName(Dia::Core::StringCRC groupId)
    {
        for (const GroupEntry& entry : kGroups)
        {
            if (groupId == Dia::Core::StringCRC(entry.name))
                return entry.name;
        }
        return "CoreDebug";
    }

    // "#rrggbb" — the panel sets each card's --accent CSS variable from this.
    void FormatAccentHex(const Dia::Core::RGBA& colour, char (&out)[8])
    {
        snprintf(out, sizeof(out), "#%02x%02x%02x",
                 static_cast<unsigned int>(colour.R()),
                 static_cast<unsigned int>(colour.G()),
                 static_cast<unsigned int>(colour.B()));
    }
}

const Dia::Core::StringCRC DebugPanelPageModule::kTypeId("DebugPanelPageModule");

DebugPanelPageModule::DebugPanelPageModule(const Dia::Core::StringCRC& instanceId)
    : Module(instanceId)
{}

Dia::ApplicationFlow::StartResult DebugPanelPageModule::DoStart()
{
    UIModule* ui = mUI.Get();
    if (ui == nullptr || !ui->HasStarted())
        return Dia::ApplicationFlow::StartResult::kLoading;

    if (!mLoaded)
    {
        mPage.InitializePage();
        ui->LoadPage(mPage);
        mLoaded = true;
        DIA_LOG_INFO("Debug", "DebugPanelPageModule loaded debug-panel.html");
    }

    return Dia::ApplicationFlow::StartResult::kReady;
}

void DebugPanelPageModule::DoUpdate(float /*dt*/)
{
    DIA_TRACE_ZONE("DebugPanelPageModule.Update", Dia::Observation::Trace::Category::kDiaApplicationFlow);

    PushDomainStatesToPanel();
}

Dia::ApplicationFlow::StopResult DebugPanelPageModule::DoStop()
{
    if (mLoaded)
    {
        if (UIModule* ui = mUI.Get())
            ui->UnloadPage();
        mLoaded = false;
        DIA_LOG_INFO("Debug", "DebugPanelPageModule unloaded debug-panel.html");
    }

    return Dia::ApplicationFlow::StopResult::kDone;
}

void DebugPanelPageModule::OnConnectStreams(Dia::ApplicationFlow::Application& app)
{
    mDomainRegistry.Connect(app);
    mPanelCommands.Connect(app);
}

void DebugPanelPageModule::OnCommand(const char* domainId, const char* cmd, const char* argsJson)
{
    if (domainId == nullptr || cmd == nullptr)
        return;

    DebugPanelCommandEvent event{};
    event.domainId = Dia::Core::StringCRC(domainId);
    event.cmd      = Dia::Core::StringCRC(cmd);

    if (argsJson != nullptr)
    {
        strncpy(event.argsJson, argsJson, sizeof(event.argsJson) - 1);
        event.argsJson[sizeof(event.argsJson) - 1] = '\0';
    }

    DIA_LOG_DEBUG("Debug", "DebugPanelPageModule: command '%s' -> '%s' %s", cmd, domainId, event.argsJson);
    mPanelCommands.Send(event);
}

void DebugPanelPageModule::PushDomainStatesToPanel()
{
    UIModule* ui = mUI.Get();
    if (ui == nullptr || !mLoaded)
        return;

    Dia::UI::IUISystem* uiSystem = ui->GetUISystem();
    if (uiSystem == nullptr || !uiSystem->IsPageLoaded())
        return;

    if (!mDomainRegistry.IsAvailable())
        return;

    const Dia::VisualDebugger::DiaDebugDomainRegistry& registry = mDomainRegistry.Get();

    Json::Value root(Json::objectValue);
    registry.VisitAll([&root](Dia::VisualDebugger::IDebugDomain& domain)
    {
        Json::Value state(Json::objectValue);
        domain.GetJSONState(state);

        const char* displayName = domain.GetDisplayName();
        if (displayName == nullptr)
            return;

        char accentHex[8] = {};
        FormatAccentHex(domain.GetAccentColour(), accentHex);

        state["_displayName"] = displayName;
        state["_group"]       = ResolveGroupName(domain.GetGroup());
        state["_description"] = domain.GetDescription() ? domain.GetDescription() : "";
        state["_accent"]      = accentHex;

        root[displayName] = state;
    });

    Json::FastWriter writer;
    writer.omitEndingLineFeed();
    const std::string json = writer.write(root);

    uiSystem->CallJSFunction("updateDomainState", json.c_str());
}

} } // namespace Cluiche::AppFlow

namespace { using DebugPanelPageModule_ = Cluiche::AppFlow::DebugPanelPageModule; }
DIA_MODULE(DebugPanelPageModule_);
DIA_DESCRIBE(DebugPanelPageModule_::kTypeId, "Ultralight HTML debug panel overlay: domain accordions, drawer toggles, live stats, scale sliders.");

#endif // DIA_DEBUG
