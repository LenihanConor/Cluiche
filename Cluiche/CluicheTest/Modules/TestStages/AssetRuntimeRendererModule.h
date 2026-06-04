#pragma once
#include <DiaApplicationFlow/Module.h>
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaStreams/StreamWriter.h>
#include <DiaStreams/ServiceStreamReader.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaGraphics/Frame/FrameData.h>
#include <DiaAssetRuntime/Handlers/TextureHandler.h>

namespace CluicheTest {

class AssetRuntimeRendererModule : public Dia::ApplicationFlow::Module
{
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kSim;
    static constexpr const char* kDescription = "Renders asset runtime texture previews to SimToRender";
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
