#pragma once
#include <DiaDebugServer/InspectorDataSourceBase.h>
#include <DiaApplicationFlow/IApplicationInspectable.h>

namespace Cluiche { namespace AppFlow {

// Broadcasts app.modules — flat array of {moduleId, puId, lifecycleState}.
// Fires only when module set or state changes (change-detected).
class ModuleStateSource final : public Dia::DebugServer::ChangeDetectedSourceBase
{
public:
    explicit ModuleStateSource(Dia::ApplicationFlow::IApplicationInspectable* app);

    Dia::Core::StringCRC  GetTopic()  const override;
    Dia::DebugServer::SourcePolicy GetPolicy() const override;

protected:
    unsigned int CollectAndHash(Json::Value& payload) override;

private:
    Dia::ApplicationFlow::IApplicationInspectable* mApp = nullptr;
};

}} // namespace Cluiche::AppFlow
