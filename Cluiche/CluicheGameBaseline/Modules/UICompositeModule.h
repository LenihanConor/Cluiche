#pragma once
#include <DiaApplicationFlow/SimModule.h>
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/SimTime/SimTimeContext.h>
#include <DiaStreams/StreamWriter.h>
#include <DiaStreams/StreamReader.h>
#include <DiaGraphics/Frame/FrameData.h>
#include <DiaUI/UIDataBuffer.h>

namespace Dia { namespace Observation { namespace Metric { class Counter; } } }

namespace Cluiche { namespace AppFlow {

// Universal SimPU compositor and SOLE writer of SimToRender.
//
// Scene/geometry producers (LoadingScreenModule, DummyLevelModule,
// AssetRuntimeRendererModule) publish to SimScene; VisualDebuggerModule
// publishes debug overlays to SimDebug; UIModule (MainPU) publishes the
// Ultralight pixel buffer to UIToSim. This module merges all three into a
// single FrameData and writes SimToRender exactly ONCE per tick.
//
// Having a single SimToRender writer is what fixes the cross-thread flashing:
// the RenderPU double-buffer always observes a complete frame (scene + debug +
// UI), never a partial one from a mid-tick write by a competing writer.
class UICompositeModule : public Dia::ApplicationFlow::SimModule {
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kSim;
    static constexpr const char* kDescription = "Sole SimToRender writer: merges SimScene + SimDebug + UIToSim into one frame";
    explicit UICompositeModule(const Dia::Core::StringCRC& instanceId);

protected:
    Dia::ApplicationFlow::StartResult DoStart()  override;
    void                              DoUpdate(const Dia::SimTime::SimTimeContext& ctx) override;
    Dia::ApplicationFlow::StopResult  DoStop()   override;
    void OnConnectStreams(Dia::ApplicationFlow::Application& app) override;

private:
    Dia::ApplicationFlow::StreamWriter<Dia::Graphics::FrameData> mRenderOutput{this, "SimToRender"};
    Dia::ApplicationFlow::StreamReader<Dia::Graphics::FrameData> mSceneInput{this, "SimScene"};
    Dia::ApplicationFlow::StreamReader<Dia::Graphics::FrameData> mDebugInput{this, "SimDebug"};
    Dia::ApplicationFlow::StreamReader<Dia::UI::UIDataBuffer>    mUIInput{this, "UIToSim"};
    Dia::Graphics::FrameData mFrame;

    Dia::Observation::Metric::Counter* mMetricUIFramesWritten = nullptr;
    Dia::Observation::Metric::Counter* mMetricEmptyFramesWritten = nullptr;
};

} } // namespace Cluiche::AppFlow
