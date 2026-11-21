#pragma once
#include <DiaApplicationFlow/SimModule.h>
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaApplicationFlow/ModuleRefV2.h>
#include <DiaAssetRuntime/AssetRuntime.h>
#include <DiaAutomation/AutomationService.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/SimTime/SimTimeContext.h>
#include <DiaStreams/ServiceStreamReader.h>
#include "Modules/InputStreamModule.h"
#include "Types/AssetLoadStatus.h"

namespace CoW {

// Placeholder game logic for the FrontEnd stage (SimPU).
// Waits for the stage's assets, logs once it is running, then idles.
// Escape returns to the Boot menu.
class FrontEndModule : public Dia::ApplicationFlow::SimModule
{
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kSim;
    static constexpr const char* kDescription = "FrontEnd stage logic (placeholder: logs and idles)";
    explicit FrontEndModule(const Dia::Core::StringCRC& instanceId);

protected:
    Dia::ApplicationFlow::StartResult DoStart() override;
    void DoUpdate(const Dia::SimTime::SimTimeContext& ctx) override;
    Dia::ApplicationFlow::StopResult DoStop() override;
    void OnConnectStreams(Dia::ApplicationFlow::Application& app) override;

private:
    void NavigateTo(const Dia::Core::StringCRC& target);

    Dia::ApplicationFlow::ModuleRef<Cluiche::AppFlow::InputStreamModule> mInput{this};

    // Stage asset load progress, published by AssetServiceModule (MainPU).
    Dia::ApplicationFlow::ServiceStreamReader<Cluiche::AppFlow::AssetLoadStatus> mAssetLoadStatus{this, "AssetLoadStatus"};
    // Asset lookups for stage content.
    Dia::ApplicationFlow::ServiceStreamReader<Dia::AssetRuntime::AssetRuntime> mAssetRuntime{this, "AssetRuntimeService"};
    // Navigation hold: AutomationModule holds every transition until released.
    Dia::ApplicationFlow::ServiceStreamReader<Dia::Automation::AutomationService> mAutomation{this, "AutomationService"};

    bool mWaitLogged = false;
    bool mExitRequested = false;
};

} // namespace CoW
