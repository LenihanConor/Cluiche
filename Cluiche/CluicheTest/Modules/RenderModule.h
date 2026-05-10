#pragma once
#include <DiaApplicationFlow/Module.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaApplicationFlow/Streams/StreamReader.h>
#include <DiaGraphics/Frame/FrameData.h>

namespace Dia { namespace Graphics { class ICanvas; } }

namespace Cluiche { namespace AppFlow {

class RenderModule : public Dia::ApplicationFlow::Module {
public:
    static const Dia::Core::StringCRC kTypeId;
    explicit RenderModule(const Dia::Core::StringCRC& instanceId);

protected:
    Dia::ApplicationFlow::StartResult DoStart() override;
    void DoUpdate(float dt) override;
    Dia::ApplicationFlow::StopResult DoStop() override;
    void OnConnectStreams(Dia::ApplicationFlow::Application& app) override;

private:
    Dia::ApplicationFlow::StreamReader<Dia::Graphics::FrameData> mFrameInput{this, "SimToRender"};
    Dia::Graphics::ICanvas* mCanvas = nullptr;
    Dia::Graphics::FrameData mEmptyFrame;
};

} } // namespace Cluiche::AppFlow
