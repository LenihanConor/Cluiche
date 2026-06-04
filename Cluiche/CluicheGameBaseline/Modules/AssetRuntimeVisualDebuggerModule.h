#pragma once

#ifdef DIA_DEBUG

#include <DiaApplicationFlow/Module.h>
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaApplicationFlow/ModuleRefV2.h>
#include <DiaStreams/ServiceStreamReader.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaAssetRuntimeVisualDebugger/DiaAssetRuntimeVisualDebugger.h>
#include <DiaVisualDebugger/DebugLayerManager.h>
#include "Modules/DebugUIModule.h"

namespace Cluiche { namespace AppFlow {

class AssetRuntimeVisualDebuggerModule : public Dia::ApplicationFlow::Module
{
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kRender;
    static constexpr const char* kDescription = "Visual debugger layer for asset runtime state";
    explicit AssetRuntimeVisualDebuggerModule(const Dia::Core::StringCRC& instanceId);

protected:
    Dia::ApplicationFlow::StartResult DoStart() override;
    void DoUpdate(float dt) override;
    Dia::ApplicationFlow::StopResult DoStop() override;
    void OnConnectStreams(Dia::ApplicationFlow::Application& app) override;

private:
    Dia::ApplicationFlow::ModuleRef<DebugUIModule> mDebugUI{this, Dia::Core::StringCRC("DebugUI")};
    Dia::ApplicationFlow::ServiceStreamReader<Dia::Debug::DebugLayerManager> mLayerManagerStream{this, "DebugLayerManager"};
    Dia::AssetRuntime::DiaAssetRuntimeVisualDebugger mDebugger;
    bool mRegistered = false;
};

} } // namespace Cluiche::AppFlow

#endif // DIA_DEBUG
