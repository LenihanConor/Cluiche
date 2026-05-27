#pragma once
#include <DiaApplicationFlow/Module.h>
#include <DiaApplicationFlow/Streams/StreamWriter.h>
#include <DiaCore/CRC/StringCRC.h>
#include "Types/MainToRenderFrame.h"

namespace Cluiche { namespace AppFlow {

// Collects HUD + automation state from MainPU singletons each tick and
// publishes it to the MainToRender FrameStream so RenderPU modules can
// display it without direct cross-PU static access.
// Override DoPopulateFrame() to inject app-specific frame data.
class MainStateProducerModule : public Dia::ApplicationFlow::Module
{
public:
    static const Dia::Core::StringCRC kTypeId;
    explicit MainStateProducerModule(const Dia::Core::StringCRC& instanceId);

protected:
    Dia::ApplicationFlow::StartResult DoStart() override;
    void DoUpdate(float dt) override;
    Dia::ApplicationFlow::StopResult DoStop() override;
    void OnConnectStreams(Dia::ApplicationFlow::Application& app) override;

    virtual void DoPopulateFrame(MainToRenderFrame& /*frame*/) {}

private:
    Dia::ApplicationFlow::StreamWriter<MainToRenderFrame> mFrameOutput{this, "MainToRender"};
    MainToRenderFrame mLastFrame;
};

} } // namespace Cluiche::AppFlow
