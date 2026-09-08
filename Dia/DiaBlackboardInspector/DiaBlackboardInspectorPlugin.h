#pragma once

#include <DiaEditor/Plugin/LiveConnectionPluginBase.h>

namespace Dia::Editor {

class DiaBlackboardInspectorPlugin final : public LiveConnectionPluginBase
{
public:
    DiaBlackboardInspectorPlugin()
        : LiveConnectionPluginBase({
            "Blackboard Inspector",
            "1.0.0",
            "Live view of all registered blackboards, slots, and observers",
            "dia://plugins/blackboardinspector/index.html",
            Dia::Editor::LayoutMode::kDockable,
            nullptr,
            nullptr,
            false
        }, "blackboard_inspector")
    {}

protected:
    void OnLivePluginLoad()      override;
    void OnLivePluginUnload()    override;
    void OnGameConnected()       override;
    void OnGameDisconnected()    override;

private:
    void OnBlackboardStateUpdate(const Json::Value& payload);
};

} // namespace Dia::Editor
