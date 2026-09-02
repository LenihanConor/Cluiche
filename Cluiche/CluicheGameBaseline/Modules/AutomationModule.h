////////////////////////////////////////////////////////////////////////////////
// Filename: AutomationModule.h
// CluicheGameBaseline — wires AutomationService into the app lifecycle.
//
// Instantiates AutomationService in DoStart, registers commands, enables
// navigation hold. AutomationService lives as long as this module lives.
// This module uses stages: ["all"] so it runs for the full session.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include <DiaApplicationFlow/MainModule.h>
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Memory/UniquePtr.h>
#include <DiaCore/SimTime/SimTimeContext.h>

namespace Dia { namespace Automation { class AutomationService; } }
namespace Dia { namespace ApplicationFlow { template<typename T> class ServiceStreamWriter; } }

namespace Cluiche { namespace AppFlow {

class AutomationModule : public Dia::ApplicationFlow::MainModule {
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kMain;
    static constexpr const char* kDescription = "Checkpoint registry and automation command surface";
    explicit AutomationModule(const Dia::Core::StringCRC& instanceId);
    ~AutomationModule();

    Dia::Automation::AutomationService* GetService() { return mService.Get(); }

protected:
    Dia::ApplicationFlow::StartResult DoStart() override;
    void DoUpdate(const Dia::SimTime::MainTimeContext& ctx) override;
    Dia::ApplicationFlow::StopResult DoStop() override;
    void OnConnectStreams(Dia::ApplicationFlow::Application& app) override;

private:
    Dia::Core::UniquePtr<Dia::Automation::AutomationService> mService;
    // Heap-allocated to avoid pulling Application.h into every TU via AutomationService.h
    Dia::ApplicationFlow::ServiceStreamWriter<Dia::Automation::AutomationService>* mAutomationServiceStream = nullptr;
};

} } // namespace Cluiche::AppFlow
