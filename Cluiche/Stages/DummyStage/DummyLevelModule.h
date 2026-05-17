#pragma once
#include <DiaApplicationFlow/Module.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaApplicationFlow/Streams/StreamWriter.h>
#include <DiaApplicationFlow/Streams/StreamReader.h>
#include <DiaApplicationFlow/Streams/EventStreamWriter.h>
#include <DiaApplicationFlow/ModuleRefV2.h>
#include <DiaGraphics/Frame/FrameData.h>
#include <DiaUI/UIDataBuffer.h>
#include "Types/UICommand.h"
#include "Modules/TimeServerModule.h"
#include "Modules/InputStreamModule.h"

namespace Cluiche { namespace AppFlow {

class DummyLevelModule : public Dia::ApplicationFlow::Module {
public:
    static const Dia::Core::StringCRC kTypeId;
    explicit DummyLevelModule(const Dia::Core::StringCRC& instanceId);

protected:
    Dia::ApplicationFlow::StartResult DoStart() override;
    void DoUpdate(float dt) override;
    Dia::ApplicationFlow::StopResult DoStop() override;
    void OnConnectStreams(Dia::ApplicationFlow::Application& app) override;

private:
    Dia::ApplicationFlow::StreamWriter<Dia::Graphics::FrameData>  mRenderOutput{this, "SimToRender"};
    Dia::ApplicationFlow::EventStreamWriter<UICommand>             mUIOutput{this, "SimToUI"};
    Dia::ApplicationFlow::StreamReader<Dia::UI::UIDataBuffer>      mUIInput{this, "UIToSim"};
    Dia::ApplicationFlow::ModuleRef<TimeServerModule>              mTimeServer{this};
    Dia::ApplicationFlow::ModuleRef<InputStreamModule>             mInput{this};

    Dia::Graphics::FrameData mFrame;
    bool  mLoadEntryLogged = false;
    float mSpriteX = 400.0f;
    float mSpriteY = 300.0f;
    float mScore   = 0.0f;
};

} } // namespace Cluiche::AppFlow
