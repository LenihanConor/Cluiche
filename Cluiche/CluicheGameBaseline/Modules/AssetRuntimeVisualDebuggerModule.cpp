#include "Modules/AssetRuntimeVisualDebuggerModule.h"

#ifdef DIA_DEBUG

#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaVisualDebugger/DebugLayerManager.h>
#include <DiaObservation/Log/DiaLog.h>

namespace Cluiche { namespace AppFlow {

const Dia::Core::StringCRC AssetRuntimeVisualDebuggerModule::kTypeId("AssetRuntimeVisualDebuggerModule");

AssetRuntimeVisualDebuggerModule::AssetRuntimeVisualDebuggerModule(const Dia::Core::StringCRC& instanceId)
    : Module(instanceId)
{}

AssetRuntimeVisualDebuggerModule::~AssetRuntimeVisualDebuggerModule() = default;

Dia::ApplicationFlow::StartResult AssetRuntimeVisualDebuggerModule::DoStart()
{
    if (!mDebugUI.Get())
        return Dia::ApplicationFlow::StartResult::kLoading;

    if (!mLayerManagerStream.IsAvailable())
        return Dia::ApplicationFlow::StartResult::kLoading;

    if (!mDebugDomain)
    {
        mDebugDomain = std::make_unique<Dia::AssetRuntime::AssetRuntimeDebugDomain>();
        mDebugDomain->Register(mLayerManagerStream.Get());
    }

    return Dia::ApplicationFlow::StartResult::kReady;
}

void AssetRuntimeVisualDebuggerModule::DoUpdate(float /*dt*/)
{
}

Dia::ApplicationFlow::StopResult AssetRuntimeVisualDebuggerModule::DoStop()
{
    if (mDebugDomain && mLayerManagerStream.IsAvailable())
        mDebugDomain->Unregister(mLayerManagerStream.Get());
    mDebugDomain.reset();
    return Dia::ApplicationFlow::StopResult::kDone;
}

void AssetRuntimeVisualDebuggerModule::OnConnectStreams(Dia::ApplicationFlow::Application& app)
{
    mLayerManagerStream.Connect(app);
}

} } // namespace Cluiche::AppFlow

namespace { using AssetRuntimeVisualDebuggerModule_ = Cluiche::AppFlow::AssetRuntimeVisualDebuggerModule; }
DIA_MODULE(AssetRuntimeVisualDebuggerModule_);
DIA_DESCRIBE(AssetRuntimeVisualDebuggerModule_::kTypeId, "Renders asset runtime debug overlays and live asset state visualization.");

#endif // DIA_DEBUG
