#pragma once
#include <DiaApplicationFlow/Module.h>
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaStreams/StreamReader.h>
#include <DiaStreams/StreamWriter.h>
#include <DiaStreams/ServiceStreamReader.h>
#include <DiaGraphics/Frame/FrameData.h>
#include <DiaGraphics/Frame/RenderFence.h>
#include <DiaGraphics/Interface/ICanvas.h>
#include <DiaAssetRuntime/Handlers/TextureHandler.h>

namespace Cluiche { namespace AppFlow {

class RenderModule : public Dia::ApplicationFlow::Module {
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kRender;
    static constexpr const char* kDescription = "Main renderer — reads SimToRender, drives canvas";
    explicit RenderModule(const Dia::Core::StringCRC& instanceId);

protected:
    Dia::ApplicationFlow::StartResult DoStart() override;
    void DoUpdate(float dt) override;
    Dia::ApplicationFlow::StopResult DoStop() override;
    void OnConnectStreams(Dia::ApplicationFlow::Application& app) override;

private:
    Dia::ApplicationFlow::StreamReader<Dia::Graphics::FrameData>               mFrameInput{this, "SimToRender"};
    Dia::ApplicationFlow::StreamWriter<Dia::Graphics::RenderFence>             mFenceOutput{this, "RenderToSim"};
    Dia::ApplicationFlow::ServiceStreamReader<Dia::Graphics::ICanvas>          mCanvasService{this, "KernelCanvas"};
    Dia::ApplicationFlow::ServiceStreamReader<Dia::AssetRuntime::TextureHandler> mTextureHandlerService{this, "KernelTextureHandler"};
    Dia::Graphics::ICanvas*  mCanvas = nullptr;
    Dia::Graphics::FrameData mLastFrame;
    uint64_t mPresentCount = 0;
};

} } // namespace Cluiche::AppFlow
