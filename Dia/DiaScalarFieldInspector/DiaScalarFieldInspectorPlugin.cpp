#include "DiaScalarFieldInspector/DiaScalarFieldInspectorPlugin.h"
#include <DiaEditor/Plugin/EditorPluginRegistrationMacros.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaObservation/Log/DiaLog.h>

using namespace Dia::ScalarField;

REGISTER_EDITOR_PLUGIN(DiaScalarFieldInspectorPlugin, "DiaScalarFieldInspector")

namespace Dia::ScalarField {

DiaScalarFieldInspectorPlugin::DiaScalarFieldInspectorPlugin()
    : LiveConnectionPluginBase({
        "ScalarField Inspector",
        "1.0.0",
        "Live view of registered scalar fields with heatmap, history, and write authoring",
        "dia://plugins/scalarfieldinspector/index.html",
        Dia::Editor::LayoutMode::kDockable,
        nullptr, nullptr, false
    }, "scalarfield_inspector")
{}

void DiaScalarFieldInspectorPlugin::OnLivePluginLoad()
{
    DIA_LOG_INFO("ScalarFieldInspector", "DiaScalarFieldInspectorPlugin: OnLivePluginLoad");

    RegisterGameTopic(
        Dia::Core::StringCRC{"scalarfield.state"},
        [this](const Json::Value& payload) { OnScalarFieldStateUpdate(payload); });

    RegisterHandler(
        Dia::Core::StringCRC{"scalarfield_inspector.write_point"},
        [this](const Json::Value& req) { return HandleWritePoint(req); });

    RegisterHandler(
        Dia::Core::StringCRC{"scalarfield_inspector.write_radial"},
        [this](const Json::Value& req) { return HandleWriteRadial(req); });

    RegisterHandler(
        Dia::Core::StringCRC{"scalarfield_inspector.write_box"},
        [this](const Json::Value& req) { return HandleWriteBox(req); });

    DIA_LOG_INFO("ScalarFieldInspector", "DiaScalarFieldInspectorPlugin: OnLivePluginLoad complete");
}

void DiaScalarFieldInspectorPlugin::OnLivePluginUnload()
{
    DIA_LOG_INFO("ScalarFieldInspector", "DiaScalarFieldInspectorPlugin: OnLivePluginUnload");
}

void DiaScalarFieldInspectorPlugin::OnGameConnected()
{
    DIA_LOG_INFO("ScalarFieldInspector", "DiaScalarFieldInspectorPlugin: Connected to game");
}

void DiaScalarFieldInspectorPlugin::OnGameDisconnected()
{
    DIA_LOG_INFO("ScalarFieldInspector", "DiaScalarFieldInspectorPlugin: Disconnected from game");
}

void DiaScalarFieldInspectorPlugin::OnScalarFieldStateUpdate(const Json::Value& payload)
{
    GetBridge()->NotifyUIDataChanged("scalarfield_inspector.state", payload);
}

Json::Value DiaScalarFieldInspectorPlugin::HandleWritePoint(const Json::Value& req)
{
    GetGameConnection()->SendCommand("scalarfield.write_point", req);
    return MakeSuccessResponse();
}

Json::Value DiaScalarFieldInspectorPlugin::HandleWriteRadial(const Json::Value& req)
{
    GetGameConnection()->SendCommand("scalarfield.write_radial", req);
    return MakeSuccessResponse();
}

Json::Value DiaScalarFieldInspectorPlugin::HandleWriteBox(const Json::Value& req)
{
    GetGameConnection()->SendCommand("scalarfield.write_box", req);
    return MakeSuccessResponse();
}

} // namespace Dia::ScalarField
