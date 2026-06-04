#pragma once
#include <DiaApplicationFlow/Module.h>
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaApplicationFlow/ModuleRefV2.h>
#include <DiaStreams/StreamReader.h>
#include <DiaCore/CRC/StringCRC.h>
#include "Modules/DebugUIModule.h"
#include "Types/MainToRenderFrame.h"

namespace CluicheTest {

class AssetRuntimeHUDModule : public Dia::ApplicationFlow::Module
{
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kRender;
    static constexpr const char* kDescription = "Asset runtime reload snapshot and metrics HUD";
    explicit AssetRuntimeHUDModule(const Dia::Core::StringCRC& instanceId);

protected:
    Dia::ApplicationFlow::StartResult DoStart() override;
    void DoUpdate(float deltaTime) override;
    Dia::ApplicationFlow::StopResult DoStop() override;
    void OnConnectStreams(Dia::ApplicationFlow::Application& app) override;

private:
    Dia::ApplicationFlow::ModuleRef<Cluiche::AppFlow::DebugUIModule>        mDebugUI{this, Dia::Core::StringCRC("DebugUI")};
    Dia::ApplicationFlow::StreamReader<Cluiche::AppFlow::MainToRenderFrame> mMainStateInput{this, "MainToRender"};
};

} // namespace CluicheTest
