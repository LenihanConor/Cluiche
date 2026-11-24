#pragma once
#include <DiaCore/Json/external/json/json.h>
#include <functional>
#include <string>

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

        // Sends a command to the live game and delivers the reply. Returns false when there
        // is no connected game (the callback is then never invoked).
        using SchemaReplyFn = std::function<void(bool success, const Json::Value& result)>;
        using SchemaSendFn  = std::function<bool(const char* command, const Json::Value& args, SchemaReplyFn reply)>;

        // Registers entity_inspector.schema_get {prefix}: forwards `valueschema.get {prefix}` to the
        // game and pushes the reply on entity_inspector.schema_data {prefix, success, entries}.
        void RegisterHandlers(SchemaSendFn send);
        void UnregisterHandlers();

    private:
        void PushSchemaData(const std::string& prefix, bool success, const Json::Value& entries);

        Dia::Editor::WebUIBridge* mBridge = nullptr;
        SchemaSendFn              mSend;
    };

} // namespace Dia::EntityInspector
