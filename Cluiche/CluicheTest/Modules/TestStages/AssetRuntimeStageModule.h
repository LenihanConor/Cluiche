#pragma once
#include <DiaApplicationFlow/Module.h>
#include <DiaApplicationFlow/ModuleRefV2.h>
#include <DiaApplicationFlow/Streams/StreamWriter.h>
#include <DiaApplicationFlow/Streams/ServiceStreamReader.h>
#include <DiaAssetRuntime/Handlers/TextureHandler.h>
#include <DiaGraphics/Frame/FrameData.h>
#include <DiaCore/CRC/StringCRC.h>
#include "Modules/AutomationModule.h"

namespace Dia { namespace Observation { namespace Metric { class Gauge; } } }

namespace CluicheTest {

class AssetRuntimeStageModule : public Dia::ApplicationFlow::Module
{
public:
    static const Dia::Core::StringCRC kTypeId;
    explicit AssetRuntimeStageModule(const Dia::Core::StringCRC& instanceId);

protected:
    Dia::ApplicationFlow::StartResult DoStart() override;
    void DoUpdate(float deltaTime) override;
    Dia::ApplicationFlow::StopResult DoStop() override;
    void OnConnectStreams(Dia::ApplicationFlow::Application& app) override;

private:
    void RegisterCheckpoints();
    void RenderLoadedTextures();

    Dia::ApplicationFlow::StreamWriter<Dia::Graphics::FrameData>                 mRenderOutput{this, "SimToRender"};
    Dia::ApplicationFlow::ServiceStreamReader<Dia::AssetRuntime::TextureHandler> mTextureHandlerService{this, "KernelTextureHandler"};
    Dia::ApplicationFlow::ModuleRef<Cluiche::AppFlow::AutomationModule>          mAutomation{this};

    struct LoadSnapshot {
        unsigned int loadedCount = 0;
        bool allSucceeded = false;
    };

    Dia::Graphics::FrameData mFrame;
    LoadSnapshot             mFirstEntrySnapshot;
    unsigned int             mEntryCount    = 0;
    unsigned int             mFrameCount    = 0;
    unsigned int             mLoadStartFrame = 0;
    bool                     mAllLoaded     = false;
    bool                     mCleanReload   = false;

    Dia::Observation::Metric::Gauge* mMetricLoadCount     = nullptr;
    Dia::Observation::Metric::Gauge* mMetricActiveHandles = nullptr;
    Dia::Observation::Metric::Gauge* mMetricLoadTimeMs    = nullptr;
};

} // namespace CluicheTest
