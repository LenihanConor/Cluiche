#include "DiaEconomyInspector/DiaEconomyInspectorPlugin.h"
#include <DiaEditor/Plugin/EditorPluginRegistrationMacros.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaObservation/Log/DiaLog.h>

using namespace Dia::EconomyInspector;

REGISTER_EDITOR_PLUGIN(DiaEconomyInspectorPlugin, "EconomyInspector")

namespace Dia::EconomyInspector {

DiaEconomyInspectorPlugin::DiaEconomyInspectorPlugin()
    : LiveConnectionPluginBase({
        "Economy Inspector", "1.0.0",
        "Live view of economy simulation — pools, modifiers, events, schema",
        "dia://plugins/economyinspector/index.html",
        Dia::Editor::LayoutMode::kDockable,
        nullptr, nullptr, false
    }, "economy_inspector")
{}

void DiaEconomyInspectorPlugin::OnLivePluginLoad()
{
    mInstancesController.Init(GetBridge());
    mModifiersController.Init(GetBridge());
    mEventsController.Init(GetBridge());
    mSchemaController.Init(GetBridge());

    RegisterGameTopic(
        Dia::Core::StringCRC{"economy.instances"},
        [this](const Json::Value& payload) { mInstancesController.OnPayload(payload); });

    RegisterGameTopic(
        Dia::Core::StringCRC{"economy.modifiers"},
        [this](const Json::Value& payload) { mModifiersController.OnPayload(payload); });

    RegisterGameTopic(
        Dia::Core::StringCRC{"economy.events"},
        [this](const Json::Value& payload) { mEventsController.OnPayload(payload); });

    RegisterGameTopic(
        Dia::Core::StringCRC{"economy.schema"},
        [this](const Json::Value& payload) { mSchemaController.OnPayload(payload); });
}

void DiaEconomyInspectorPlugin::OnLivePluginUnload()
{
    DIA_LOG_INFO("EconomyInspector", "Plugin unloaded");
}

void DiaEconomyInspectorPlugin::OnGameConnected()
{
    DIA_LOG_INFO("EconomyInspector", "Game connected");
}

void DiaEconomyInspectorPlugin::OnGameDisconnected()
{
    DIA_LOG_INFO("EconomyInspector", "Game disconnected");
}

} // namespace Dia::EconomyInspector
