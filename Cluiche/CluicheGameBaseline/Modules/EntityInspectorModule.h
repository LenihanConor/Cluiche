#pragma once

#ifdef DIA_DEBUG

#include <DiaApplicationFlow/Module.h>
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaApplicationFlow/ModuleRefV2.h>
#include <DiaStreams/EventStreamWriter.h>
#include <DiaCore/CRC/StringCRC.h>
#include "Modules/EntityModule.h"
#include "Types/EntityInspectEvent.h"

namespace Dia { namespace DebugServer { class DebugServer; } }

namespace Cluiche { namespace AppFlow {

class EntityInspectorModule : public Dia::ApplicationFlow::Module
{
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kSim;
    static constexpr const char* kDescription = "Pushes entity.inspect payloads to the debug server for DiaEntityInspector";

    explicit EntityInspectorModule(const Dia::Core::StringCRC& instanceId);
    ~EntityInspectorModule() override;

protected:
    void                              OnConnectStreams(Dia::ApplicationFlow::Application& app) override;
    Dia::ApplicationFlow::StartResult DoStart()          override;
    void                              DoUpdate(float dt) override;
    Dia::ApplicationFlow::StopResult  DoStop()           override;

private:
    void RegisterHandlers();
    void UnregisterHandlers();
    void PushInspect(uint32_t selectedId);

    Dia::ApplicationFlow::ModuleRef<EntityModule> mEntityRef{this};

    Dia::ApplicationFlow::EventStreamWriter<EntityInspectEvent> mInspectWriter{
        this, Dia::Core::StringCRC("EntityInspectPush")};

    Dia::DebugServer::DebugServer* mDebugServer = nullptr;

    int      mSlowPollCounter = 0;
    uint64_t mFrameCounter    = 0;

    static constexpr int kSlowPollInterval = 30;
};

} } // namespace Cluiche::AppFlow

#endif // DIA_DEBUG
