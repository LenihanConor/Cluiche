#pragma once
#include <DiaApplicationFlow/Module.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaStreams/StreamWriter.h>
#include <DiaStreams/StreamReader.h>
#include <DiaStreams/EventStreamWriter.h>
#include <DiaStreams/ServiceStreamReader.h>
#include <DiaApplicationFlow/ModuleRefV2.h>
#include <DiaGraphics/Frame/FrameData.h>
#include <DiaUI/UIDataBuffer.h>
#include <DiaAssetRuntime/Handlers/TextureHandler.h>
#include "Types/SimToMainEvent.h"
#include "Types/AssetLoadStatus.h"
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
    Dia::ApplicationFlow::StreamWriter<Dia::Graphics::FrameData>                          mRenderOutput{this, "SimToRender"};
    Dia::ApplicationFlow::EventStreamWriter<SimToMainEvent>                                mUIOutput{this, "SimToMain"};
    Dia::ApplicationFlow::StreamReader<Dia::UI::UIDataBuffer>                              mUIInput{this, "UIToSim"};
    Dia::ApplicationFlow::ServiceStreamReader<Dia::AssetRuntime::TextureHandler>           mTextureHandlerService{this, "KernelTextureHandler"};
    Dia::ApplicationFlow::ServiceStreamReader<AssetLoadStatus>                             mAssetLoadStatusStream{this, "AssetLoadStatus"};
    Dia::ApplicationFlow::ModuleRef<TimeServerModule>                                      mTimeServer{this};
    Dia::ApplicationFlow::ModuleRef<InputStreamModule>                                     mInput{this};

    Dia::Graphics::FrameData mFrame;
    bool  mLoadEntryLogged = false;
    float mSpriteX = 400.0f;
    float mSpriteY = 300.0f;
    float mScore   = 0.0f;
};

} } // namespace Cluiche::AppFlow
