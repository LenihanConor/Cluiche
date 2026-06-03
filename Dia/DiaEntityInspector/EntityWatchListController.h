#pragma once
#include <DiaCore/Json/external/json/json.h>

namespace Dia { namespace Editor { class WebUIBridge; } }

namespace Dia::EntityInspector {

    // Drives the Watch tab. Stores persistent (entity, component, field) triples
    // that survive reconnects. Stable reference uses entity debug name (not handle).
    class EntityWatchListController
    {
    public:
        static constexpr unsigned int kMaxWatchEntries = 32;

        void Activate(Dia::Editor::WebUIBridge* bridge);
        void Deactivate();
        void RegisterHandlers();
        void UnregisterHandlers();

        // Called each time a new entity.inspect payload arrives.
        // Updates the current values of any watched fields matching this entity.
        void OnInspectPayload(const Json::Value& payload);

        // Called on reconnect to rebind watch entries by debug name.
        void OnConnectionStateChanged(bool connected);

    private:
        Dia::Editor::WebUIBridge* mBridge = nullptr;
        Json::Value mWatchList{Json::arrayValue};

        void PushWatchListToUI();
    };

} // namespace Dia::EntityInspector
