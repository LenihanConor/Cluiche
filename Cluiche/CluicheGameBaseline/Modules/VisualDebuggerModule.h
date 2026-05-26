#pragma once

#ifdef DIA_DEBUG

#include <DiaApplicationFlow/Module.h>
#include <DiaApplicationFlow/Streams/StreamWriter.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaGraphics/Frame/FrameData.h>
#include <DiaVisualDebugger/DebugLayerManager.h>

namespace Cluiche { namespace AppFlow {

class VisualDebuggerModule : public Dia::ApplicationFlow::Module
{
public:
    static const Dia::Core::StringCRC kTypeId;
    explicit VisualDebuggerModule(const Dia::Core::StringCRC& instanceId);

    Dia::Debug::DebugLayerManager& GetLayerManager() { return mLayerManager; }
    static Dia::Debug::DebugLayerManager* GetStaticLayerManager() { return sLayerManager; }

protected:
    Dia::ApplicationFlow::StartResult DoStart() override;
    void DoUpdate(float dt) override;
    Dia::ApplicationFlow::StopResult DoStop() override;
    void OnConnectStreams(Dia::ApplicationFlow::Application& app) override;

private:
    Dia::Debug::DebugLayerManager mLayerManager;
    Dia::ApplicationFlow::StreamWriter<Dia::Graphics::FrameData> mRenderOutput{this, "SimToRender"};
    Dia::Graphics::FrameData mFrame;

    static Dia::Debug::DebugLayerManager* sLayerManager;
};

} } // namespace Cluiche::AppFlow

#endif // DIA_DEBUG
