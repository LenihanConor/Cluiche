#pragma once
#include <DiaApplicationFlow/SimModule.h>
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/SimTime/SimTimeContext.h>
#include <DiaStreams/StreamWriter.h>
#include <DiaStreams/StreamReader.h>
#include <DiaGraphics/Frame/FrameData.h>
#include <DiaUI/UIDataBuffer.h>

namespace Cluiche { namespace AppFlow {

class LoadingScreenModule : public Dia::ApplicationFlow::SimModule {
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kSim;
    static constexpr const char* kDescription = "Boot-stage loading screen renderer";
    explicit LoadingScreenModule(const Dia::Core::StringCRC& instanceId);

protected:
    Dia::ApplicationFlow::StartResult DoStart()                              override;
    void                              DoUpdate(const Dia::SimTime::SimTimeContext& ctx) override;
    Dia::ApplicationFlow::StopResult  DoStop()                               override;
    void                              OnConnectStreams(Dia::ApplicationFlow::Application& app) override;

private:
    // Publishes the loading-screen geometry to SimScene. UI compositing is
    // owned by UICompositeModule (the sole SimToRender writer).
    Dia::ApplicationFlow::StreamWriter<Dia::Graphics::FrameData>  mRenderOutput{this, "SimScene"};
    Dia::Graphics::FrameData mLoadingFrame;
    float mElapsed = 0.0f;
};

} } // namespace Cluiche::AppFlow
