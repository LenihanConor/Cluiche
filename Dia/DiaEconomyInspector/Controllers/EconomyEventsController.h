#pragma once
namespace Json { class Value; }
namespace Dia::Editor { class WebUIBridge; }

namespace Dia::EconomyInspector {
class EconomyEventsController {
public:
    void Init(Dia::Editor::WebUIBridge* bridge);
    void OnPayload(const Json::Value& payload);
private:
    Dia::Editor::WebUIBridge* mBridge = nullptr;
};
} // namespace Dia::EconomyInspector
