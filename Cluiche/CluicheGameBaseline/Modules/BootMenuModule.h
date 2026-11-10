#pragma once
#include <DiaApplicationFlow/RenderModule.h>
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaApplicationFlow/ModuleRefV2.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaObservation/Metric/MetricRegistry.h>
#include <DiaStreams/EventStreamWriter.h>
#include "Modules/DebugUIModule.h"
#include "Modules/DebugServerHostModule.h"
#include "Modules/StageStatusModule.h"
#include "Modules/BootMenuLogic.h"
#include "Types/RenderToSimNavRequest.h"

namespace Cluiche { namespace AppFlow {

class BootMenuModule : public Dia::ApplicationFlow::RenderModule {
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kRender;
    static constexpr const char* kDescription = "Boot stage navigation menu with optional per-stage status badges";
    explicit BootMenuModule(const Dia::Core::StringCRC& instanceId);

protected:
    Dia::ApplicationFlow::StartResult DoStart() override;
    void DoUpdate(const Dia::SimTime::RenderTimeContext& ctx) override;
    Dia::ApplicationFlow::StopResult DoStop() override;
    void OnConnectStreams(Dia::ApplicationFlow::Application& app) override;
    void OnConfigure(const char* configJson) override;

private:
    void CacheNavigableStages();
    void DrawMenu();
    void RefreshProvider();

    Dia::ApplicationFlow::ModuleRef<DebugUIModule> mDebugUI{this, Dia::Core::StringCRC("DebugUI")};
    Dia::ApplicationFlow::ModuleRef<DebugServerHostModule> mDebugServer{this, Dia::Core::StringCRC("DebugServerHostModule")};
    // Optional: absent or inactive means no badge column and no counts footer.
    Dia::ApplicationFlow::ModuleRef<StageStatusModule> mStageStatus{this, Dia::Core::StringCRC("StageStatus")};
    Dia::ApplicationFlow::EventStreamWriter<Cluiche::AppFlow::RenderToSimNavRequest> mNavRequest{this, "BootMenuNavRequest"};

    static constexpr unsigned int kMaxStages = 32;
    Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, kMaxStages> mNavigableStages;
    BootMenuLogic mLogic;
    char mTitle[128] = {};
    char mProject[128] = {};
    int mSelectedIndex = -1;
    bool mFirstDraw = true;
    bool mFrameWasActive = false;

    Dia::Observation::Metric::Gauge* mStageCountGauge = nullptr;
    Dia::Observation::Metric::Counter* mLaunchCounter = nullptr;
};

} } // namespace Cluiche::AppFlow
