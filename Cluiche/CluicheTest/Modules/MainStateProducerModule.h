#pragma once
#include <DiaApplicationFlow/Module.h>
#include <DiaApplicationFlow/Streams/StreamWriter.h>
#include <DiaCore/CRC/StringCRC.h>
#include "Types/MainToRenderFrame.h"

namespace CluicheTest {

// MainStateProducerModule (MainPU, stages: all)
// Collects HUD + automation state from MainPU singletons each tick and
// publishes it to the MainToRender FrameStream so RenderPU modules can
// display it without direct cross-PU static access.
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

private:
    Dia::ApplicationFlow::StreamWriter<Cluiche::AppFlow::MainToRenderFrame> mFrameOutput{this, "MainToRender"};
    Cluiche::AppFlow::MainToRenderFrame mLastFrame;
};

} // namespace CluicheTest
