#include "DiaEntityInspector/EntityWatchListController.h"
#include <DiaEditor/UI/WebUIBridge.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaObservation/Log/DiaLog.h>

namespace Dia::EntityInspector {

void EntityWatchListController::Activate(Dia::Editor::WebUIBridge* bridge)
{
    DIA_LOG_INFO("Editor", "EntityWatchListController: Activate");
    mBridge = bridge;
}
void EntityWatchListController::Deactivate()
{
    DIA_LOG_INFO("Editor", "EntityWatchListController: Deactivate");
    mBridge = nullptr;
}

void EntityWatchListController::RegisterHandlers()
{
    if (!mBridge) return;

    mBridge->RegisterRequestHandler(
        Dia::Core::StringCRC("entity_inspector.watch_add"),
        [this](const Json::Value& data) -> Json::Value
        {
            Json::Value result;
            if (!data.isMember("entity") || !data.isMember("component") || !data.isMember("field"))
            {
                result["success"] = false;
                result["error"]   = "missing entity, component, or field";
                return result;
            }
            if (mWatchList.size() >= kMaxWatchEntries)
            {
                result["success"] = false;
                result["error"]   = "watch list full";
                return result;
            }
            Json::Value entry;
            entry["entity"]    = data["entity"];
            entry["component"] = data["component"];
            entry["field"]     = data["field"];
            entry["value"]     = Json::Value(Json::nullValue);
            mWatchList.append(entry);
            DIA_LOG_INFO("Editor", "EntityWatchListController: watch_add entity='%s' component='%s' field='%s'",
                data["entity"].asCString(), data["component"].asCString(), data["field"].asCString());
            PushWatchListToUI();
            result["success"] = true;
            return result;
        });

    mBridge->RegisterRequestHandler(
        Dia::Core::StringCRC("entity_inspector.watch_remove"),
        [this](const Json::Value& data) -> Json::Value
        {
            Json::Value result;
            if (!data.isMember("index"))
            {
                result["success"] = false;
                result["error"]   = "missing index";
                return result;
            }
            const int idx = data["index"].asInt();
            if (idx < 0 || idx >= static_cast<int>(mWatchList.size()))
            {
                result["success"] = false;
                result["error"]   = "index out of range";
                return result;
            }
            Json::Value trimmed(Json::arrayValue);
            for (unsigned int i = 0; i < mWatchList.size(); ++i)
                if (static_cast<int>(i) != idx) trimmed.append(mWatchList[i]);
            mWatchList = trimmed;
            DIA_LOG_INFO("Editor", "EntityWatchListController: watch_remove index=%d", idx);
            PushWatchListToUI();
            result["success"] = true;
            return result;
        });

    mBridge->RegisterRequestHandler(
        Dia::Core::StringCRC("entity_inspector.watch_get"),
        [this](const Json::Value& /*data*/) -> Json::Value
        {
            Json::Value result;
            result["success"]   = true;
            result["watchList"] = mWatchList;
            return result;
        });
}

void EntityWatchListController::UnregisterHandlers()
{
    if (!mBridge) return;
    mBridge->UnregisterRequestHandler(Dia::Core::StringCRC("entity_inspector.watch_add"));
    mBridge->UnregisterRequestHandler(Dia::Core::StringCRC("entity_inspector.watch_remove"));
    mBridge->UnregisterRequestHandler(Dia::Core::StringCRC("entity_inspector.watch_get"));
}

void EntityWatchListController::OnInspectPayload(const Json::Value& payload)
{
    if (payload.isNull() || !payload.isMember("entity")) return;
    const std::string entityName = payload["entity"].get("debug_name", "").asString();

    bool changed = false;
    for (unsigned int i = 0; i < mWatchList.size(); ++i)
    {
        if (mWatchList[i]["entity"].asString() != entityName) continue;
        const std::string compName  = mWatchList[i]["component"].asString();
        const std::string fieldName = mWatchList[i]["field"].asString();

        const Json::Value& comps = payload["components"];
        for (unsigned int ci = 0; ci < comps.size(); ++ci)
        {
            if (comps[ci]["type_name"].asString() != compName) continue;
            const Json::Value& fields = comps[ci]["fields"];
            for (unsigned int fi = 0; fi < fields.size(); ++fi)
            {
                if (fields[fi]["name"].asString() == fieldName)
                {
                    mWatchList[i]["value"] = fields[fi]["value"];
                    changed = true;
                    break;
                }
            }
        }
    }

    if (changed)
        PushWatchListToUI();
}

void EntityWatchListController::OnConnectionStateChanged(bool connected)
{
    if (!connected)
        return;
    // On reconnect, values become stale — clear them so UI shows pending state.
    DIA_LOG_INFO("Editor", "EntityWatchListController: reconnect — clearing %u stale watch values", mWatchList.size());
    for (unsigned int i = 0; i < mWatchList.size(); ++i)
        mWatchList[i]["value"] = Json::Value(Json::nullValue);
    PushWatchListToUI();
}

void EntityWatchListController::PushWatchListToUI()
{
    if (!mBridge) return;
    Json::Value msg;
    msg["watchList"] = mWatchList;
    mBridge->NotifyUIDataChanged("entity_inspector.watch_data", msg);
}

} // namespace Dia::EntityInspector
