#pragma once

#ifdef DIA_DEBUG

#include <DiaApplicationFlow/Module.h>
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaApplicationFlow/Streams/StreamWriter.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaGraphics/Frame/FrameData.h>
#include <DiaVisualDebugger/DebugLayerManager.h>

namespace Cluiche { namespace AppFlow {

class VisualDebuggerModule : public Dia::ApplicationFlow::Module
{
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kSim;
    static constexpr const char* kDescription = "Debug layer manager and SimToRender debug draw";
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
    Dia::Core::StringCRC mLastKnownStage;

    static Dia::Debug::DebugLayerManager* sLayerManager;
};

} } // namespace Cluiche::AppFlow

#endif // DIA_DEBUG
