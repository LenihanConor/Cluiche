#pragma once

#ifdef DIA_DEBUG

#include <DiaApplicationFlow/SimModule.h>
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaDebugServer/IInspectorDataSource.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/SimTime/SimTimeContext.h>
#include <memory>

namespace Dia { namespace DebugServer { class DebugServer; } }
namespace Dia { namespace Economy {
    class EconomySystem;
    class EconomySchema;
} }

namespace Cluiche { namespace AppFlow {

class EconomyInspectorModule : public Dia::ApplicationFlow::SimModule
{
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kSim;
    static constexpr const char* kDescription = "Pushes economy inspector payloads to the debug server";

    explicit EconomyInspectorModule(const Dia::Core::StringCRC& instanceId);
    ~EconomyInspectorModule() override;

    // Called by a game stage after its EconomySystem is ready.
    // Creates and activates the 4 sources.
    void Bind(Dia::Economy::EconomySystem& system,
              const Dia::Economy::EconomySchema& schema);

    // Called before the stage tears down its EconomySystem.
    void Unbind();

protected:
    void                              OnConnectStreams(Dia::ApplicationFlow::Application& app) override;
    Dia::ApplicationFlow::StartResult DoStart()          override;
    void                              DoUpdate(const Dia::SimTime::SimTimeContext& ctx) override;
    Dia::ApplicationFlow::StopResult  DoStop()            override;

private:
    void ActivateSources();
    void DeactivateSources();

    Dia::DebugServer::DebugServer* mDebugServer = nullptr;
    bool mSourcesActive = false;

    std::unique_ptr<Dia::DebugServer::IInspectorDataSource> mSchemaSource;
    std::unique_ptr<Dia::DebugServer::IInspectorDataSource> mInstancesSource;
    std::unique_ptr<Dia::DebugServer::IInspectorDataSource> mModifiersSource;
    std::unique_ptr<Dia::DebugServer::IInspectorDataSource> mEventsSource;
};

} } // namespace Cluiche::AppFlow

#endif // DIA_DEBUG
