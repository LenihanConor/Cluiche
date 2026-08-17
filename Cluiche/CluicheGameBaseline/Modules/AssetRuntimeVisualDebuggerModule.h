#pragma once

#ifdef DIA_DEBUG

#include <DiaApplicationFlow/Module.h>
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaApplicationFlow/ModuleRefV2.h>
#include <DiaStreams/ServiceStreamReader.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaAssetRuntime/AssetRuntime.h>
#include <DiaAssetRuntimeVisualDebugger/AssetRuntimeDebugDomain.h>
#include <DiaVisualDebugger/DebugLayerManager.h>
#include "Modules/DebugUIModule.h"
#include <memory>

namespace Cluiche { namespace AppFlow {

class AssetRuntimeVisualDebuggerModule : public Dia::ApplicationFlow::Module
{
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kRender;
    static constexpr const char* kDescription = "Visual debugger layer for asset runtime state";
    explicit AssetRuntimeVisualDebuggerModule(const Dia::Core::StringCRC& instanceId);
    ~AssetRuntimeVisualDebuggerModule() override;

protected:
    Dia::ApplicationFlow::StartResult DoStart() override;
    void DoUpdate(float dt) override;
    Dia::ApplicationFlow::StopResult DoStop() override;
    void OnConnectStreams(Dia::ApplicationFlow::Application& app) override;

private:
    Dia::ApplicationFlow::ModuleRef<DebugUIModule> mDebugUI{this, Dia::Core::StringCRC("DebugUI")};
    Dia::ApplicationFlow::ServiceStreamReader<Dia::Debug::DebugLayerManager>    mLayerManagerStream{this, "DebugLayerManager"};
    Dia::ApplicationFlow::ServiceStreamReader<Dia::AssetRuntime::AssetRuntime>  mRuntimeStream{this, "AssetRuntimeService"};
    std::unique_ptr<Dia::AssetRuntime::AssetRuntimeDebugDomain> mDebugDomain;
};

} } // namespace Cluiche::AppFlow

#endif // DIA_DEBUG
