#pragma once
////////////////////////////////////////////////////////////////////////////////
// Filename: DebugServerHostModule.h
//
// DiaApplicationFlow v2 Module adapter for Dia::DebugServer::DebugServer.
// Owns a DebugServer instance and drives its Start / Tick / Stop from the
// v2 module lifecycle.  Implements IDebugStateProvider to translate the
// v2 IApplicationInspectable interface into the narrow shape DebugServer
// consumes.
//
// This adapter lives in CluicheGameBaseline so that Dia/DiaDebugServer
// itself has zero dependency on DiaApplicationFlow.
////////////////////////////////////////////////////////////////////////////////

#include <DiaApplicationFlow/Module.h>
#include <DiaDebugServer/DebugServer.h>
#include <DiaDebugServer/IDebugStateProvider.h>
#include <DiaCore/CRC/StringCRC.h>

namespace Cluiche { namespace AppFlow {

class DebugServerHostModule
    : public Dia::ApplicationFlow::Module
    , private Dia::DebugServer::IDebugStateProvider
{
public:
    static const Dia::Core::StringCRC kTypeId;

    explicit DebugServerHostModule(const Dia::Core::StringCRC& instanceId);
    ~DebugServerHostModule() override;

    // Exposed so game code can reach the underlying server (subscribers,
    // query registry, etc.) in its own DoStart/DoUpdate after this module
    // has started.  Returns the same pointer for the lifetime of the
    // module — may be null before DoStart.
    Dia::DebugServer::DebugServer* GetServer() { return &mServer; }

protected:
    // v2 Module lifecycle
    void OnConfigure(const char* configJson) override;
    Dia::ApplicationFlow::StartResult DoStart() override;
    void DoUpdate(float deltaTime) override;
    Dia::ApplicationFlow::StopResult DoStop() override;

    // IDebugStateProvider — translates v2 IApplicationInspectable into the
    // debug-server-local types so DiaDebugServer doesn't depend on
    // DiaApplicationFlow.
    Dia::Core::StringCRC GetCurrentStage() const override;
    bool IsTransitioning() const override;
    bool IsShuttingDown() const override;
    void GetProcessingUnitIds(
        Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 4>& out) const override;
    void GetModulesInPU(
        const Dia::Core::StringCRC& puId,
        Dia::Core::Containers::DynamicArrayC<Dia::DebugServer::DebugModuleInfo, 64>& out) const override;

private:
    Dia::DebugServer::DebugServer mServer;
};

} } // namespace Cluiche::AppFlow
