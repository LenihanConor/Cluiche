#pragma once
#include <DiaApplicationFlow/Module.h>
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaStreams/EventStreamReader.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaStreams/Event.h>
#include "Types/RenderToSimNavRequest.h"

namespace Dia { namespace Automation { class AutomationService; } }
namespace Dia { namespace ApplicationFlow { template<typename T> class ServiceStreamReader; } }

namespace Cluiche { namespace AppFlow {

// Reads navigation requests from RenderPU modules and executes TransitionTo()
// on the sim thread. Enforces the rule that application flow decisions are made
// on SimPU, not on the render thread.
class SimNavigationHandlerModule : public Dia::ApplicationFlow::Module
{
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kSim;
    static constexpr const char* kDescription = "Executes navigation requests sent by RenderPU modules";

    explicit SimNavigationHandlerModule(const Dia::Core::StringCRC& instanceId);
    ~SimNavigationHandlerModule() override;

protected:
    Dia::ApplicationFlow::StartResult DoStart() override;
    void DoUpdate(float dt) override;
    Dia::ApplicationFlow::StopResult DoStop() override;
    void OnConnectStreams(Dia::ApplicationFlow::Application& app) override;

private:
    void Navigate(const Dia::Core::StringCRC& target);

    Dia::ApplicationFlow::EventStreamReader<RenderToSimNavRequest> mBootNavInput{this, "BootMenuNavRequest"};
    Dia::ApplicationFlow::EventStreamReader<RenderToSimNavRequest> mHUDNavInput{this,  "HUDNavRequest"};

    // Heap-allocated to avoid pulling AutomationService.h into every TU.
    Dia::ApplicationFlow::ServiceStreamReader<Dia::Automation::AutomationService>* mAutomationService = nullptr;
};

} } // namespace Cluiche::AppFlow
