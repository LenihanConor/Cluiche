#pragma once
#include <DiaApplicationFlow/Module.h>
#include <DiaApplicationFlow/Streams/StreamWriter.h>
#include <DiaApplicationFlow/Streams/ServiceStreamReader.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaGraphics/Frame/FrameData.h>
#include <DiaAssetRuntime/Handlers/TextureHandler.h>

namespace CluicheTest {

class AssetRuntimeRendererModule : public Dia::ApplicationFlow::Module
{
public:
    static const Dia::Core::StringCRC kTypeId;
    explicit AssetRuntimeRendererModule(const Dia::Core::StringCRC& instanceId);

protected:
    Dia::ApplicationFlow::StartResult DoStart() override;
    void DoUpdate(float deltaTime) override;
    Dia::ApplicationFlow::StopResult DoStop() override;
    void OnConnectStreams(Dia::ApplicationFlow::Application& app) override;

private:
    Dia::ApplicationFlow::StreamWriter<Dia::Graphics::FrameData>                mRenderOutput{this, "SimToRender"};
    Dia::ApplicationFlow::ServiceStreamReader<Dia::AssetRuntime::TextureHandler> mTextureHandlerService{this, "KernelTextureHandler"};

    Dia::Graphics::FrameData mFrame;
};

} // namespace CluicheTest
