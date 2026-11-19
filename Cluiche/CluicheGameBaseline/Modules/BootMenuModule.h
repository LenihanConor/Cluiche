#pragma once
#include <DiaApplicationFlow/RenderModule.h>
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaApplicationFlow/ModuleRefV2.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaObservation/Metric/MetricRegistry.h>
#include <DiaStreams/EventStreamWriter.h>
#include <DiaStreams/StreamReader.h>
#include "Modules/DebugUIModule.h"
#include "Modules/DebugServerHostModule.h"
#include "Modules/BootMenuLogic.h"
#include "Types/RenderToSimNavRequest.h"
#include "Types/StageCatalogueSnapshot.h"

#include <memory>

namespace Cluiche { namespace AppFlow {

// ImGui view over BootMenuLogic. Reads the StageCatalogueSnapshot FrameStream (MainPU -> RenderPU); falls back to the
// Boot transitions list when no snapshot arrives within kFallbackSeconds.
class BootMenuModule : public Dia::ApplicationFlow::RenderModule {
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kRender;
    static constexpr const char* kDescription = "Boot stage navigation menu over the stage catalogue (groups, filters, search, presets, result badges)";
    static constexpr float kFallbackSeconds = 2.0f;
    explicit BootMenuModule(const Dia::Core::StringCRC& instanceId);

protected:
    Dia::ApplicationFlow::StartResult DoStart() override;
    void DoUpdate(const Dia::SimTime::RenderTimeContext& ctx) override;
    Dia::ApplicationFlow::StopResult DoStop() override;
    void OnConnectStreams(Dia::ApplicationFlow::Application& app) override;
    void OnConfigure(const char* configJson) override;

private:
    void PollSnapshot(float frameDt);
    void UseFallback();
    void OnRebuilt(const char* why);
    void DrawMenu();
    void DrawFilterBar();
    void DrawGroup(unsigned int groupIndex);
    void Launch(const Dia::Core::StringCRC& stage, const Dia::Core::StringCRC& preset, StageGroup fromGroup);

    Dia::ApplicationFlow::ModuleRef<DebugUIModule> mDebugUI{this, Dia::Core::StringCRC("DebugUI")};
    Dia::ApplicationFlow::ModuleRef<DebugServerHostModule> mDebugServer{this, Dia::Core::StringCRC("DebugServerHostModule")};
    Dia::ApplicationFlow::EventStreamWriter<Cluiche::AppFlow::RenderToSimNavRequest> mNavRequest{this, "BootMenuNavRequest"};
    Dia::ApplicationFlow::StreamReader<StageCatalogueSnapshot> mSnapshotIn{this, Dia::Core::StringCRC("StageCatalogueSnapshot")};

    static constexpr unsigned int kMaxStages = 32;
    Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, kMaxStages> mNavigableStages;

    BootMenuLogic mLogic;   // ~90 KB: lives in the (heap-allocated) module, never on the stack
    char mTitle[128] = {};
    char mProject[128] = {};
    char mSearchBuf[BootMenuLogic::kMaxSearch] = {};

    Dia::Core::StringCRC mSelectedPreset;     // preset of the clicked row (empty for a stage row)
    StageGroup mSelectedGroup = StageGroup::kOther;

    float mWaitedSeconds = 0.0f;
    uint32_t mLastRevision = 0;
    bool mHaveRevision = false;
    bool mFallbackLogged = false;
    bool mFirstDraw = true;
    bool mFrameWasActive = false;

    Dia::Observation::Metric::Gauge* mStageCountGauge = nullptr;
    Dia::Observation::Metric::Counter* mLaunchCounter = nullptr;
};

} } // namespace Cluiche::AppFlow
