#pragma once
#include <DiaApplicationFlow/Module.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaApplicationFlow/Streams/StreamWriter.h>
#include <DiaApplicationFlow/Streams/StreamReader.h>
#include <DiaGraphics/Frame/FrameData.h>
#include <DiaUI/UIDataBuffer.h>

namespace Cluiche { namespace AppFlow {

class LoadingScreenModule : public Dia::ApplicationFlow::Module {
public:
    static const Dia::Core::StringCRC kTypeId;
    explicit LoadingScreenModule(const Dia::Core::StringCRC& instanceId);

protected:
    Dia::ApplicationFlow::StartResult DoStart()                              override;
    void                              DoUpdate(float dt)                     override;
    Dia::ApplicationFlow::StopResult  DoStop()                               override;
    void                              OnConnectStreams(Dia::ApplicationFlow::Application& app) override;

private:
    Dia::ApplicationFlow::StreamWriter<Dia::Graphics::FrameData>  mRenderOutput{this, "SimToRender"};
    Dia::ApplicationFlow::StreamReader<Dia::UI::UIDataBuffer>     mUIInput{this, "UIToSim"};
    Dia::Graphics::FrameData mLoadingFrame;
    float mElapsed = 0.0f;
};

} } // namespace Cluiche::AppFlow
