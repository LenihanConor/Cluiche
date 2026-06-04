#pragma once
#include <DiaApplicationFlow/Module.h>
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaStreams/StreamWriter.h>
#include <DiaStreams/StreamReader.h>
#include <DiaGraphics/Frame/FrameData.h>
#include <DiaUI/UIDataBuffer.h>

namespace Cluiche { namespace AppFlow {

// Thin SimPU passthrough: reads the UIToSim pixel buffer produced by UIModule
// (MainPU) and writes it to SimToRender as a RequestDrawUI frame.
// Use this for any stage that needs the Ultralight UI overlay but does not
// have its own sim render module (e.g. UIUltralightTestStage).
// Unlike LoadingScreenModule, this module draws nothing of its own — no spinner.
class UICompositeModule : public Dia::ApplicationFlow::Module {
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kSim;
    static constexpr const char* kDescription = "Passthrough: composites UIToSim buffer onto SimToRender";
    explicit UICompositeModule(const Dia::Core::StringCRC& instanceId);

protected:
    Dia::ApplicationFlow::StartResult DoStart()  override;
    void                              DoUpdate(float dt) override;
    Dia::ApplicationFlow::StopResult  DoStop()   override;
    void OnConnectStreams(Dia::ApplicationFlow::Application& app) override;

private:
    Dia::ApplicationFlow::StreamWriter<Dia::Graphics::FrameData> mRenderOutput{this, "SimToRender"};
    Dia::ApplicationFlow::StreamReader<Dia::UI::UIDataBuffer>    mUIInput{this, "UIToSim"};
    Dia::Graphics::FrameData mFrame;
};

} } // namespace Cluiche::AppFlow
