#pragma once
#include <DiaApplicationFlow/SimModule.h>
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaApplicationFlow/IApplicationControl.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/SimTime/SimTimeContext.h>
#include <DiaStreams/EventStreamReader.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaStreams/Event.h>
#include "Types/RenderToSimNavRequest.h"
#include "Types/StageLaunchRequest.h"

namespace Dia { namespace Automation { class AutomationService; } }
namespace Dia { namespace ApplicationFlow { template<typename T> class ServiceStreamReader; } }

namespace Cluiche { namespace AppFlow {

// Reads navigation requests from RenderPU modules and executes TransitionTo()
// on the sim thread. Enforces the rule that application flow decisions are made
// on SimPU, not on the render thread.
class SimNavigationHandlerModule : public Dia::ApplicationFlow::SimModule
{
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kSim;
    static constexpr const char* kDescription = "Executes navigation requests sent by RenderPU modules";

    // Routes one navigation: releases an active automation hold (carrying the preset), otherwise
    // calls app.TransitionTo(target[, preset]).  Empty preset behaves as a plain TransitionTo.
    static void RouteNavigation(Dia::ApplicationFlow::IApplicationControl& app,
                                Dia::Automation::AutomationService* automation,
                                const Dia::Core::StringCRC& target,
                                const Dia::Core::StringCRC& preset);

    // Routes a stages.launch request (StageCatalogueModule, via the StageLaunchRequest stream). An empty stage is ignored.
    static void HandleLaunchRequest(Dia::ApplicationFlow::IApplicationControl& app,
                                    Dia::Automation::AutomationService* automation,
                                    const StageLaunchRequest& request);

    enum class InitialLaunchResult { kNone, kIssued, kSkippedHold, kUnknownStage };

    // Once Boot is the running stage, takes the --stage/--preset launch (if any) and issues it.
    // Skipped (but consumed) while an automation hold is active; an unknown stage logs an Error and
    // stays at Boot.  kNone when nothing was pending or the app is not at Boot yet (nothing consumed).
    static InitialLaunchResult IssueInitialLaunch(Dia::ApplicationFlow::IApplicationControl& app,
                                                  Dia::Automation::AutomationService* automation);

    explicit SimNavigationHandlerModule(const Dia::Core::StringCRC& instanceId);
    ~SimNavigationHandlerModule() override;

protected:
    Dia::ApplicationFlow::StartResult DoStart() override;
    void DoUpdate(const Dia::SimTime::SimTimeContext& ctx) override;
    Dia::ApplicationFlow::StopResult DoStop() override;
    void OnConnectStreams(Dia::ApplicationFlow::Application& app) override;

private:
    void Navigate(const Dia::Core::StringCRC& target, const Dia::Core::StringCRC& preset);

    Dia::ApplicationFlow::EventStreamReader<RenderToSimNavRequest> mBootNavInput{this, "BootMenuNavRequest"};
    Dia::ApplicationFlow::EventStreamReader<RenderToSimNavRequest> mHUDNavInput{this,  "HUDNavRequest"};
    Dia::ApplicationFlow::EventStreamReader<StageLaunchRequest>    mLaunchInput{this, "StageLaunchRequest"};

    bool mInitialLaunchDone = false; // set once the --stage/--preset launch has been taken (issued, skipped or refused)

    // Heap-allocated to avoid pulling AutomationService.h into every TU.
    Dia::ApplicationFlow::ServiceStreamReader<Dia::Automation::AutomationService>* mAutomationService = nullptr;
};

} } // namespace Cluiche::AppFlow
