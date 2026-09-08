#pragma once
#include <DiaCore/Json/external/json/json.h>

namespace Dia { namespace Editor { class WebUIBridge; } }

namespace Dia::EntityInspector {

    // Drives the Fields tab. Receives entity.inspect payloads and exposes
    // component+field data to the UI via entity_inspector.* handlers.
    class EntityInspectorController
    {
    public:
        void Activate(Dia::Editor::WebUIBridge* bridge);
        void Deactivate();
        void OnInspectPayload(const Json::Value& payload);

    private:
        Dia::Editor::WebUIBridge* mBridge = nullptr;
    };

} // namespace Dia::EntityInspector
