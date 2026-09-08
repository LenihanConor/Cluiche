#pragma once
#include <DiaCore/Json/external/json/json.h>

namespace Dia { namespace Editor { class WebUIBridge; } }

namespace Dia::EntityInspector {

    // Drives the Queries tab. Parses the queries array from entity.inspect.
    class QueryBrowserController
    {
    public:
        void Activate(Dia::Editor::WebUIBridge* bridge);
        void Deactivate();
        void OnInspectPayload(const Json::Value& payload);

    private:
        Dia::Editor::WebUIBridge* mBridge = nullptr;
    };

} // namespace Dia::EntityInspector
