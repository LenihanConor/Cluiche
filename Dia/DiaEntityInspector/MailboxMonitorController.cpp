#include "DiaEntityInspector/MailboxMonitorController.h"
#include <DiaEditor/UI/WebUIBridge.h>
#include <DiaObservation/Log/DiaLog.h>

namespace Dia::EntityInspector {

void MailboxMonitorController::Activate(Dia::Editor::WebUIBridge* bridge)
{
    DIA_LOG_INFO("Editor", "MailboxMonitorController: Activate");
    mBridge = bridge;
}
void MailboxMonitorController::Deactivate()
{
    DIA_LOG_INFO("Editor", "MailboxMonitorController: Deactivate");
    mBridge = nullptr;
}

void MailboxMonitorController::Clear()
{
    DIA_LOG_INFO("Editor", "MailboxMonitorController: Clear");
    mLog = Json::Value(Json::arrayValue);
    if (mBridge)
    {
        Json::Value msg;
        msg["log"] = mLog;
        mBridge->NotifyUIDataChanged("entity_inspector.mailbox_data", msg);
    }
}

void MailboxMonitorController::OnInspectPayload(const Json::Value& payload)
{
    if (!mBridge || payload.isNull()) return;
    if (!payload.isMember("mailbox_log")) return;

    const Json::Value& newEntries = payload["mailbox_log"];
    for (unsigned int i = 0; i < newEntries.size(); ++i)
    {
        if (mLog.size() >= kRingBufferSize)
        {
            // Drop the oldest entry (index 0) to make room.
            Json::Value trimmed(Json::arrayValue);
            for (unsigned int j = 1; j < mLog.size(); ++j)
                trimmed.append(mLog[j]);
            mLog = trimmed;
        }
        mLog.append(newEntries[i]);
    }

    Json::Value msg;
    msg["log"] = mLog;
    mBridge->NotifyUIDataChanged("entity_inspector.mailbox_data", msg);
}

} // namespace Dia::EntityInspector
