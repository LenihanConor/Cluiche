#include "DiaEntityInspector/EntityInspectorController.h"
#include <DiaEditor/UI/WebUIBridge.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaObservation/Log/DiaLog.h>

namespace Dia::EntityInspector {

void EntityInspectorController::Activate(Dia::Editor::WebUIBridge* bridge)
{
    DIA_LOG_INFO("Editor", "EntityInspectorController: Activate");
    mBridge = bridge;
}

void EntityInspectorController::Deactivate()
{
    DIA_LOG_INFO("Editor", "EntityInspectorController: Deactivate");
    UnregisterHandlers();
    mBridge = nullptr;
}

void EntityInspectorController::RegisterHandlers(SchemaSendFn send)
{
    if (!mBridge) return;
    mSend = std::move(send);

    mBridge->RegisterRequestHandler(
        Dia::Core::StringCRC("entity_inspector.schema_get"),
        [this](const Json::Value& data) -> Json::Value
        {
            Json::Value result;
            if (!data.isObject() || !data["prefix"].isString() || data["prefix"].asString().empty())
            {
                result["success"] = false;
                result["error"]   = "missing prefix";
                return result;
            }
            const std::string prefix = data["prefix"].asString();

            Json::Value args(Json::objectValue);
            args["prefix"] = prefix;

            const bool sent = mSend && mSend("valueschema.get", args,
                [this, prefix](bool success, const Json::Value& reply)
                {
                    Json::Value entries(Json::arrayValue);
                    if (success && reply.isObject() && reply["entries"].isArray())
                        entries = reply["entries"];
                    PushSchemaData(prefix, success, entries);
                });

            if (!sent)
            {
                DIA_LOG_DEBUG("Editor", "EntityInspectorController: schema_get '%s' not sent (no game link)", prefix.c_str());
                PushSchemaData(prefix, false, Json::Value(Json::arrayValue));
            }
            result["success"] = sent;
            return result;
        });
}

void EntityInspectorController::UnregisterHandlers()
{
    if (mBridge)
        mBridge->UnregisterRequestHandler(Dia::Core::StringCRC("entity_inspector.schema_get"));
    mSend = nullptr;
}

void EntityInspectorController::PushSchemaData(const std::string& prefix, bool success, const Json::Value& entries)
{
    if (!mBridge) return;
    Json::Value payload(Json::objectValue);
    payload["prefix"]  = prefix;
    payload["success"] = success;
    payload["entries"] = entries;
    mBridge->NotifyUIDataChanged("entity_inspector.schema_data", payload);
}

void EntityInspectorController::OnInspectPayload(const Json::Value& payload)
{
    DIA_LOG_INFO("Editor", "EntityInspectorController::OnInspectPayload bridge=%s null=%d",
        mBridge ? "ok" : "null", payload.isNull() ? 1 : 0);
    if (!mBridge || payload.isNull()) return;
    DIA_LOG_INFO("Editor", "EntityInspectorController: pushing inspect_data to UI");
    mBridge->NotifyUIDataChanged("entity_inspector.inspect_data", payload);
}

} // namespace Dia::EntityInspector
