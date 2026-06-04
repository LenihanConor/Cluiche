#pragma once
#include <DiaApplicationFlow/Module.h>
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaApplicationFlow/ModuleRefV2.h>
#include <DiaStreams/StreamWriter.h>
#include <DiaCore/CRC/StringCRC.h>
#include "Types/MainToRenderFrame.h"
#include "Modules/AutomationModule.h"
#include "Modules/AssetServiceModule.h"

namespace Cluiche { namespace AppFlow {

// Collects HUD + automation state from MainPU each tick and publishes it
// to the MainToRender FrameStream so RenderPU modules can display it.
// Override DoPopulateFrame() to inject app-specific frame data.
class MainStateProducerModule : public Dia::ApplicationFlow::Module
{
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kMain;
    static constexpr const char* kDescription = "Populates MainToRender frame from game state";
    explicit MainStateProducerModule(const Dia::Core::StringCRC& instanceId);

protected:
    Dia::ApplicationFlow::StartResult DoStart() override;
    void DoUpdate(float dt) override;
    Dia::ApplicationFlow::StopResult DoStop() override;
    void OnConnectStreams(Dia::ApplicationFlow::Application& app) override;

    virtual void DoPopulateFrame(MainToRenderFrame& /*frame*/) {}

private:
    Dia::ApplicationFlow::ModuleRef<AutomationModule>   mAutomationRef{this};
    Dia::ApplicationFlow::ModuleRef<AssetServiceModule> mAssetServiceRef{this};
    Dia::ApplicationFlow::StreamWriter<MainToRenderFrame> mFrameOutput{this, "MainToRender"};
    MainToRenderFrame mLastFrame;
};

} } // namespace Cluiche::AppFlow
