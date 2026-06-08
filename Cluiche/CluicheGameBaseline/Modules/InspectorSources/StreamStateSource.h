#pragma once
#include <DiaDebugServer/InspectorDataSourceBase.h>
#include <DiaApplicationFlow/IApplicationInspectable.h>

namespace Cluiche { namespace AppFlow {

// Broadcasts app.streams — stream topology array.
// Fires only when stream set or sequence numbers change (change-detected).
class StreamStateSource final : public Dia::DebugServer::ChangeDetectedSourceBase
{
public:
    explicit StreamStateSource(Dia::ApplicationFlow::IApplicationInspectable* app);

    Dia::Core::StringCRC           GetTopic()  const override;
    Dia::DebugServer::SourcePolicy GetPolicy() const override;

protected:
    unsigned int CollectAndHash(Json::Value& payload) override;

private:
    Dia::ApplicationFlow::IApplicationInspectable* mApp = nullptr;
};

}} // namespace Cluiche::AppFlow
