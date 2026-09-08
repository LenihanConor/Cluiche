#pragma once
#include <DiaCore/Json/external/json/json.h>

namespace Dia { namespace Editor { class WebUIBridge; } }

namespace Dia::EntityInspector {

    // Drives the Mailbox tab. Maintains a client-side ring buffer of up to 64
    // mailbox_log entries received via entity.inspect payloads.
    class MailboxMonitorController
    {
    public:
        static constexpr unsigned int kRingBufferSize = 64;

        void Activate(Dia::Editor::WebUIBridge* bridge);
        void Deactivate();
        void OnInspectPayload(const Json::Value& payload);
        void Clear();

    private:
        Dia::Editor::WebUIBridge* mBridge = nullptr;
        Json::Value mLog{Json::arrayValue};
    };

} // namespace Dia::EntityInspector
