#pragma once
#include <DiaApplicationFlow/Module.h>
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaApplicationFlow/ModuleRefV2.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaObservation/Metric/MetricRegistry.h>
#include <DiaStreams/EventStreamWriter.h>
#include "Modules/DebugUIModule.h"
#include "Modules/DebugServerHostModule.h"
#include "Types/RenderToSimNavRequest.h"

namespace Cluiche { namespace AppFlow {

class BootMenuModule : public Dia::ApplicationFlow::Module {
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kRender;
    static constexpr const char* kDescription = "Boot stage navigation menu";
    explicit BootMenuModule(const Dia::Core::StringCRC& instanceId);

protected:
    Dia::ApplicationFlow::StartResult DoStart() override;
    void DoUpdate(float dt) override;
    Dia::ApplicationFlow::StopResult DoStop() override;
    void OnConnectStreams(Dia::ApplicationFlow::Application& app) override;

private:
    void CacheNavigableStages();
    void DrawMenu();
    const char* GetStatusLabel(const Dia::Core::StringCRC& stageCrc) const;
    bool IsLoadedThisSession(unsigned int index) const;
    void MarkLoaded(unsigned int index);

    Dia::ApplicationFlow::ModuleRef<DebugUIModule> mDebugUI{this, Dia::Core::StringCRC("DebugUI")};
    Dia::ApplicationFlow::ModuleRef<DebugServerHostModule> mDebugServer{this, Dia::Core::StringCRC("DebugServerHostModule")};
    Dia::ApplicationFlow::EventStreamWriter<Cluiche::AppFlow::RenderToSimNavRequest> mNavRequest{this, "BootMenuNavRequest"};

    static constexpr unsigned int kMaxStages = 32;
    Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, kMaxStages> mNavigableStages;
    unsigned int mLoadedBitfield = 0;
    int mSelectedIndex = -1;
    bool mFirstDraw = true;
    bool mFrameWasActive = false;

    Dia::Observation::Metric::Gauge* mStageCountGauge = nullptr;
    Dia::Observation::Metric::Counter* mLaunchCounter = nullptr;
};

} } // namespace Cluiche::AppFlow
