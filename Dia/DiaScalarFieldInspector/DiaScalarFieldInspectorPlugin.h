#pragma once
#include <DiaEditor/Plugin/LiveConnectionPluginBase.h>

namespace Dia::ScalarField {

class DiaScalarFieldInspectorPlugin final : public Dia::Editor::LiveConnectionPluginBase
{
public:
    DiaScalarFieldInspectorPlugin();

protected:
    void OnLivePluginLoad()   override;
    void OnLivePluginUnload() override;
    void OnGameConnected()    override;
    void OnGameDisconnected() override;

private:
    void OnScalarFieldStateUpdate(const Json::Value& payload);
    Json::Value HandleWritePoint (const Json::Value& req);
    Json::Value HandleWriteRadial(const Json::Value& req);
    Json::Value HandleWriteBox   (const Json::Value& req);
};

} // namespace Dia::ScalarField
