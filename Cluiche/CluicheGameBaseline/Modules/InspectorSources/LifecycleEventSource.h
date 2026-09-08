#pragma once
#include <DiaDebugServer/InspectorDataSourceBase.h>
#include <DiaApplicationFlow/IApplicationInspectable.h>

namespace Cluiche { namespace AppFlow {

// Broadcasts app.event — stage transitions, module state changes, shutdown.
// Event-driven: fires from the $lifecycle stream tap. Unlimited rate.
class LifecycleEventSource final : public Dia::DebugServer::EventDrivenSourceBase
{
public:
    LifecycleEventSource(Dia::ApplicationFlow::IApplicationInspectable* app,
                         const double* uptimeSecs);

    Dia::Core::StringCRC           GetTopic()  const override;
    Dia::DebugServer::SourcePolicy GetPolicy() const override;
    void Activate(Dia::DebugServer::DebugServer* server) override;
    void Deactivate() override;

private:
    Dia::ApplicationFlow::IApplicationInspectable* mApp        = nullptr;
    const double*                                  mUptimeSecs = nullptr;
    unsigned int                                   mTapId      = 0;
};

}} // namespace Cluiche::AppFlow
